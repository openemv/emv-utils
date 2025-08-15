/**
 * @file print_helpers.c
 * @brief Helper functions for command line output
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

#include "print_helpers.h"

#include "iso7816.h"
#include "iso7816_compact_tlv.h"
#include "iso7816_strings.h"

#include "iso8825_ber.h"
#include "iso8825_strings.h"

#include "emv_tlv.h"
#include "emv_dol.h"
#include "emv_app.h"
#include "emv_debug.h"
#include "emv_strings.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static bool verbose_enabled = true;

void print_set_verbose(bool enabled)
{
	verbose_enabled = enabled;
}

void print_buf(const char* buf_name, const void* buf, size_t length)
{
	const uint8_t* ptr = buf;
	if (buf_name) {
		printf("%s: ", buf_name);
	}
	if (buf) {
		for (size_t i = 0; i < length; i++) {
			printf("%02X", ptr[i]);
		}
	} else {
		printf("(null)");
	}
	printf("\n");
}

void print_str_list(
	const char* str_list,
	const char* delim,
	const char* prefix,
	unsigned int depth,
	const char* bullet,
	const char* suffix
)
{
	const char* str;
	char* str_tmp = strdup(str_list);
	char* str_free = str_tmp;
	char* save_ptr = NULL;

	while ((str = strtok_r(str_tmp, delim, &save_ptr))) {
		// for strtok_r()
		if (str_tmp) {
			str_tmp = NULL;
		}

		for (unsigned int i = 0; i < depth; ++i) {
			printf("%s", prefix ? prefix : "");
		}

		printf("%s%s%s", bullet ? bullet : "", str, suffix ? suffix : "");
	}

	free(str_free);
}

void print_atr(const struct iso7816_atr_info_t* atr_info)
{
	char str[1024];

	print_buf("ATR", atr_info->atr, atr_info->atr_len);

	// Print ATR info
	printf("  TS  = 0x%02X: %s\n", atr_info->TS, iso7816_atr_TS_get_string(atr_info));
	printf("  T0  = 0x%02X: %s\n", atr_info->T0, iso7816_atr_T0_get_string(atr_info, str, sizeof(str)));
	for (size_t i = 1; i < 5; ++i) {
		if (atr_info->TA[i] ||
			atr_info->TB[i] ||
			atr_info->TC[i] ||
			atr_info->TD[i] ||
			i < 3
		) {
			printf("  ----\n");
		}

		// Print TAi
		if (atr_info->TA[i]) {
			printf("  TA%zu = 0x%02X: %s\n", i, *atr_info->TA[i],
				iso7816_atr_TAi_get_string(atr_info, i, str, sizeof(str))
			);
		} else if (i < 3) {
			printf("  TA%zu absent: %s\n", i,
				iso7816_atr_TAi_get_string(atr_info, i, str, sizeof(str))
			);
		}

		// Print TBi
		if (atr_info->TB[i]) {
			printf("  TB%zu = 0x%02X: %s\n", i, *atr_info->TB[i],
				iso7816_atr_TBi_get_string(atr_info, i, str, sizeof(str))
			);
		} else if (i < 3) {
			printf("  TB%zu absent: %s\n", i,
				iso7816_atr_TBi_get_string(atr_info, i, str, sizeof(str))
			);
		}

		// Print TCi
		if (atr_info->TC[i]) {
			printf("  TC%zu = 0x%02X: %s\n", i, *atr_info->TC[i],
				iso7816_atr_TCi_get_string(atr_info, i, str, sizeof(str))
			);
		} else if (i < 3) {
			printf("  TC%zu absent: %s\n", i,
				iso7816_atr_TCi_get_string(atr_info, i, str, sizeof(str))
			);
		}

		if (atr_info->TD[i]) {
			printf("  TD%zu = 0x%02X: %s\n", i, *atr_info->TD[i],
				iso7816_atr_TDi_get_string(atr_info, i, str, sizeof(str))
			);
		}
	}
	if (atr_info->K_count) {
		printf("  ----\n");
		print_atr_historical_bytes(atr_info);

		if (atr_info->status_indicator_bytes) {
			printf("  ----\n");

			printf("  LCS = %02X: %s\n",
				atr_info->status_indicator.LCS,
				iso7816_lcs_get_string(atr_info->status_indicator.LCS)
			);

			if (atr_info->status_indicator.SW1 ||
				atr_info->status_indicator.SW2
			) {
				printf("  SW  = %02X%02X: (%s)\n",
					atr_info->status_indicator.SW1,
					atr_info->status_indicator.SW2,
					iso7816_sw1sw2_get_string(
						atr_info->status_indicator.SW1,
						atr_info->status_indicator.SW2,
						str, sizeof(str)
					)
				);
			}
		}
	}

	printf("  ----\n");
	printf("  TCK = 0x%02X\n", atr_info->TCK);
}

void print_atr_historical_bytes(const struct iso7816_atr_info_t* atr_info)
{
	int r;
	struct iso7816_compact_tlv_itr_t itr;
	struct iso7816_compact_tlv_t tlv;
	char str[1024];

	printf("  T1  = 0x%02X: %s\n", atr_info->T1,
		iso7816_atr_T1_get_string(atr_info)
	);

	if (atr_info->T1 != ISO7816_ATR_T1_COMPACT_TLV_SI &&
		atr_info->T1 != ISO7816_ATR_T1_COMPACT_TLV
	) {
		// Unknown historical byte format
		print_buf("  Historical bytes", atr_info->historical_bytes, atr_info->historical_bytes_len);
		return;
	}

	r = iso7816_compact_tlv_itr_init(
		atr_info->historical_bytes,
		atr_info->historical_bytes_len,
		&itr
	);
	if (r) {
		printf("Failed to parse ATR historical bytes\n");
		return;
	}

	while ((r = iso7816_compact_tlv_itr_next(&itr, &tlv)) > 0) {
		printf("  %s (0x%X): [%u] ",
			iso7816_compact_tlv_tag_get_string(tlv.tag),
			tlv.tag,
			tlv.length
		);
		for (size_t i = 0; i < tlv.length; ++i) {
			printf("%s%02X", i ? " " : "", tlv.value[i]);
		}
		printf("\n");

		switch (tlv.tag) {
			case ISO7816_COMPACT_TLV_CARD_SERVICE_DATA:
				r = iso7816_card_service_data_get_string_list(tlv.value[0], str, sizeof(str));
				break;

			case ISO7816_COMPACT_TLV_CARD_CAPABILITIES:
				r = iso7816_card_capabilities_get_string_list(tlv.value, tlv.length, str, sizeof(str));
				break;

			default:
				r = -1;
		}

		if (r == 0) {
			print_str_list(str, "\n", "  ", 2, "- ", "\n");
		}
	}
	if (r) {
		printf("Failed to parse ATR historical bytes\n");
		return;
	}
}

void print_capdu(const void* c_apdu, size_t c_apdu_len)
{
	int r;
	const uint8_t* ptr = c_apdu;
	char str[1024];

	if (!c_apdu || !c_apdu_len) {
		printf("(null)\n");
		return;
	}

	for (size_t i = 0; i < c_apdu_len; i++) {
		printf("%02X", ptr[i]);
	}

	r = emv_capdu_get_string(
		c_apdu,
		c_apdu_len,
		str,
		sizeof(str)
	);
	if (r) {
		// Failed to parse C-APDU
		printf("\n");
		return;
	}

	printf(" (%s)\n", str);
}

void print_rapdu(const void* r_apdu, size_t r_apdu_len)
{
	const uint8_t* ptr = r_apdu;
	char str[1024];
	const char* s;

	if (!r_apdu || !r_apdu_len) {
		printf("(null)\n");
		return;
	}

	for (size_t i = 0; i < r_apdu_len; i++) {
		printf("%02X", ptr[i]);
	}

	if (r_apdu_len < 2) {
		// No status
		printf("\n");
		return;
	}

	s = iso7816_sw1sw2_get_string(
		ptr[r_apdu_len - 2],
		ptr[r_apdu_len - 1],
		str,
		sizeof(str)
	);
	if (!s || !s[0]) {
		// No string or empty string
		printf("\n");
		return;
	}

	printf(" (%s)\n", s);
}

void print_sw1sw2(uint8_t SW1, uint8_t SW2)
{
	char str[1024];
	const char* s;

	s = iso7816_sw1sw2_get_string(SW1, SW2, str, sizeof(str));
	if (!s) {
		printf("Failed to parse SW1-SW2 status bytes\n");
		return;
	}

	printf("SW1SW2: %02X%02X (%s)\n", SW1, SW2, s);
}

/**
 * Print BER data (internal)
 * @param ptr BER encoded data
 * @param len Length of BER encoded data in bytes
 * @param prefix Recursion prefix to print before every string
 * @param depth Depth of current recursion
 * @param ignore_padding Ignore invalid data if it is likely DES or AES padding
 * @return Number of bytes consumed. Less than zero for error.
 */
