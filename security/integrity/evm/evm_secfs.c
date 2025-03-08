// SPDX-License-Identifier: GPL-2.0-only
/*
 * EVM Security Filesystem Interface
 *
 * Copyright (C) 2010 IBM Corporation
 *
 * Authors:
 * Mimi Zohar <zohar@us.ibm.com>
 */

#include <linux/audit.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/security.h>
#include "evm.h"

static struct dentry *evm_dir;
static struct dentry *evm_init_tpm;
static struct dentry *evm_symlink;

#ifdef CONFIG_EVM_ADD_XATTRS
static struct dentry *evm_xattrs;
static DEFINE_MUTEX(xattr_list_mutex);
static int evm_xattrs_locked;
#endif

/**
 * evm_read_key - Read the EVM key status from the securityfs
 *
 * @filp: File pointer (not used)
 * @buf: Buffer to store the result
 * @count: Maximum bytes to read
 * @ppos: Position to start reading from
 *
 * Returns number of bytes read or error code.
 */
static ssize_t evm_read_key(struct file *filp, char __user *buf,
                             size_t count, loff_t *ppos)
{
    char temp[80];
    ssize_t rc;

    if (*ppos != 0)
        return 0;

    snprintf(temp, sizeof(temp), "%d", (evm_initialized & ~EVM_SETUP_COMPLETE));
    rc = simple_read_from_buffer(buf, count, ppos, temp, strlen(temp));

    return rc;
}

/**
 * evm_write_key - Write to the EVM key status in the securityfs
 *
 * @file: File pointer (not used)
 * @buf: Buffer containing the data to write
 * @count: Number of bytes to write
 * @ppos: Position to start writing to
 *
 * Returns number of bytes written or error code.
 */
static ssize_t evm_write_key(struct file *file, const char __user *buf,
                              size_t count, loff_t *ppos)
{
    unsigned int i;
    int ret;

    if (!capable(CAP_SYS_ADMIN) || (evm_initialized & EVM_SETUP_COMPLETE))
        return -EPERM;

    ret = kstrtouint_from_user(buf, count, 0, &i);
    if (ret)
        return ret;

    /* Reject invalid values */
    if (!i || (i & ~EVM_INIT_MASK) != 0)
        return -EINVAL;

    /* Prevent metadata writes if an HMAC key is loaded */
    if ((i & EVM_ALLOW_METADATA_WRITES) && (evm_initialized & EVM_INIT_HMAC))
        return -EPERM;

    if (i & EVM_INIT_HMAC) {
        ret = evm_init_key();
        if (ret != 0)
            return ret;
        i |= EVM_SETUP_COMPLETE; // Mark as complete
    }

    evm_initialized |= i;

    /* Prevent protected metadata modification if a symmetric key is loaded */
    if (evm_initialized & EVM_INIT_HMAC)
        evm_initialized &= ~(EVM_ALLOW_METADATA_WRITES);

    return count;
}

static const struct file_operations evm_key_ops = {
    .read = evm_read_key,
    .write = evm_write_key,
};

#ifdef CONFIG_EVM_ADD_XATTRS
/**
 * evm_read_xattrs - Read extended attributes from the securityfs
 *
 * @filp: File pointer (not used)
 * @buf: Buffer to store the result
 * @count: Maximum bytes to read
 * @ppos: Position to start reading from
 *
 * Returns number of bytes read or error code.
 */
static ssize_t evm_read_xattrs(struct file *filp, char __user *buf,
                                size_t count, loff_t *ppos)
{
    char *temp;
    ssize_t rc, size = 0;
    struct xattr_list *xattr;

    if (*ppos != 0)
        return 0;

    rc = mutex_lock_interruptible(&xattr_list_mutex);
    if (rc)
        return -ERESTARTSYS;

    list_for_each_entry(xattr, &evm_config_xattrnames, list) {
        if (xattr->enabled)
            size += strlen(xattr->name) + 1; // +1 for newline
    }

    temp = kmalloc(size + 1, GFP_KERNEL);
    if (!temp) {
        mutex_unlock(&xattr_list_mutex);
        return -ENOMEM;
    }

    size = 0; // Reset size for writing names
    list_for_each_entry(xattr, &evm_config_xattrnames, list) {
        if (xattr->enabled) {
            size += snprintf(temp + size, size + 1, "%s\n", xattr->name);
        }
    }

    mutex_unlock(&xattr_list_mutex);
    rc = simple_read_from_buffer(buf, count, ppos, temp, size);
    kfree(temp);

    return rc;
}

