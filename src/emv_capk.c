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

#include <stddef.h>
#include <stdint.h>
#include <string.h>

const struct emv_capk_t* emv_capk_lookup(const uint8_t* rid, uint8_t index)
{
	for (size_t i = 0; i < sizeof(capk_list) / sizeof(capk_list[0]); ++i) {
		if (index == capk_list[i].index &&
			memcmp(rid, capk_list[i].rid, EMV_CAPK_RID_LEN) == 0
		) {
			return &capk_list[i];
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
	const struct emv_capk_t* capk;

	if (!itr) {
		return NULL;
	}
	if (itr->idx >= sizeof(capk_list) / sizeof(capk_list[0])) {
		return NULL;
	}

	capk = &capk_list[itr->idx];
	itr->idx++;
	return capk;
}