static int print_ber_buf_internal(
	const void* ptr,
	size_t len,
	const char* prefix,
	unsigned int depth,
	bool ignore_padding
)
{
	int r;
	size_t valid_bytes = 0;
	struct iso8825_ber_itr_t itr;
	struct iso8825_tlv_t tlv;
	char str[1024];

	r = iso8825_ber_itr_init(ptr, len, &itr);
	if (r) {
		printf("Failed to initialise BER iterator\n");
		return -1;
	}

	while ((r = iso8825_ber_itr_next(&itr, &tlv)) > 0) {

		for (unsigned int i = 0; i < depth; ++i) {
			printf("%s", prefix ? prefix : "");
		}

		printf("%02X : [%u]", tlv.tag, tlv.length);

		if (iso8825_ber_is_constructed(&tlv)) {
			// If the field is constructed, only consider the tag and length
			// to be valid until the value has been parsed
			valid_bytes += (r - tlv.length);

			printf("\n");
			r = print_ber_buf_internal(
				tlv.value,
				tlv.length,
				prefix,
				depth + 1,
				ignore_padding
			);
			if (r < 0) {
				// Return here instead of breaking out to avoid repeated
				// processing of the error by recursive callers
				return r;
			}
			valid_bytes += r;
			if (r < tlv.length) {
				// If only part of the constructed field was valid, return here
				// to avoid further processing of the data
				return valid_bytes;
			}

		} else {
			// If the field is not constructed, consider all of the bytes to
			// be valid BER encoded data
			valid_bytes += r;

			for (size_t i = 0; i < tlv.length; ++i) {
				printf(" %02X", tlv.value[i]);
			}

			if (iso8825_ber_is_string(&tlv)) {
				if (tlv.length < sizeof(str)) {
					// Print as-is and let the console figure out the encoding
					memcpy(str, tlv.value, tlv.length);
					str[tlv.length] = 0;
					printf(" \"%s\"", str);
				} else {
					// String too long
					printf(" \"...\"");
				}

			} else if (tlv.tag == ASN1_OBJECT_IDENTIFIER) {
				struct iso8825_oid_t oid;

				r = iso8825_ber_oid_decode(tlv.value, tlv.length, &oid);
				if (r) {
					// Failed to decode OID
					break;
				}

				printf(" {");
				for (unsigned int i = 0; i < oid.length; ++i) {
					printf("%s%u", i ? " ": "", oid.value[i]);
				}
				printf("}");
			}

			printf("\n");
		}
	}

	if (r < 0) {
		// Determine whether invalid data is padding
		if (ignore_padding &&
			valid_bytes < len &&
			(
				((len & 0x7) == 0 && len - valid_bytes < 8) ||
				((len & 0xF) == 0 && len - valid_bytes < 15)
			)
		) {
			for (unsigned int i = 0; i < depth; ++i) {
				printf("%s", prefix ? prefix : "");
			}

			printf("Padding : [%zu]", len - valid_bytes);
			for (size_t i = valid_bytes; i < len; ++i) {
				printf(" %02X", *((uint8_t*)ptr + i));
			}
			printf("\n");

			// If the remaining bytes appear to be padding, consider these
			// bytes to be valid
			valid_bytes = len;

		} else {
			printf("BER decoding error %d", r); // Caller to print newline
		}
	}

	return valid_bytes;
}

