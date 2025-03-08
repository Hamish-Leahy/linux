// SPDX-License-Identifier: GPL-2.0-only
/*
 * EVM POSIX ACL Handling
 *
 * Copyright (C) 2011 IBM Corporation
 *
 * Author:
 * Mimi Zohar <zohar@us.ibm.com>
 */

#include <linux/xattr.h>
#include <linux/evm.h>
#include <linux/slab.h>
#include <linux/string.h>

/**
 * posix_xattr_acl - Check if the given xattr is a POSIX ACL
 * @xattr: The xattr name to check
 *
 * Returns 1 if the xattr is a POSIX ACL, 0 otherwise.
 */
int posix_xattr_acl(const char *xattr)
{
	if (strcmp(xattr, XATTR_NAME_POSIX_ACL_ACCESS) == 0 ||
	    strcmp(xattr, XATTR_NAME_POSIX_ACL_DEFAULT) == 0) {
		return 1;
	}
	return 0;
}

/**
 * evm_set_posix_acl - Set the POSIX ACL for a given inode
 * @inode: The inode to set the ACL on
 * @acl: The ACL to set
 *
 * Returns 0 on success, or a negative error code on failure.
 */
int evm_set_posix_acl(struct inode *inode, const struct posix_acl *acl)
{
	int ret;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (!acl) {
		// Remove existing ACL
		ret = vfs_setxattr(inode, XATTR_NAME_POSIX_ACL_ACCESS, NULL, 0, 0);
		if (ret < 0)
			return ret;
		ret = vfs_setxattr(inode, XATTR_NAME_POSIX_ACL_DEFAULT, NULL, 0, 0);
		return ret;
	}

	// Set the ACL
	ret = vfs_setxattr(inode, XATTR_NAME_POSIX_ACL_ACCESS, acl->a_entries,
			   acl->a_count * sizeof(struct posix_acl_entry), 0);
	if (ret < 0)
		return ret;

	ret = vfs_setxattr(inode, XATTR_NAME_POSIX_ACL_DEFAULT, acl->a_entries,
			   acl->a_count * sizeof(struct posix_acl_entry), 0);
	return ret;
}

/**
 * evm_get_posix_acl - Get the POSIX ACL for a given inode
 * @inode: The inode to get the ACL from
 * @acl_type: The type of ACL to get (access or default)
 * @acl: Pointer to store the retrieved ACL
 *
 * Returns 0 on success, or a negative error code on failure.
 */
int evm_get_posix_acl(struct inode *inode, int acl_type, struct posix_acl **acl)
{
	const char *name;
	ssize_t size;

	if (acl_type == ACL_TYPE_ACCESS)
		name = XATTR_NAME_POSIX_ACL_ACCESS;
	else if (acl_type == ACL_TYPE_DEFAULT)
		name = XATTR_NAME_POSIX_ACL_DEFAULT;
	else
		return -EINVAL;

	size = vfs_getxattr(inode, name, NULL, 0);
	if (size < 0)
		return size;

	*acl = kmalloc(sizeof(struct posix_acl) + size, GFP_KERNEL);
	if (!*acl)
		return -ENOMEM;

	size = vfs_getxattr(inode, name, (*acl)->a_entries, size);
	if (size < 0) {
		kfree(*acl);
		return size;
	}

	(*acl)->a_count = size / sizeof(struct posix_acl_entry);
	return 0;
}

/**
 * evm_remove_posix_acl - Remove the POSIX ACL for a given inode
 * @inode: The inode to remove the ACL from
 *
 * Returns 0 on success, or a negative error code on failure.
 */
int evm_remove_posix_acl(struct inode *inode)
{
	int ret;

	ret = vfs_setxattr(inode, XATTR_NAME_POSIX_ACL_ACCESS, NULL, 0, 0);
	if (ret < 0)
		return ret;

	ret = vfs_setxattr(inode, XATTR_NAME_POSIX_ACL_DEFAULT, NULL, 0, 0);
	return ret;
}
