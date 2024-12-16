/**
 * @file iso8825_strings.c
 * @brief ISO/IEC 8825 string helper functions
 *
 * Copyright 2024 Leon Lynch
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

#include "iso8825_strings.h"
#include "iso8825_ber.h"

#include <string.h>
#include <inttypes.h>
#include <stdio.h>

static int iso8825_asn1_boolean_get_string(
	const struct iso8825_tlv_t* tlv,
	char* str,
	size_t str_len
)
{
	if (!tlv || !tlv->value || !tlv->length) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (tlv->length != 1) {
		// ASN.1 Boolean consists of one octet
		// See ISO 8825-1:2021, 8.2.1
		return 1;
	}

	// See ISO 8825-1:2021, 8.2.2
	if (tlv->value[0]) {
		strncpy(str, "True", str_len - 1);
	} else {
		strncpy(str, "False", str_len - 1);
	}
	str[str_len - 1] = 0;

	return 0;
}

static int iso8825_asn1_integer_get_string(
	const struct iso8825_tlv_t* tlv,
	char* str,
	size_t str_len
)
{
	int64_t x;

	if (!tlv || !tlv->value || !tlv->length) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (tlv->length > 8) {
		// Not supported
		return -2;
	}

	// Extract value as 2s-complementin host endianness
	// See ISO 8825-1:2021, 8.3.3
	if ((tlv->value[0] & 0x80) == 0x80) {
		// If highest bit is set, set all leading bits for negative integer
		x = -1;
	} else {
		// If highest bit is unset, integerr is positive
		x = 0;
	}
	for (unsigned int i = 0; i < tlv->length; ++i) {
		x = (x << 8) | tlv->value[i];
	}

	snprintf(str, str_len, "%"PRId64, x);

	return 0;
}

static int iso8825_asn1_real_get_string(
	const struct iso8825_tlv_t* tlv,
	char* str,
	size_t str_len
)
{
	if (!tlv || !tlv->value) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (tlv->length == 0) {
		// Value is plus zero
		// See ISO 8825-1:2021, 8.5.2
		snprintf(str, str_len, "0");
		return 0;
	}
	if (tlv->length == 1) {
		// Special values
		// See ISO 8825-1:2021, 8.5.9
		switch (tlv->value[0]) {
			case 0x40: snprintf(str, str_len, "PLUS-INFINITY"); return 0;
			case 0x41: snprintf(str, str_len, "MINUS-INFINITY"); return 0;
			case 0x42: snprintf(str, str_len, "NOT-A-NUMBER"); return 0;
			case 0x43: snprintf(str, str_len, "-0"); return 0;
			default:
				// Unknown encoding
				return 1;
		}
	}

	// Identify encoding
	// See ISO 8825-1:2021, 8.5.6
	if ((tlv->value[0] & 0x80) == 0x80) {
		// Binary encoding
		// See ISO 8825-1:2021, 8.5.7
		int S;
		unsigned int B;
		unsigned int F;
		int E;
		unsigned int N_offset;
		unsigned int N = 0;
		int64_t M;

		// Sign
		// See ISO 8825-1:2021, 8.5.7.1
		if ((tlv->value[0] & 0x40) == 0x40) { // Bit 7
			S = -1;
		} else {
			S = 1;
		}

		// Base B
		// See ISO 8825-1:2021, 8.5.7.2
		switch (tlv->value[0] & 0x30) { // Bits 6 to 5
			case 0x00: B = 2; break;
			case 0x10: B = 8; break;
			case 0x20: B = 16; break;
			default:
				// Unknown base
				return 2;
		}

		// Binary scaling factor F
		// See ISO 8825-1:2021, 8.5.7.3
		F = (tlv->value[0] & 0x0C) >> 2; // Bits 4 to 3

		// Exponent
		// See ISO 8825-1:2021, 8.5.7.4
		if ((tlv->value[0] & 0x03) == 0x03) {
			// Not supported
			return 3;
		}
		// First octet + number of exponent octets
		N_offset = 1 + (tlv->value[0] & 0x03) + 1;
		if (tlv->length < N_offset + 1) {
			return -2;
		}
		E = (int8_t)tlv->value[1];
		for (unsigned int i = 2; i < N_offset; ++i) {
			E = (E << 8) | tlv->value[i];
		}

		// Binary number N
		// See ISO 8825-1:2021, 8.5.7.5
		for (unsigned int i = N_offset; i < tlv->length; ++i) {
			N = (N << 8) | tlv->value[i];
		}

		// Compute mantissa M
		// See ISO 8825-1:2021, 8.5.7
		M = (S * N) << F;

		snprintf(str, str_len, "0x%02"PRIX64" x %u^%d", M, B, E);
		return 0;
	}

	// Identify encoding
	// See ISO 8825-1:2021, 8.5.6
	if ((tlv->value[0] & 0xC0) == 0x00) {
		// Decimal encoding
		// See ISO 8825-1:2021, 8.5.8
		if (str_len < tlv->length) {
			// Insufficient space in string buffer; truncate instead of
			// providing incomplete real value
			str[0] = 0;
			return 0;
		}

		// Copy ISO 6093 string
		strncpy(str, (const char*)tlv->value + 1, tlv->length - 1);
		str[tlv->length - 1] = 0;
		return 0;
	}

	return 1;
}

static int iso8825_asn1_value_get_8bit_string(
	const struct iso8825_tlv_t* tlv,
	char* str,
	size_t str_len
)
{
	if (!tlv || !tlv->value) {
		return -1;
	}

	if (!str || !str_len) {
		// Caller didn't want the value string
		return 0;
	}

	if (str_len < tlv->length + 1) {
		// Insufficient space in string buffer; truncate instead of
		// providing incomplete string
		str[0] = 0;
		return 0;
	}

	// NOTE: This implementation intentionally does not distinguish between the
	// different 8-bit encodings supported by the various ASN.1 string types.
	strncpy(str, (const char*)tlv->value, tlv->length);
	str[tlv->length] = 0;
	return 0;
}

int iso8825_tlv_get_info(
	const struct iso8825_tlv_t* tlv,
	struct iso8825_tlv_info_t* info,
	char* value_str,
	size_t value_str_len
)
{
	if (!tlv || !info) {
		return -1;
	}

	memset(info, 0, sizeof(*info));
	if (value_str && value_str_len) {
		value_str[0] = 0; // Default to empty value string
	}

	if (tlv->tag > 0xFF) {
		// Multi-byte tag is proprietary
		return 1;
	}

	if (tlv->tag & ISO8825_BER_CLASS_MASK) {
		// Application, context-specific or private class tags are proprietary
		return 1;
	}

	if (tlv->flags & ISO8825_BER_TAG_NUMBER_MASK) {
		// The presence of flags not related to ISO 8825 is proprietary
		return 1;
	}

	// See ISO 8824-1:2021, 8.4
	switch (tlv->tag & ISO8825_BER_TAG_NUMBER_MASK) {
		case ASN1_BOOLEAN:
			info->tag_name = "ASN.1 Boolean";
			return iso8825_asn1_boolean_get_string(tlv, value_str, value_str_len);

		case ASN1_INTEGER:
			info->tag_name = "ASN.1 Integer";
			return iso8825_asn1_integer_get_string(tlv, value_str, value_str_len);

		case ASN1_BIT_STRING:
			info->tag_name = "ASN.1 Bit string";
			return 0;

		case ASN1_OCTET_STRING:
			info->tag_name = "ASN.1 Octet string";
			return 0;

		case ASN1_NULL:
			info->tag_name = "ASN.1 Null";
			return 0;

		case ASN1_OBJECT_IDENTIFIER:
			info->tag_name = "ASN.1 Object Identifier (OID)";
			return 0;

		case ASN1_OBJECT_DESCRIPTOR:
			info->tag_name = "ASN.1 Object descriptor";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_EXTERNAL:
			info->tag_name = "ASN.1 External";
			return 0;

		case ASN1_REAL:
			info->tag_name = "ASN.1 Real";
			return iso8825_asn1_real_get_string(tlv, value_str, value_str_len);

		case ASN1_ENUMERATED:
			info->tag_name = "ASN.1 Enumerated";
			// See ISO 8825-1:2021, 8.4
			return iso8825_asn1_integer_get_string(tlv, value_str, value_str_len);

		case ASN1_EMBEDDED_PDV:
			info->tag_name = "ASN.1 Embedded PDV";
			return 0;

		case ASN1_UTF8STRING:
			info->tag_name = "ASN.1 UTF-8 string";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_RELATIVE_OBJECT_IDENTIFIER:
			info->tag_name = "ASN.1 Relative object identifier";
			return 0;

		case ASN1_TIME:
			info->tag_name = "ASN.1 Time";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_SEQUENCE:
			info->tag_name = "ASN.1 Sequence";
			return 0;

		case ASN1_SET:
			info->tag_name = "ASN.1 Set";
			return 0;

		case ASN1_NUMERICSTRING:
			info->tag_name = "ASN.1 Numeric string";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_PRINTABLESTRING:
			info->tag_name = "ASN.1 Printable string";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_TELETEXSTRING:
			info->tag_name = "ASN.1 Teletex (T61) string";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_VIDEOTEXSTRING:
			info->tag_name = "ASN.1 Videotex string";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_IA5STRING:
			info->tag_name = "ASN.1 IA5 (ISO 646) string";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_UTCTIME:
			info->tag_name = "ASN.1 UTC time";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_GENERALIZEDTIME:
			info->tag_name = "ASN.1 Generalized time";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_GRAPHICSTRING:
			info->tag_name = "ASN.1 Graphic string";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_VISIBLESTRING:
			info->tag_name = "ASN.1 Visible (ISO 646) string";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_GENERALSTRING:
			info->tag_name = "ASN.1 General string";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_UNIVERSALSTRING:
			info->tag_name = "ASN.1 Universal string";
			// Uses 4-octet canonical form (UTF-32) of ISO 10646
			// See ISO 8825-1:2021, 8.23.7
			return 0;

		case ASN1_CHARACTERSTRING:
			info->tag_name = "ASN.1 Unrestricted character string";
			// Constructed type that provides the syntax identification as well
			// as the character string value(s)
			// See ISO 8824-1:2021, 44.5
			return 0;

		case ASN1_BMPSTRING:
			info->tag_name = "ASN.1 Basic Multilingual Plane (BMP) string";
			// Uses 2-octet canonical form (UTF-16) of ISO 10646
			// See ISO 8825-1:2021, 8.23.8
			return 0;

		case ASN1_DATE:
			info->tag_name = "ASN.1 Date";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_TIME_OF_DAY:
			info->tag_name = "ASN.1 Time-of-day";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_DATE_TIME:
			info->tag_name = "ASN.1 Date-Time";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_DURATION:
			info->tag_name = "ASN.1 Duration";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_OID_IRI:
			info->tag_name = "ASN.1 Object Identifier (OID) Internationalized Resource Identifier (IRI)";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		case ASN1_RELATIVE_OID_IRI:
			info->tag_name = "ASN.1 Relative Object Identifier (OID) Internationalized Resource Identifier (IRI)";
			return iso8825_asn1_value_get_8bit_string(tlv, value_str, value_str_len);

		default:
			return 1;
	}
}