void print_ber_buf(
	const void* ptr,
	size_t len,
	const char* prefix,
	unsigned int depth,
	bool ignore_padding
)
{
	int r;

	r = print_ber_buf_internal(
		ptr,
		len,
		prefix,
		depth,
		ignore_padding
	);
	if (r < 0) {
		printf("BER decoding failed\n");
		return;
	}
	if (r < len) {
		printf(" at offset %d; remaining invalid data:", r);
		for (size_t i = r; i < len; ++i) {
			printf(" %02X", *((uint8_t*)ptr + i));
		}
		printf("\n");
	}
}

/**
 * Print BER value bytes and truncate if necessary (internal)
 * @param value BER value bytes
 * @param length Length of BER value bytes
 */
static void print_ber_value(const uint8_t* value, size_t length)
{
	if (verbose_enabled || length <= 16) {
		for (size_t i = 0; i < length; ++i) {
			printf(" %02X", value[i]);
		}
	} else {
		for (size_t i = 0; i < 8; ++i) {
			printf(" %02X", value[i]);
		}
		printf(" ...");
		for (size_t i = length - 8; i < length; ++i) {
			printf(" %02X", value[i]);
		}
	}
}

/**
 * Determine whether value string provided by @ref emv_tlv_get_info() is a
 * list of strings (internal)
 * @param str Value string provided by @ref emv_tlv_get_info()
 */
