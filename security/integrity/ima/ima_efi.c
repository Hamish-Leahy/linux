/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * EVM EFI Secure Boot Handling
 *
 * Copyright (C) 2018 IBM Corporation
 */
#include <linux/efi.h>
#include <linux/module.h>
#include <linux/ima.h>
#include <asm/efi.h>

#ifndef arch_ima_efi_boot_mode
#define arch_ima_efi_boot_mode efi_secureboot_mode_unset
#endif

/**
 * get_sb_mode - Retrieve the current secure boot mode
 *
 * Returns the current secure boot mode or efi_secureboot_mode_unknown
 * if EFI runtime services are not supported.
 */
static enum efi_secureboot_mode get_sb_mode(void)
{
	enum efi_secureboot_mode mode;

	if (!efi_rt_services_supported(EFI_RT_SUPPORTED_GET_VARIABLE)) {
		pr_info("ima: Secure boot mode unknown, EFI not supported\n");
		return efi_secureboot_mode_unknown;
	}

	mode = efi_get_secureboot_mode(efi.get_variable);
	switch (mode) {
		case efi_secureboot_mode_disabled:
			pr_info("ima: Secure boot mode disabled\n");
			break;
		case efi_secureboot_mode_unknown:
			pr_info("ima: Secure boot mode unknown\n");
			break;
		case efi_secureboot_mode_enabled:
			pr_info("ima: Secure boot mode enabled\n");
			break;
		default:
			pr_err("ima: Invalid secure boot mode\n");
			mode = efi_secureboot_mode_unknown;
			break;
	}
	return mode;
}

/**
 * arch_ima_get_secureboot - Check if secure boot is enabled
 *
 * Returns true if secure boot is enabled, false otherwise.
 */
bool arch_ima_get_secureboot(void)
{
	static enum efi_secureboot_mode sb_mode = efi_secureboot_mode_unset;
	static bool initialized = false;

	if (!initialized && efi_enabled(EFI_BOOT)) {
		sb_mode = arch_ima_efi_boot_mode;

		if (sb_mode == efi_secureboot_mode_unset) {
			sb_mode = get_sb_mode();
		}
		initialized = true;
	}

	return (sb_mode == efi_secureboot_mode_enabled);
}

/* Secure boot architecture rules */
static const char * const sb_arch_rules[] = {
#if !IS_ENABLED(CONFIG_KEXEC_SIG)
	"appraise func=KEXEC_KERNEL_CHECK appraise_type=imasig",
#endif /* CONFIG_KEXEC_SIG */
	"measure func=KEXEC_KERNEL_CHECK",
#if !IS_ENABLED(CONFIG_MODULE_SIG)
	"appraise func=MODULE_CHECK appraise_type=imasig",
#endif
#if IS_ENABLED(CONFIG_INTEGRITY_MACHINE_KEYRING) && IS_ENABLED(CONFIG_IMA_KEYRINGS_PERMIT_SIGNED_BY_BUILTIN_OR_SECONDARY)
	"appraise func=POLICY_CHECK appraise_type=imasig",
#endif
	"measure func=MODULE_CHECK",
	NULL
};

/**
 * arch_get_ima_policy - Retrieve the IMA policy based on secure boot status
 *
 * Returns a pointer to the IMA policy rules if secure boot is enabled,
 * NULL otherwise.
 */
const char * const *arch_get_ima_policy(void)
{
	if (IS_ENABLED(CONFIG_IMA_ARCH_POLICY) && arch_ima_get_secureboot()) {
		if (IS_ENABLED(CONFIG_MODULE_SIG))
			set_module_sig_enforced();
		if (IS_ENABLED(CONFIG_KEXEC_SIG))
			set_kexec_sig_enforced();
		return sb_arch_rules;
	}
	return NULL;
}
