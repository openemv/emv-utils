/**
 * @file emv_dol.c
 * @brief EMV Data Object List processing functions
 *
 * Copyright (c) 2021 Leon Lynch
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

#include "emv_dol.h"
#include "iso8825_ber.h"

int emv_dol_decode(const void* ptr, size_t len, struct emv_dol_entry_t* entry)
{
	int r;
	const uint8_t* buf = ptr;
	size_t offset = 0;

	if (!ptr || !entry) {
		return -1;
	}
	if (!len) {
		// End of encoded data
		return 0;
	}

	// Ensure minimum encoded data length
	// 1 byte tag and 1 byte length
	if (len < 2) {
		return -2;
	}

	// NOTE: According to EMV Book 3, 5.4, a Data Object List (DOL) entry
	// consists of a BER encoded tag, followed by a one-byte length.

	// Decode tag octets
	r = iso8825_ber_tag_decode(ptr, len, &entry->tag);
	if (r <= 0) {
		// Error or end of encoded data
		return r;
	}
	offset += r;

	if (offset >= len) {
		// Not enough bytes remaining
		return -5;
	}

	// Length byte
	entry->length = buf[offset];
	++offset;

	return offset;
}

int emv_dol_itr_init(const void* ptr, size_t len, struct emv_dol_itr_t* itr)
{
	if (!ptr || !itr) {
		return -1;
	}

	itr->ptr = ptr;
	itr->len = len;

	return 0;
}

int emv_dol_itr_next(struct emv_dol_itr_t* itr, struct emv_dol_entry_t* entry)
{
	int r;

	if (!itr || !entry) {
		return -1;
	}

	r = emv_dol_decode(itr->ptr, itr->len, entry);
	if (r > 0) {
		// Advance iterator
		itr->ptr += r;
		itr->len -= r;
	}

	return r;
}