static bool str_is_list(const char* str)
{
	if (!str || !str[0]) {
		return false;
	}

	// If the last character is a newline, assume it's a string list
	return str[strlen(str) - 1] == '\n';
}

/**
 * Print EMV TLV data (internal)
 * @param ptr EMV TLV data
 * @param len Length of EMV TLV data in bytes
 * @param prefix Recursion prefix to print before every string
 * @param depth Depth of current recursion
 * @param ignore_padding Ignore invalid data if it is likely DES or AES padding
 * @return Number of bytes consumed. Less than zero for error.
 */
static int print_emv_buf_internal(
	const void* ptr,
	size_t len,
	const char* prefix,
	unsigned int depth,
	bool ignore_padding
)
{
	int r;
	size_t valid_bytes = 0;
	struct iso8825_ber_itr_t itr;
	struct iso8825_tlv_t tlv;

	r = iso8825_ber_itr_init(ptr, len, &itr);
	if (r) {
		printf("Failed to initialise BER iterator\n");
		return -1;
	}

	while ((r = iso8825_ber_itr_next(&itr, &tlv)) > 0) {

		struct emv_tlv_t emv_tlv;
		struct emv_tlv_info_t info;
		char value_str[2048];

		emv_tlv.ber = tlv;
		emv_tlv_get_info(&emv_tlv, &info, value_str, sizeof(value_str));

		for (unsigned int i = 0; i < depth; ++i) {
			printf("%s", prefix ? prefix : "");
		}

		if (iso8825_ber_is_constructed(&tlv) && value_str[0]) {
			// Assume that a constructed field with a value string is an object
			// of some kind
			printf("%02X | %s : [%u]", tlv.tag, value_str, tlv.length);
		} else if (info.tag_name) {
			printf("%02X | %s : [%u]", tlv.tag, info.tag_name, tlv.length);
		} else {
			printf("%02X : [%u]", tlv.tag, tlv.length);
		}

		if (iso8825_ber_is_constructed(&tlv)) {
			unsigned int nested_offset;
			unsigned int nested_bytes;

			// If the field is constructed, only consider the tag and length
			// to be valid until the value has been parsed
			valid_bytes += (r - tlv.length);

			// Attempt to decode field as ASN.1 object
			r = iso8825_ber_asn1_object_decode(&tlv, NULL);
			if (r <= 0) {
				// Not ASN.1 object
				nested_offset = 0;
			} else {
				// Continue parsing after OID for ASN.1 objects
				nested_offset = r;
			}
			valid_bytes += nested_offset;

			printf("\n");
			r = print_emv_buf_internal(
				tlv.value + nested_offset,
				tlv.length - nested_offset,
				prefix,
				depth + 1,
				ignore_padding
			);
			if (r < 0) {
				// Return here instead of breaking out to avoid repeated
				// processing of the error by recursive callers
				return r;
			}
			nested_bytes = r;
			valid_bytes += nested_bytes;
			if (nested_offset + nested_bytes < tlv.length) {
				// If only part of the constructed field was valid, return here
				// to avoid further processing of the data
				return valid_bytes;
			}

		} else {
			// If the field is not constructed, consider all of the bytes to
			// be valid BER encoded data
			valid_bytes += r;

			// Print value bytes
			print_ber_value(tlv.value, tlv.length);

			// If the value string is empty or the value string is a list, end this
			// line and continue on the next line. This implementation assumes that
			// Data Object List (DOL) fields or Tag List fields will allways have
			// an empty value string.
			if (!value_str[0] || str_is_list(value_str)) {
				printf("\n");

				if (str_is_list(value_str)) {
					print_str_list(value_str, "\n", prefix, depth + 1, "- ", "\n");
				}
				if (info.format == EMV_FORMAT_DOL) {
					print_emv_dol(tlv.value, tlv.length, prefix, depth + 1);
				}
				if (info.format == EMV_FORMAT_TAG_LIST) {
					print_emv_tag_list(tlv.value, tlv.length, prefix, depth + 1);
				}
			} else if (value_str[0]) {
				// Use quotes for strings and parentheses for everything else
				if (info.format == EMV_FORMAT_A ||
					info.format == EMV_FORMAT_AN ||
					info.format == EMV_FORMAT_ANS ||
					iso8825_ber_is_string(&tlv)
				) {
					printf(" \"%s\"\n", value_str);
				} else {
					printf(" (%s)\n", value_str);
				}
			}
		}
	}

	if (r < 0) {
		// Determine whether invalid data is padding
		if (ignore_padding &&
			valid_bytes < len &&
			(
				((len & 0x7) == 0 && len - valid_bytes < 8) ||
				((len & 0xF) == 0 && len - valid_bytes < 15)
			)
		) {
			for (unsigned int i = 0; i < depth; ++i) {
				printf("%s", prefix ? prefix : "");
			}

			printf("Padding : [%zu]", len - valid_bytes);
			for (size_t i = valid_bytes; i < len; ++i) {
				printf(" %02X", *((uint8_t*)ptr + i));
			}
			printf("\n");

			// If the remaining bytes appear to be padding, consider these
			// bytes to be valid
			valid_bytes = len;

		} else {
			printf("BER decoding error %d", r); // Caller to print newline
		}
	}

	return valid_bytes;
}

