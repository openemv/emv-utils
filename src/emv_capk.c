/**
 * @file emv_capk.c
 * @brief EMV Certificate Authority Public Key (CAPK) helper functions
 *
 * Copyright 2025-2026 Leon Lynch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "emv_capk.h"
#include "emv_capk_static_data.h"

#include "crypto_sha.h"
#include "crypto_mem.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static bool static_enabled = false;

struct emv_capk_list_entry_t {
	struct emv_capk_t capk;
	struct emv_capk_list_entry_t* next;
	uint8_t data[]; // RID | modulus | exponent | hash
};
static struct emv_capk_list_entry_t* dynamic_list = NULL;

static int emv_capk_validate(const struct emv_capk_t* capk)
{
	int r;
	crypto_sha1_ctx_t ctx;
	uint8_t hash[SHA1_SIZE];

	if (capk->hash_len != SHA1_SIZE) {
		return SHA1_SIZE;
	}

	do {
		r = crypto_sha1_init(&ctx);
		if (r) {
			break;
		}
		r = crypto_sha1_update(&ctx, capk->rid, EMV_CAPK_RID_LEN);
		if (r) {
			break;
		}
		r = crypto_sha1_update(&ctx, &capk->index, sizeof(capk->index));
		if (r) {
			break;
		}
		r = crypto_sha1_update(&ctx, capk->modulus, capk->modulus_len);
		if (r) {
			break;
		}
		r = crypto_sha1_update(&ctx, capk->exponent, capk->exponent_len);
		if (r) {
			break;
		}
		r = crypto_sha1_finish(&ctx, hash);
		if (r) {
			break;
		}
		if (crypto_memcmp_s(hash, capk->hash, SHA1_SIZE) != 0) {
			r = 42;
			break;
		}
	} while (0);
	crypto_sha1_free(&ctx);

	return r;
}

int emv_capk_load_static(void)
{
	int r;

	// Do not use the iterator here because it will skip over invalid CAPKs
	for (size_t i = 0; i < sizeof(capk_list) / sizeof(capk_list[0]); ++i) {
		r = emv_capk_validate(&capk_list[i]);
		if (r) {
			return r;
		}
	}

	static_enabled = true;
	return 0;
}

int emv_capk_add(const struct emv_capk_t* capk)
{
	int r;
	struct emv_capk_list_entry_t* entry;
	size_t data_len;
	uint8_t* ptr;

	if (!capk ||
		!capk->rid ||
		!capk->modulus || !capk->modulus_len ||
		!capk->exponent || !capk->exponent_len ||
		!capk->hash || !capk->hash_len
	) {
		return -1;
	}

	r = emv_capk_validate(capk);
	if (r) {
		return r;
	}

	data_len = EMV_CAPK_RID_LEN + capk->modulus_len + capk->exponent_len + capk->hash_len;
	entry = malloc(sizeof(*entry) + data_len);
	if (!entry) {
		return -2;
	}

	ptr = entry->data;

	entry->capk.rid = ptr;
	memcpy(ptr, capk->rid, EMV_CAPK_RID_LEN);
	ptr += EMV_CAPK_RID_LEN;

	entry->capk.modulus = ptr;
	entry->capk.modulus_len = capk->modulus_len;
	memcpy(ptr, capk->modulus, capk->modulus_len);
	ptr += capk->modulus_len;

	entry->capk.exponent = ptr;
	entry->capk.exponent_len = capk->exponent_len;
	memcpy(ptr, capk->exponent, capk->exponent_len);
	ptr += capk->exponent_len;

	entry->capk.hash = ptr;
	entry->capk.hash_len = capk->hash_len;
	memcpy(ptr, capk->hash, capk->hash_len);

	entry->capk.index = capk->index;
	entry->capk.hash_id = capk->hash_id;

	entry->next = dynamic_list;
	dynamic_list = entry;

	return 0;
}

void emv_capk_clear(void)
{
	while (dynamic_list) {
		struct emv_capk_list_entry_t* entry = dynamic_list;
		dynamic_list = entry->next;
		free(entry);
	}

	static_enabled = false;
}

const struct emv_capk_t* emv_capk_lookup(const uint8_t* rid, uint8_t index)
{
	int r;

	// Search dynamic CAPK list
	for (struct emv_capk_list_entry_t* entry = dynamic_list; entry != NULL; entry = entry->next) {
		if (index == entry->capk.index &&
			memcmp(rid, entry->capk.rid, EMV_CAPK_RID_LEN) == 0
		) {
			r = emv_capk_validate(&entry->capk);
			if (r) {
				continue;
			}
			return &entry->capk;
		}
	}

	if (!static_enabled) {
		return NULL;
	}

	// Search static CAPK list
	for (size_t i = 0; i < sizeof(capk_list) / sizeof(capk_list[0]); ++i) {
		if (index == capk_list[i].index &&
			memcmp(rid, capk_list[i].rid, EMV_CAPK_RID_LEN) == 0
		) {
			const struct emv_capk_t* capk = &capk_list[i];

			r = emv_capk_validate(capk);
			if (r) {
				return NULL;
			}

			return capk;
		}
	}

	return NULL;
}

int emv_capk_itr_init(struct emv_capk_itr_t* itr)
{
	if (!itr) {
		return -1;
	}

	memset(itr, 0, sizeof(*itr));
	itr->entry = dynamic_list;
	itr->idx = 0;
	return 0;
}

const struct emv_capk_t* emv_capk_itr_next(struct emv_capk_itr_t* itr)
{
	int r;

	if (!itr) {
		return NULL;
	}

	// Iterate dynamic CAPK list
	while (itr->entry) {
		const struct emv_capk_list_entry_t* entry = itr->entry;

		// Advance regardless of whether CAPK is valid
		itr->entry = entry->next;

		r = emv_capk_validate(&entry->capk);
		if (r) {
			continue;
		}

		return &entry->capk;
	}

	if (!static_enabled) {
		return NULL;
	}

	// Iterate static CAPK list
	while (itr->idx < sizeof(capk_list) / sizeof(capk_list[0])) {
		const struct emv_capk_t* capk = &capk_list[itr->idx];

		// Advance regardless of whether CAPK is valid
		itr->idx++;

		r = emv_capk_validate(capk);
		if (r) {
			continue;
		}

		return capk;
	}

	return NULL;
}
