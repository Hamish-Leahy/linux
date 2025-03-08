// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Accessibility Driver Enhancements
 *
 * Copyright (c) 2025 Hamish Leahy (Hamish@hamishleahy.com)
 */

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/slab.h>

static int high_contrast_mode = 0;
static int screen_reader_enabled = 0;
static int keyboard_navigation_enabled = 0;

static ssize_t high_contrast_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	return sprintf(buf, "%d\n", high_contrast_mode);
}

static ssize_t high_contrast_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
	if (sscanf(buf, "%d", &high_contrast_mode) != 1)
		return -EINVAL;

	pr_info("High contrast mode toggled: %s\n", high_contrast_mode ? "enabled" : "disabled");
	return count;
}

static ssize_t screen_reader_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	return sprintf(buf, "%d\n", screen_reader_enabled);
}

static ssize_t screen_reader_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
	if (sscanf(buf, "%d", &screen_reader_enabled) != 1)
		return -EINVAL;

	pr_info("Screen reader toggled: %s\n", screen_reader_enabled ? "enabled" : "disabled");
	return count;
}

static ssize_t keyboard_navigation_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
	return sprintf(buf, "%d\n", keyboard_navigation_enabled);
}

static ssize_t keyboard_navigation_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count) {
	if (sscanf(buf, "%d", &keyboard_navigation_enabled) != 1)
		return -EINVAL;

	pr_info("Keyboard navigation toggled: %s\n", keyboard_navigation_enabled ? "enabled" : "disabled");
	return count;
}

static struct kobj_attribute high_contrast_attribute = __ATTR(high_contrast_mode, 0664, high_contrast_show, high_contrast_store);
static struct kobj_attribute screen_reader_attribute = __ATTR(screen_reader_enabled, 0664, screen_reader_show, screen_reader_store);
static struct kobj_attribute keyboard_navigation_attribute = __ATTR(keyboard_navigation_enabled, 0664, keyboard_navigation_show, keyboard_navigation_store);

static struct kobject *accessibility_kobj;

static int __init accessibility_init(void) {
	int ret;

	accessibility_kobj = kobject_create_and_add("accessibility", kernel_kobj);
	if (!accessibility_kobj)
		return -ENOMEM;

	ret = sysfs_create_file(accessibility_kobj, &high_contrast_attribute.attr);
	if (ret)
		goto error;

	ret = sysfs_create_file(accessibility_kobj, &screen_reader_attribute.attr);
	if (ret)
		goto error;

	ret = sysfs_create_file(accessibility_kobj, &keyboard_navigation_attribute.attr);
	if (ret)
		goto error;

	pr_info("Accessibility driver initialized\n");
	return 0;

error:
	kobject_put(accessibility_kobj);
	return ret;
}

static void __exit accessibility_exit(void) {
	kobject_put(accessibility_kobj);
	pr_info("Accessibility driver exited\n");
}

module_init(accessibility_init);
module_exit(accessibility_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Accessibility Driver Enhancements");
MODULE_AUTHOR("Your Name <your.email@example.com>");