void print_emv_buf(
	const void* ptr,
	size_t len,
	const char* prefix,
	unsigned int depth,
	bool ignore_padding
)
{
	int r;

	r = print_emv_buf_internal(
		ptr,
		len,
		prefix,
		depth,
		ignore_padding
	);
	if (r < 0) {
		printf("BER decoding failed\n");
		return;
	}
	if (r < len) {
		printf(" at offset %d; remaining invalid data:", r);
		for (size_t i = r; i < len; ++i) {
			printf(" %02X", *((uint8_t*)ptr + i));
		}
		printf("\n");
	}
}

/**
 * Print EMV TLV field (internal)
 * @param tlv EMV TLV field
 * @param prefix Recursion prefix to print before every string
 * @param depth Depth of current recursion
 * @param ignore_padding Ignore invalid data if it is likely DES or AES padding
 */
static void print_emv_tlv_internal(
	const struct emv_tlv_t* tlv,
	const char* prefix,
	unsigned int depth,
	bool ignore_padding
)
{
	struct emv_tlv_info_t info;
	char value_str[2048];

	emv_tlv_get_info(tlv, &info, value_str, sizeof(value_str));

	for (unsigned int i = 0; i < depth; ++i) {
		printf("%s", prefix ? prefix : "");
	}

	if (iso8825_ber_is_constructed(&tlv->ber) && value_str[0]) {
		// Assume that a constructed field with a value string is an object
		// of some kind
		printf("%02X | %s : [%u]", tlv->tag, value_str, tlv->length);
	} else if (info.tag_name) {
		printf("%02X | %s : [%u]", tlv->tag, info.tag_name, tlv->length);
	} else {
		printf("%02X : [%u]", tlv->tag, tlv->length);
	}

	if (iso8825_ber_is_constructed(&tlv->ber)) {
		int r;
		unsigned int nested_offset;

		// Attempt to decode field as ASN.1 object
		r = iso8825_ber_asn1_object_decode(&tlv->ber, NULL);
		if (r <= 0) {
			// Not ASN.1 object
			nested_offset = 0;
		} else {
			// Continue parsing after OID for ASN.1 objects
			nested_offset = r;
		}

		printf("\n");
		print_emv_buf(
			tlv->value + nested_offset,
			tlv->length - nested_offset,
			prefix, depth + 1,
			ignore_padding
		);

	} else {
		// Print value bytes
		print_ber_value(tlv->value, tlv->length);

		// If the value string is empty or the value string is a list, end this
		// line and continue on the next line. This implementation assumes that
		// Data Object List (DOL) fields or Tag List fields will allways have
		// an empty value string.
		if (!value_str[0] || str_is_list(value_str)) {
			printf("\n");

			if (str_is_list(value_str)) {
				print_str_list(value_str, "\n", prefix, depth + 1, "- ", "\n");
			}
			if (info.format == EMV_FORMAT_DOL) {
				print_emv_dol(tlv->value, tlv->length, prefix, depth + 1);
			}
			if (info.format == EMV_FORMAT_TAG_LIST) {
				print_emv_tag_list(tlv->value, tlv->length, prefix, depth + 1);
			}
		} else if (value_str[0]) {
			// Use quotes for strings and parentheses for everything else
			if (info.format == EMV_FORMAT_A ||
				info.format == EMV_FORMAT_AN ||
				info.format == EMV_FORMAT_ANS ||
				iso8825_ber_is_string(&tlv->ber)
			) {
				printf(" \"%s\"\n", value_str);
			} else {
				printf(" (%s)\n", value_str);
			}
		}
	}
}

