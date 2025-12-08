/**
 * @file emv_tlv.c
 * @brief EMV TLV structures and helper functions
 *
 * Copyright 2021-2025 Leon Lynch
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
#include "emv.h"
#include "emv_tags.h"

#include <stdbool.h>
#include <stdlib.h> // For malloc() and free()
#include <string.h>
#include <assert.h>

// Helper functions
static inline bool emv_tlv_list_is_valid(const struct emv_tlv_list_t* list);
static inline bool emv_tlv_sources_is_valid(const struct emv_tlv_sources_t* sources);
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

static inline bool emv_tlv_sources_is_valid(const struct emv_tlv_sources_t* sources)
{
	if (!sources || !sources->count) {
		return false;
	}
	if (sources->count > sizeof(sources->list) / sizeof(sources->list[0])) {
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
	if (tlv->length) {
		tlv->value = malloc(length);
		if (!tlv->value) {
			free(tlv);
			return NULL;
		}
		if (value) {
			memcpy(tlv->value, value, length);
		}
	} else {
		tlv->value = NULL;
	}

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

int emv_tlv_list_push_asn1_object(
	struct emv_tlv_list_t* list,
	const struct iso8825_oid_t* oid,
	unsigned int ber_length,
	const uint8_t* ber_bytes
) {
	int r;
	size_t encoded_oid_length;
	size_t max_length;
	uint8_t* value = NULL;

	if (!emv_tlv_list_is_valid(list)) {
		return -1;
	}

	if (!oid ||
		oid->length < 2 ||
		oid->length > sizeof(oid->value) / sizeof(oid->value[0])
	) {
		return -2;
	}


	if (ber_length && !ber_bytes) {
		return -3;
	}
	if (ber_length > 0xFFFF) {
		// ASN.1 object content should not be excessively large
		return -4;
	}

	// Assume a maximum of 5 octets per OID subidentifier
	encoded_oid_length = oid->length * 5;
	max_length = 1 + 1 + encoded_oid_length + ber_length;
	value = malloc(max_length);
	if (!value) {
		return -5;
	}

	// Encode OID
	value[0] = ASN1_OBJECT_IDENTIFIER;
	r = iso8825_ber_oid_encode(oid, value + 2, &encoded_oid_length);
	if (r) {
		r = -6;
		goto exit;
	}
	if (encoded_oid_length > 127) {
		r = -7;
		goto exit;
	}
	value[1] = encoded_oid_length;

	// Copy remaining BER encoded bytes without validation
	if (max_length - 2 - encoded_oid_length < ber_length) {
		r = -8;
		goto exit;
	}
	memcpy(value + 2 + encoded_oid_length, ber_bytes, ber_length);

	r = emv_tlv_list_push(
		list,
		ISO8825_BER_CONSTRUCTED | ASN1_SEQUENCE,
		2 + encoded_oid_length + ber_length,
		value,
		ISO8825_BER_CONSTRUCTED
	);
	if (r) {
		r = -9;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	if (value) {
		free(value);
		value = NULL;
	}

	return r;
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
		return false;
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
		// If the list is not empty, then attach the back of the list to the
		// front of the other list
		list->back->next = other->front;
	} else {
		// If the list is empty, then the front of the other list becomes the
		// front of the combined list
		list->front = other->front;
	}
	if (other->back) {
		// If the other list is not empty, then the back of the other list
		// becomes the back of the combined list
		list->back = other->back;
	}
	other->front = NULL;
	other->back = NULL;

	return 0;
}

int emv_tlv_sources_init_from_ctx(
	struct emv_tlv_sources_t* sources,
	const struct emv_ctx_t* ctx
)
{
	if (!sources || !ctx) {
		return -1;
	}

	if (!emv_tlv_list_is_valid(&ctx->params)) {
		return -2;
	}
	if (!emv_tlv_list_is_valid(&ctx->config)) {
		return -3;
	}
	if (!emv_tlv_list_is_valid(&ctx->terminal)) {
		return -4;
	}
	if (!emv_tlv_list_is_valid(&ctx->icc)) {
		return -5;
	}

	// Order data sources such that:
	// - Terminal data created during the current transaction takes precendence
	// - ICC data obtained from the current card should not be overridden by
	//   config or current transaction parameters
	// - Transaction parameters can override config
	sources->count = 4;
	sources->list[0] = &ctx->terminal;
	sources->list[1] = &ctx->icc;
	sources->list[2] = &ctx->params;
	sources->list[3] = &ctx->config;

	return 0;
}

const struct emv_tlv_t* emv_tlv_sources_find_const(
	const struct emv_tlv_sources_t* sources,
	unsigned int tag
)
{
	if (!emv_tlv_sources_is_valid(sources)) {
		return NULL;
	}

	for (unsigned int i = 0; i < sources->count; ++i) {
		const struct emv_tlv_t* tlv;

		tlv = emv_tlv_list_find_const(sources->list[i], tag);
		if (tlv) {
			return tlv;
		}
	}

	return NULL;
}

int emv_tlv_sources_itr_init(
	const struct emv_tlv_sources_t* sources,
	struct emv_tlv_sources_itr_t* itr
)
{
	if (!emv_tlv_sources_is_valid(sources)) {
		return -1;
	}
	if (!itr) {
		return -2;
	}

	memset(itr, 0, sizeof(*itr));
	itr->sources = sources;
	if (sources->count && sources->list[0]) {
		// Used as the starting field for emv_tlv_sources_itr_find_next_const()
		itr->tlv = sources->list[0]->front;
	}

	return 0;
}

const struct emv_tlv_t* emv_tlv_sources_itr_next_const(
	struct emv_tlv_sources_itr_t* itr
)
{
	const struct emv_tlv_sources_t* sources;
	const struct emv_tlv_t* tlv;

	if (!itr || !itr->sources) {
		return NULL;
	}
	if (!emv_tlv_sources_is_valid(itr->sources)) {
		return NULL;
	}
	sources = itr->sources;

	if (itr->tlv != NULL) {
		tlv = itr->tlv;

		// Remember the next TLV
		itr->tlv = itr->tlv->next;

		return tlv;
	}

	for (++itr->idx; itr->idx < sources->count; ++itr->idx) {
		const struct emv_tlv_t* tlv;

		if (emv_tlv_list_is_empty(sources->list[itr->idx])) {
			continue;
		}

		tlv = sources->list[itr->idx]->front;

		if (tlv) {
			// Remember the next TLV
			itr->tlv = tlv->next;
		}

		return tlv;
	}

	return NULL;
}

const struct emv_tlv_t* emv_tlv_sources_itr_find_next_const(
	struct emv_tlv_sources_itr_t* itr,
	unsigned int tag
)
{
	const struct emv_tlv_sources_t* sources;

	if (!itr || !itr->sources) {
		return NULL;
	}
	if (!emv_tlv_sources_is_valid(itr->sources)) {
		return NULL;
	}
	sources = itr->sources;

	// Iterate from the current TLV until the end of the current list
	while (itr->tlv != NULL) {
		const struct emv_tlv_t* tlv = itr->tlv;

		if (tlv->tag == tag) {
			// Remember the next TLV
			itr->tlv = itr->tlv->next;

			return tlv;
		}

		itr->tlv = itr->tlv->next;
	}

	// Otherwise iterate the remaining lists
	for (++itr->idx; itr->idx < sources->count; ++itr->idx) {
		const struct emv_tlv_t* tlv;

		tlv = emv_tlv_list_find_const(sources->list[itr->idx], tag);
		if (tlv) {
			// Remember the next TLV
			itr->tlv = tlv->next;

			return tlv;
		}
	}

	return NULL;
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

bool emv_tlv_is_format_n(unsigned int tag)
{
	// EMV tags with source 'Terminal' and format 'n'
	// See EMV 4.4 Book 3, Annex A1
	switch (tag) {
		case EMV_TAG_42_IIN:
		case EMV_TAG_9A_TRANSACTION_DATE:
		case EMV_TAG_9C_TRANSACTION_TYPE:
		case EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE:
		case EMV_TAG_5F25_APPLICATION_EFFECTIVE_DATE:
		case EMV_TAG_5F28_ISSUER_COUNTRY_CODE:
		case EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE:
		case EMV_TAG_5F30_SERVICE_CODE:
		case EMV_TAG_5F34_APPLICATION_PAN_SEQUENCE_NUMBER:
		case EMV_TAG_5F36_TRANSACTION_CURRENCY_EXPONENT:
		case EMV_TAG_5F57_ACCOUNT_TYPE:
		case EMV_TAG_9F01_ACQUIRER_IDENTIFIER:
		case EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC:
		case EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC:
		case EMV_TAG_9F0C_IINE:
		case EMV_TAG_9F11_ISSUER_CODE_TABLE_INDEX:
		case EMV_TAG_9F15_MCC:
		case EMV_TAG_9F19_TOKEN_REQUESTOR_ID:
		case EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE:
		case EMV_TAG_9F21_TRANSACTION_TIME:
		case EMV_TAG_9F25_LAST_4_DIGITS_OF_PAN:
		case EMV_TAG_9F35_TERMINAL_TYPE:
		case EMV_TAG_9F39_POS_ENTRY_MODE:
		case EMV_TAG_9F3B_APPLICATION_REFERENCE_CURRENCY:
		case EMV_TAG_9F3C_TRANSACTION_REFERENCE_CURRENCY:
		case EMV_TAG_9F3D_TRANSACTION_REFERENCE_CURRENCY_EXPONENT:
		case EMV_TAG_9F41_TRANSACTION_SEQUENCE_COUNTER:
		case EMV_TAG_9F42_APPLICATION_CURRENCY_CODE:
		case EMV_TAG_9F43_APPLICATION_REFERENCE_CURRENCY_EXPONENT:
		case EMV_TAG_9F44_APPLICATION_CURRENCY_EXPONENT:
			return true;

		default:
			return false;
	}
}

bool emv_tlv_is_format_cn(unsigned tag)
{
	// EMV tags with format 'cn'
	// See EMV 4.4 Book 3, Annex A1
	switch (tag) {
		case EMV_TAG_5A_APPLICATION_PAN:
		case EMV_TAG_9F20_TRACK2_DISCRETIONARY_DATA:
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
		// Shift decimal digit into output value
		*value = (*value * 10) + digit;

		// Extract least significant nibble
		digit = buf[i] & 0xf;
		if (digit > 9) {
			// Invalid digit for EMV format "n"
			return 2;
		}
		// Shift decimal digit into output value
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
