/**
 * @file emv_dol.c
 * @brief EMV Data Object List processing functions
 *
 * Copyright (c) 2021, 2024 Leon Lynch
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
#include "emv_tlv.h"

#include <string.h>

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

	// NOTE: According to EMV 4.4 Book 3, 5.4, a Data Object List (DOL) entry
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

int emv_dol_compute_data_length(const void* ptr, size_t len)
{
	int r;
	struct emv_dol_itr_t itr;
	struct emv_dol_entry_t entry;
	int total = 0;

	if (!ptr || !len) {
		return -1;
	}

	r = emv_dol_itr_init(ptr, len, &itr);
	if (r) {
		return -2;
	}

	while ((r = emv_dol_itr_next(&itr, &entry)) > 0) {
		total += entry.length;
	}
	if (r != 0) {
		return -3;
	}

	return total;
}

int emv_dol_build_data(
	const void* ptr,
	size_t len,
	const struct emv_tlv_list_t* source1,
	const struct emv_tlv_list_t* source2,
	void* data,
	size_t* data_len
)
{
	int r;
	struct emv_dol_itr_t itr;
	struct emv_dol_entry_t entry;
	void* data_ptr = data;

	if (!ptr || !len || !source1 || !data || !data_len || !*data_len) {
		return -1;
	}

	r = emv_dol_itr_init(ptr, len, &itr);
	if (r) {
		return -2;
	}

	while ((r = emv_dol_itr_next(&itr, &entry)) > 0) {
		const struct emv_tlv_t* tlv;

		if (*data_len < entry.length) {
			// Output data length too small
			return 1;
		}

		// Find TLV
		tlv = emv_tlv_list_find_const(source1, entry.tag);
		if (!tlv && source2) {
			tlv = emv_tlv_list_find_const(source2, entry.tag);
		}
		if (!tlv) {
			// If TLV is not found, zero data output
			// See EMV 4.4 Book 3, 5.4, step 2b
			memset(data_ptr, 0, entry.length);
			data_ptr += entry.length;
			*data_len -= entry.length;
			continue;
		}

		if (tlv->length == entry.length) {
			// TLV is found and length matches DOL entry
			memcpy(data_ptr, tlv->value, entry.length);
			data_ptr += entry.length;
			*data_len -= entry.length;
			continue;
		}

		if (tlv->length > entry.length) {
			// TLV is found and length is more than DOL entry, requiring truncation
			// See EMV 4.4 Book 3, 5.4, step 2c
			// TODO: Update for non-PDOL use, non-terminal fields and format CN
			unsigned int offset = 0;
			if (emv_tlv_is_terminal_format_n(tlv->tag)) {
				offset = tlv->length - entry.length;
			}
			memcpy(data_ptr, tlv->value + offset, entry.length);
			data_ptr += entry.length;
			*data_len -= entry.length;
			continue;
		}

		if (tlv->length < entry.length) {
			// TLV is found and length is less than DOL entry, requiring padding
			// See EMV 4.4 Book 3, 5.4, step 2d
			// TODO: Update for non-PDOL use, non-terminal fields and format CN
			unsigned int pad_len = entry.length - tlv->length;
			if (emv_tlv_is_terminal_format_n(tlv->tag)) {
				memset(data_ptr, 0, pad_len);
				memcpy(data_ptr + pad_len, tlv->value, tlv->length);
			} else {
				memcpy(data_ptr, tlv->value, tlv->length);
				memset(data_ptr + tlv->length, 0, pad_len);
			}
			data_ptr += entry.length;
			*data_len -= entry.length;
			continue;
		}

		// This should never happen
		return -3;
	}
	if (r != 0) {
		return -4;
	}

	*data_len = data_ptr - data;
	return 0;
}