/**
 * Print EMV TLV list (internal)
 * @param list EMV TLV list object
 * @param prefix Recursion prefix to print before every string
 * @param depth Depth of current recursion
 * @param ignore_padding Ignore invalid data if it is likely DES or AES padding
 */
static void print_emv_tlv_list_internal(
	const struct emv_tlv_list_t* list,
	const char* prefix,
	unsigned int depth,
	bool ignore_padding
)
{
	const struct emv_tlv_t* tlv;

	for (tlv = list->front; tlv != NULL; tlv = tlv->next) {
		print_emv_tlv_internal(tlv, prefix, depth, ignore_padding);
	}
}

void print_emv_tlv(const struct emv_tlv_t* tlv)
{
	print_emv_tlv_internal(tlv, "  ", 0, false);
}

void print_emv_tlv_list(const struct emv_tlv_list_t* list)
{
	print_emv_tlv_list_internal(list, "  ", 1, false);
}

void print_emv_dol(const void* ptr, size_t len, const char* prefix, unsigned int depth)
{
	int r;
	struct emv_dol_itr_t itr;
	struct emv_dol_entry_t entry;

	for (unsigned int i = 0; i < depth; ++i) {
		printf("%s", prefix ? prefix : "");
	}
	printf("Data Object List:\n");
	++depth;

	r = emv_dol_itr_init(ptr, len, &itr);
	if (r) {
		printf("Failed to initialise DOL iterator\n");
		return;
	}

	while ((r = emv_dol_itr_next(&itr, &entry)) > 0) {
		struct emv_tlv_t emv_tlv;
		struct emv_tlv_info_t info;

		memset(&emv_tlv, 0, sizeof(emv_tlv));
		emv_tlv.tag = entry.tag;
		emv_tlv.length = entry.length;
		emv_tlv_get_info(&emv_tlv, &info, NULL, 0);

		for (unsigned int i = 0; i < depth; ++i) {
			printf("%s", prefix ? prefix : "");
		}

		if (info.tag_name) {
			printf("%02X | %s [%u]\n", entry.tag, info.tag_name, entry.length);
		} else {
			printf("%02X [%u]\n", entry.tag, entry.length);
		}
	}
}

void print_emv_tag_list(const void* ptr, size_t len, const char* prefix, unsigned int depth)
{
	int r;
	unsigned int tag;

	for (unsigned int i = 0; i < depth; ++i) {
		printf("%s", prefix ? prefix : "");
	}
	printf("Tag List:\n");
	++depth;

	while ((r = iso8825_ber_tag_decode(ptr, len, &tag)) > 0) {
		struct emv_tlv_t emv_tlv;
		struct emv_tlv_info_t info;

		memset(&emv_tlv, 0, sizeof(emv_tlv));
		emv_tlv.tag = tag;
		emv_tlv_get_info(&emv_tlv, &info, NULL, 0);

		for (unsigned int i = 0; i < depth; ++i) {
			printf("%s", prefix ? prefix : "");
		}

		if (info.tag_name) {
			printf("%02X | %s\n", tag, info.tag_name);
		} else {
			printf("%02X\n", tag);
		}

		// Advance
		ptr += r;
		len -= r;
	}
}

