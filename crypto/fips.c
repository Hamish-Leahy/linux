// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * FIPS 200 support.
 *
 * Copyright (c) 2008 Neil Horman <nhorman@tuxdriver.com>
 */

#include <linux/export.h>
#include <linux/fips.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sysctl.h>
#include <linux/notifier.h>
#include <linux/string_choices.h>
#include <generated/utsrelease.h>
#include <linux/fs.h>
#include <linux/kobject.h>
#include <linux/string.h>

int fips_enabled;
EXPORT_SYMBOL_GPL(fips_enabled);

ATOMIC_NOTIFIER_HEAD(fips_fail_notif_chain);
EXPORT_SYMBOL_GPL(fips_fail_notif_chain);

/* Process kernel command-line parameter at boot time. fips=0 or fips=1 */
static int fips_enable(char *str)
{
	fips_enabled = !!simple_strtol(str, NULL, 0);
	pr_info("fips mode: %s\n", str_enabled_disabled(fips_enabled));
	return 1;
}

__setup("fips=", fips_enable);

#define FIPS_MODULE_NAME CONFIG_CRYPTO_FIPS_NAME
#ifdef CONFIG_CRYPTO_FIPS_CUSTOM_VERSION
#define FIPS_MODULE_VERSION CONFIG_CRYPTO_FIPS_VERSION
#else
#define FIPS_MODULE_VERSION UTS_RELEASE
#endif

static char fips_name[] = FIPS_MODULE_NAME;
static char fips_version[] = FIPS_MODULE_VERSION;

static const struct ctl_table crypto_sysctl_table[] = {
	{
		.procname	= "fips_enabled",
		.data		= &fips_enabled,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler	= proc_dointvec
	},
	{
		.procname	= "fips_name",
		.data		= &fips_name,
		.maxlen		= 64,
		.mode		= 0444,
		.proc_handler	= proc_dostring
	},
	{
		.procname	= "fips_version",
		.data		= &fips_version,
		.maxlen		= 64,
		.mode		= 0444,
		.proc_handler	= proc_dostring
	},
};

static struct ctl_table_header *crypto_sysctls;

static void crypto_proc_fips_init(void)
{
	crypto_sysctls = register_sysctl("crypto", crypto_sysctl_table);
}

static void crypto_proc_fips_exit(void)
{
	unregister_sysctl_table(crypto_sysctls);
}

void fips_fail_notify(void)
{
	if (fips_enabled)
		atomic_notifier_call_chain(&fips_fail_notif_chain, 0, NULL);
}
EXPORT_SYMBOL_GPL(fips_fail_notify);

static ssize_t fips_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", fips_enabled);
}

static ssize_t fips_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	int new_fips_enabled;

	if (sscanf(buf, "%d", &new_fips_enabled) != 1)
		return -EINVAL;

	if (new_fips_enabled != 0 && new_fips_enabled != 1)
		return -EINVAL;

	fips_enabled = new_fips_enabled;
	pr_info("FIPS mode toggled: %s\n", str_enabled_disabled(fips_enabled));
	return count;
}

static struct kobj_attribute fips_attribute = __ATTR(fips_enabled, 0664, fips_show, fips_store);

static int __init fips_init(void)
{
	int ret;

	crypto_proc_fips_init();

	// Create a sysfs entry for FIPS mode
	ret = sysfs_create_file(kernel_kobj, &fips_attribute.attr);
	if (ret) {
		pr_err("Failed to create sysfs entry for fips_enabled\n");
		return ret;
	}

	return 0;
}

static void __exit fips_exit(void)
{
	crypto_proc_fips_exit();

	// Remove the sysfs entry for FIPS mode
	sysfs_remove_file(kernel_kobj, &fips_attribute.attr);
}

subsys_initcall(fips_init);
module_exit(fips_exit);
