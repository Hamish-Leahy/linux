// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019 Microsoft Corporation
 *
 * Author: Lakshmi Ramasubramanian (nramas@linux.microsoft.com)
 *
 * File: ima_asymmetric_keys.c
 *       Defines an IMA hook to measure asymmetric keys on key
 *       create or update.
 */

#include <keys/asymmetric-type.h>
#include <linux/user_namespace.h>
#include <linux/ima.h>
#include "ima.h"

/**
 * ima_post_key_create_or_update - Measure asymmetric keys
 * @keyring: Keyring to which the key is linked
 * @key: Created or updated key
 * @payload: The data used to instantiate or update the key
 * @payload_len: The length of @payload
 * @flags: Key flags
 * @create: Flag indicating whether the key was created or updated
 *
 * Keys can only be measured, not appraised.
 * The payload data used to instantiate or update the key is measured.
 */
void ima_post_key_create_or_update(struct key *keyring, struct key *key,
				   const void *payload, size_t payload_len,
				   unsigned long flags, bool create)
{
	bool queued = false;

	/* Only asymmetric keys are handled by this hook. */
	if (key->type != &key_type_asymmetric) {
		pr_debug("Ignoring non-asymmetric key type\n");
		return;
	}

	/* Validate payload */
	if (!payload || payload_len == 0) {
		pr_err("Invalid payload for key measurement\n");
		return;
	}

	/* Check if the key should be queued for measurement */
	if (ima_should_queue_key()) {
		queued = ima_queue_key(keyring, payload, payload_len);
		if (queued) {
			pr_info("Key measurement queued for keyring: %s\n", keyring->description);
			return;
		}
	}

	/* Measure the key */
	pr_info("Measuring key for keyring: %s\n", keyring->description);
	process_buffer_measurement(&nop_mnt_idmap, NULL, payload, payload_len,
				   keyring->description, KEY_CHECK, 0,
				   keyring->description, false, NULL, 0);
}