void print_emv_app(const struct emv_app_t* app)
{
	printf("Application: ");
	for (size_t i = 0; i < app->aid->length; ++i) {
		printf("%02X", app->aid->value[i]);
	}
	printf(", %s", app->display_name);
	if (app->priority) {
		printf(", Priority %u", app->priority);
	}
	if (app->confirmation_required) {
		printf(", Cardholder confirmation required");
	}
	printf("\n");
}

static void print_emv_debug_internal(
	enum emv_debug_type_t debug_type,
	const char* str,
	const void* buf,
	size_t buf_len
)
{
	switch (debug_type) {
		case EMV_DEBUG_TYPE_MSG:
			printf("%s\n", str);
			return;

		case EMV_DEBUG_TYPE_BER:
			print_buf(str, buf, buf_len);
			print_emv_buf(buf, buf_len, "  ", 1, false);
			return;

		case EMV_DEBUG_TYPE_TLV_LIST:
			printf("%s:\n", str);
			print_emv_tlv_list_internal(buf, "  ", 1, false);
			return;

		case EMV_DEBUG_TYPE_ATR:
			print_atr(buf);
			return;

		case EMV_DEBUG_TYPE_CAPDU:
			printf("%s: ", str);
			print_capdu(buf, buf_len);
			return;

		case EMV_DEBUG_TYPE_RAPDU:
			printf("%s: ", str);
			print_rapdu(buf, buf_len);
			return;

		default:
			print_buf(str, buf, buf_len);
			return;
	}
}

void print_emv_debug(
	unsigned int timestamp,
	enum emv_debug_source_t source,
	enum emv_debug_level_t level,
	enum emv_debug_type_t debug_type,
	const char* str,
	const void* buf,
	size_t buf_len
)
{
	const char* src_str;

	switch (source) {
		case EMV_DEBUG_SOURCE_TTL:
			src_str = "TTL";
			break;

		case EMV_DEBUG_SOURCE_TAL:
			src_str = "TAL";
			break;

		case EMV_DEBUG_SOURCE_ODA:
			src_str = "ODA";
			break;

		case EMV_DEBUG_SOURCE_EMV:
			src_str = "EMV";
			break;

		case EMV_DEBUG_SOURCE_APP:
			src_str = "APP";
			break;

		default:
			src_str = "???";
			break;
	}

	printf("[%s] ", src_str);
	print_emv_debug_internal(debug_type, str, buf, buf_len);
}

void print_emv_debug_verbose(
	unsigned int timestamp,
	enum emv_debug_source_t source,
	enum emv_debug_level_t level,
	enum emv_debug_type_t debug_type,
	const char* str,
	const void* buf,
	size_t buf_len
)
{
	const char* src_str;
	const char* level_str;

	switch (source) {
		case EMV_DEBUG_SOURCE_TTL:
			src_str = "TTL";
			break;

		case EMV_DEBUG_SOURCE_TAL:
			src_str = "TAL";
			break;

		case EMV_DEBUG_SOURCE_ODA:
			src_str = "ODA";
			break;

		case EMV_DEBUG_SOURCE_EMV:
			src_str = "EMV";
			break;

		case EMV_DEBUG_SOURCE_APP:
			src_str = "APP";
			break;

		default:
			src_str = "???";
			break;
	}

	switch (level) {
		case EMV_DEBUG_LEVEL_ERROR:
			level_str = "ERROR";
			break;

		case EMV_DEBUG_LEVEL_INFO:
			level_str = "INFO";
			break;

		case EMV_DEBUG_LEVEL_CARD:
			level_str = "CARD";
			break;

		case EMV_DEBUG_LEVEL_TRACE:
			level_str = "TRACE";
			break;

		default:
			level_str = "????";
			break;
	}

	printf("[%010u,%s,%s] ", timestamp, src_str, level_str);
	print_emv_debug_internal(debug_type, str, buf, buf_len);
}
