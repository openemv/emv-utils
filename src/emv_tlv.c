/**
 * @file emv_tlv.c
 * @brief EMV TLV structures and helper functions
 *
 * Copyright (c) 2021-2024 Leon Lynch
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

#include "emv_tlv.h"
#include "iso8825_ber.h"
#include "emv_tags.h"

#include <stdbool.h>
#include <stdlib.h> // for malloc() and free()
#include <string.h>
#include <assert.h>

// Helper functions
static inline bool emv_tlv_list_is_valid(const struct emv_tlv_list_t* list);
static struct emv_tlv_t* emv_tlv_alloc(unsigned int tag, unsigned int length, const uint8_t* value, uint8_t flags);

static inline bool emv_tlv_list_is_valid(const struct emv_tlv_list_t* list)
{
	if (!list) {
		return false;
	}

	if (list->front && !list->back) {
		return false;
	}

	if (!list->front && list->back) {
		return false;
	}

	return true;
}

static struct emv_tlv_t* emv_tlv_alloc(unsigned int tag, unsigned int length, const uint8_t* value, uint8_t flags)
{
	struct emv_tlv_t* tlv;

	tlv = malloc(sizeof(*tlv));
	if (!tlv) {
		return NULL;
	}

	tlv->tag = tag;
	tlv->length = length;
	tlv->value = malloc(length);
	if (!tlv->value) {
		free(tlv);
		return NULL;
	}
	memcpy(tlv->value, value, length);
	tlv->flags = flags;
	tlv->next = NULL;

	return tlv;
}

int emv_tlv_free(struct emv_tlv_t* tlv)
{
	if (!tlv) {
		return -1;
	}
	if (tlv->next) {
		// EMV TLV field is part of a list; unsafe to free
		return 1;
	}

	if (tlv->value) {
		free(tlv->value);
		tlv->value = NULL;
	}
	free(tlv);

	return 0;
}

bool emv_tlv_list_is_empty(const struct emv_tlv_list_t* list)
{
	if (!emv_tlv_list_is_valid(list)) {
		// Indicate that the list is empty to dissuade the caller from
		// attempting to access it
		return true;
	}

	return !list->front;
}

void emv_tlv_list_clear(struct emv_tlv_list_t* list)
{
	if (!emv_tlv_list_is_valid(list)) {
		list->front = NULL;
		list->back = NULL;
		return;
	}

	while (list->front) {
		struct emv_tlv_t* tlv;
		int r;
		int emv_tlv_is_safe_to_free __attribute__((unused));

		tlv = emv_tlv_list_pop(list);
		r = emv_tlv_free(tlv);

		emv_tlv_is_safe_to_free = r;
		assert(emv_tlv_is_safe_to_free == 0);
	}
	assert(list->front == NULL);
	assert(list->back == NULL);
}

int emv_tlv_list_push(
	struct emv_tlv_list_t* list,
	unsigned int tag,
	unsigned int length,
	const uint8_t* value,
	uint8_t flags
)
{
	struct emv_tlv_t* tlv;

	if (!emv_tlv_list_is_valid(list)) {
		return -1;
	}

	tlv = emv_tlv_alloc(tag, length, value, flags);
	if (!tlv) {
		return -2;
	}

	if (list->back) {
		list->back->next = tlv;
		list->back = tlv;
	} else {
		list->front = tlv;
		list->back = tlv;
	}

	return 0;
}

struct emv_tlv_t* emv_tlv_list_pop(struct emv_tlv_list_t* list)
{
	struct emv_tlv_t* tlv = NULL;

	if (!emv_tlv_list_is_valid(list)) {
		return NULL;
	}

	if (list->front) {
		tlv = list->front;
		list->front = tlv->next;
		if (!list->front) {
			list->back = NULL;
		}

		tlv->next = NULL;
	}

	return tlv;
}

struct emv_tlv_t* emv_tlv_list_find(struct emv_tlv_list_t* list, unsigned int tag)
{
	struct emv_tlv_t* tlv = NULL;

	if (!emv_tlv_list_is_valid(list)) {
		return NULL;
	}

	for (tlv = list->front; tlv != NULL; tlv = tlv->next) {
		if (tlv->tag == tag) {
			return tlv;
		}
	}

	return NULL;
}

bool emv_tlv_list_has_duplicate(const struct emv_tlv_list_t* list)
{
	if (!emv_tlv_list_is_valid(list)) {
		return NULL;
	}

	for (const struct emv_tlv_t* tlv = list->front; tlv != NULL; tlv = tlv->next) {
		for (const struct emv_tlv_t* tlv2 = tlv->next; tlv2 != NULL; tlv2 = tlv2->next) {
			if (tlv->tag == tlv2->tag) {
				return true;
			}
		}
	}

	return false;
}

int emv_tlv_list_append(struct emv_tlv_list_t* list, struct emv_tlv_list_t* other)
{
	if (!emv_tlv_list_is_valid(list)) {
		return -1;
	}

	if (!emv_tlv_list_is_valid(other)) {
		return -2;
	}

	if (list->back) {
		// If list not empty, attach list back to other front
		list->back->next = other->front;
	} else {
		// If list empty, attach list front to other front
		list->front = other->front;
	}
	if (other->back) {
		// If other not empty, other back becomes list back
		list->back = other->back;
	}
	other->front = NULL;
	other->back = NULL;

	return 0;
}

int emv_tlv_parse(const void* ptr, size_t len, struct emv_tlv_list_t* list)
{
	int r;
	struct iso8825_ber_itr_t itr;
	struct iso8825_tlv_t tlv;

	r = iso8825_ber_itr_init(ptr, len, &itr);
	if (r) {
		return -1;
	}

	while ((r = iso8825_ber_itr_next(&itr, &tlv)) > 0) {
		if (iso8825_ber_is_constructed(&tlv)) {
			// Recurse into constructed/template field but omit it from the list
			r = emv_tlv_parse(tlv.value, tlv.length, list);
			if (r) {
				return r;
			}
		} else {
			r = emv_tlv_list_push(list, tlv.tag, tlv.length, tlv.value, 0);
			if (r) {
				return -2;
			}
		}
	}

	if (r < 0) {
		// BER decoding error
		return 1;
	}

	return 0;
}

bool emv_tlv_is_terminal_format_n(unsigned int tag)
{
	// EMV tags with source 'Terminal' and format 'n'
	// See EMV 4.4 Book 3, Annex A1
	switch (tag) {
		case EMV_TAG_9A_TRANSACTION_DATE:
		case EMV_TAG_9C_TRANSACTION_TYPE:
		case EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE:
		case EMV_TAG_5F36_TRANSACTION_CURRENCY_EXPONENT:
		case EMV_TAG_5F57_ACCOUNT_TYPE:
		case EMV_TAG_9F01_ACQUIRER_IDENTIFIER:
		case EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC:
		case EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC:
		case EMV_TAG_9F15_MCC:
		case EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE:
		case EMV_TAG_9F21_TRANSACTION_TIME:
		case EMV_TAG_9F35_TERMINAL_TYPE:
		case EMV_TAG_9F39_POS_ENTRY_MODE:
		case EMV_TAG_9F3C_TRANSACTION_REFERENCE_CURRENCY:
		case EMV_TAG_9F3D_TRANSACTION_REFERENCE_CURRENCY_EXPONENT:
		case EMV_TAG_9F41_TRANSACTION_SEQUENCE_COUNTER:
			return true;

		default:
			return false;
	}
}

int emv_format_ans_to_non_control_str(
	const uint8_t* buf,
	size_t buf_len,
	char* str,
	size_t str_len
)
{
	if (!buf || !buf_len || !str || !str_len) {
		return -1;
	}

	while (buf_len) {
		if (str_len == 1) {
			// Ensure enough space for NULL termination
			break;
		}

		// Only copy non-control characters
		if ((*buf >= 0x20 && *buf <= 0x7E) || *buf >= 0xA0) {
			*str = *buf;

			// Advance output
			++str;
			--str_len;
		}

		++buf;
		--buf_len;
	}

	*str = 0; // NULL terminate

	return 0;
}

int emv_format_ans_to_alnum_space_str(
	const uint8_t* buf,
	size_t buf_len,
	char* str,
	size_t str_len
)
{
	if (!buf || !buf_len || !str || !str_len) {
		return -1;
	}

	while (buf_len) {
		if (str_len == 1) {
			// Ensure enough space for NULL termination
			break;
		}

		// Only copy alphanumeric and space characters
		if (
			(*buf >= 0x30 && *buf <= 0x39) || // 0-9
			(*buf >= 0x41 && *buf <= 0x5A) || // A-Z
			(*buf >= 0x61 && *buf <= 0x7A) || // a-z
			*buf == 0x20                      // Space
		) {
			*str = *buf;

			// Advance output
			++str;
			--str_len;
		}

		++buf;
		--buf_len;
	}

	*str = 0; // NULL terminate

	return 0;
}

int emv_format_b_to_str(
	const uint8_t* buf,
	size_t buf_len,
	char* str,
	size_t str_len
)
{
	if (!buf || !buf_len || !str || !str_len) {
		return -1;
	}

	for (size_t i = 0; i < buf_len * 2; ++i) {
		uint8_t digit;

		if (str_len == 1) {
			// Ensure enough space for NULL termination
			break;
		}

		if ((i & 0x01) == 0) {
			// Most significant nibble
			digit = buf[i >> 1] >> 4;
		} else {
			// Least significant nibble
			digit = buf[i >> 1] & 0x0F;
		}

		// Convert to ASCII digit
		if (digit < 0xA) {
			*str = '0' + digit;
		} else {
			*str = 'A' + (digit - 0xA);
		}

		++str;
		--str_len;
	}

	*str = 0; // NULL terminate

	return 0;
}

const uint8_t* emv_uint_to_format_n(uint32_t value, uint8_t* buf, size_t buf_len)
{
	size_t i;
	uint32_t divider;

	if (!buf || !buf_len) {
		return NULL;
	}

	// Pack digits, right justified
	i = 0;
	divider = 10; // Start with the least significant decimal digit
	while (buf_len) {
		uint8_t digit;

		// Extract digit and advance to next digit
		if (value) {
			digit = value % divider;
			value /= 10;
		} else {
			digit = 0;
		}

		if ((i & 0x1) == 0) { // i is even
			// Least significant nibble
			buf[buf_len - 1] = digit;
		} else { // i is odd
			// Most significant nibble
			buf[buf_len - 1] |= digit << 4;
			--buf_len;
		}

		++i;
	}

	return buf;
}

int emv_format_n_to_uint(const uint8_t* buf, size_t buf_len, uint32_t* value)
{
	if (!buf || !buf_len || !value) {
		return -1;
	}

	// Extract two decimal digits per byte
	*value = 0;
	for (unsigned int i = 0; i < buf_len; ++i) {
		uint8_t digit;

		// Extract most significant nibble
		digit = buf[i] >> 4;
		if (digit > 9) {
			// Invalid digit for EMV format "n"
			return 1;
		}
		// Shift decimal digit into x
		*value = (*value * 10) + digit;

		// Extract least significant nibble
		digit = buf[i] & 0xf;
		if (digit > 9) {
			// Invalid digit for EMV format "n"
			return 2;
		}
		// Shift decimal digit into x
		*value = (*value * 10) + digit;
	}

	return 0;
}

const uint8_t* emv_uint_to_format_b(uint32_t value, uint8_t* buf, size_t buf_len)
{
	if (!buf || !buf_len) {
		return NULL;
	}

	// Pack bytes, right justified
	while (buf_len) {
		buf[buf_len - 1] = value & 0xFF;
		value >>= 8;
		--buf_len;
	}

	return buf;
}

int emv_format_b_to_uint(const uint8_t* buf, size_t buf_len, uint32_t* value)
{
	if (!buf || !buf_len || !value) {
		return -1;
	}

	if (buf_len > 4) {
		// Not supported
		return -2;
	}

	// Extract value in host endianness
	*value = 0;
	for (unsigned int i = 0; i < buf_len; ++i) {
		*value = (*value << 8) | buf[i];
	}

	return 0;
}
