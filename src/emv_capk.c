/**
 * @file emv_capk.c
 * @brief EMV Certificate Authority Public Key (CAPK) helper functions
 *
 * Copyright 2025 Leon Lynch
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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

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
		if (memcmp(hash, capk->hash, SHA1_SIZE) != 0) {
			r = 42;
			break;
		}
	} while (0);
	crypto_sha1_free(&ctx);

	return r;
}

int emv_capk_init(void)
{
	int r;

	// Do not use the iterator here because it will skip over invalid CAPKs
	for (size_t i = 0; i < sizeof(capk_list) / sizeof(capk_list[0]); ++i) {
		r = emv_capk_validate(&capk_list[i]);
		if (r) {
			return r;
		}
	}

	return 0;
}

const struct emv_capk_t* emv_capk_lookup(const uint8_t* rid, uint8_t index)
{
	int r;

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
	return 0;
}

const struct emv_capk_t* emv_capk_itr_next(struct emv_capk_itr_t* itr)
{
	int r;
	const struct emv_capk_t* capk;

	if (!itr) {
		return NULL;
	}

	while (itr->idx < sizeof(capk_list) / sizeof(capk_list[0])) {
		capk = &capk_list[itr->idx];

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