/**
 * evm_write_xattrs - Write extended attributes to the securityfs
 *
 * @file: File pointer (not used)
 * @buf: Buffer containing the data to write
 * @count: Number of bytes to write
 * @ppos: Position to start writing to
 *
 * Returns number of bytes written or error code.
 */
static ssize_t evm_write_xattrs(struct file *file, const char __user *buf,
                                 size_t count, loff_t *ppos)
{
    int err;
    struct xattr_list *xattr;
    struct audit_buffer *ab;

    if (!capable(CAP_SYS_ADMIN) || evm_xattrs_locked)
        return -EPERM;

    if (*ppos != 0 || count > XATTR_NAME_MAX)
        return -EINVAL;

    ab = audit_log_start(audit_context(), GFP_KERNEL, AUDIT_INTEGRITY_EVM_XATTR);
    if (!ab && IS_ENABLED(CONFIG_AUDIT))
        return -ENOMEM;

    xattr = kmalloc(sizeof(struct xattr_list), GFP_KERNEL);
    if (!xattr) {
        err = -ENOMEM;
        goto out;
    }

    xattr->enabled = true;
    xattr->name = memdup_user_nul(buf, count);
    if (IS_ERR(xattr->name)) {
        err = PTR_ERR(xattr->name);
        xattr->name = NULL;
        goto out;
    }

    /* Remove any trailing newline */
    if (xattr->name[strlen(xattr->name) - 1] == '\n')
        xattr->name[strlen(xattr->name) - 1] = '\0';

    audit_log_format(ab, "xattr=");
    audit_log_untrustedstring(ab, xattr->name);

    if (strcmp(xattr->name, ".") == 0) {
        evm_xattrs_locked = 1;
        struct iattr newattrs = {
            .ia_mode = S_IFREG | 0440,
            .ia_valid = ATTR_MODE,
        };
        struct inode *inode = evm_xattrs->d_inode;
        inode_lock(inode);
        err = simple_setattr(&nop_mnt_idmap, evm_xattrs, &newattrs);
        inode_unlock(inode);
        if (!err)
            err = count;
        goto out;
    }

    if (strncmp(xattr->name, XATTR_SECURITY_PREFIX, XATTR_SECURITY_PREFIX_LEN) != 0) {
        err = -EINVAL;
        goto out;
    }

    mutex_lock(&xattr_list_mutex);
    list_add_tail(&xattr->list, &evm_config_xattrnames);
    mutex_unlock(&xattr_list_mutex);

    audit_log_format(ab, " res=0");
    audit_log_end(ab);
    return count;

out:
    audit_log_format(ab, " res=%d", (err < 0) ? err : 0);
    audit_log_end(ab);
    if (xattr) {
        kfree(xattr->name);
        kfree(xattr);
    }
    return err;
}

static const struct file_operations evm_xattr_ops = {
    .read = evm_read_xattrs,
    .write = evm_write_xattrs,
};

static int evm_init_xattrs(void)
{
    evm_xattrs = securityfs_create_file("evm_xattrs", 0660, evm_dir, NULL, &evm_xattr_ops);
    return IS_ERR(evm_xattrs) ? -EFAULT : 0;
}
#else
static int evm_init_xattrs(void)
{
    return 0;
}
#endif

/**
 * evm_init_secfs - Initialize the EVM security filesystem
 *
 * Returns 0 on success, or a negative error code on failure.
 */
int __init evm_init_secfs(void)
{
    int error;

    evm_dir = securityfs_create_dir("evm", integrity_dir);
    if (IS_ERR(evm_dir))
        return -EFAULT;

    evm_init_tpm = securityfs_create_file("evm", 0660, evm_dir, NULL, &evm_key_ops);
    if (IS_ERR(evm_init_tpm)) {
        error = -EFAULT;
        goto out;
    }

    evm_symlink = securityfs_create_symlink("evm", NULL, "integrity/evm/evm", NULL);
    if (IS_ERR(evm_symlink)) {
        error = -EFAULT;
        goto out;
    }

    if (evm_init_xattrs() != 0) {
        error = -EFAULT;
        goto out;
    }

    return 0;

out:
    securityfs_remove(evm_symlink);
    securityfs_remove(evm_init_tpm);
    securityfs_remove(evm_dir);
    return error;
}
