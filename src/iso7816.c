/**
 * @file iso7816.c
 * @brief ISO/IEC 7816 definitions and helper functions
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

#include "iso7816.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// Helper functions
static void iso7816_atr_populate_default_parameters(struct iso7816_atr_info_t* atr_info);
static int iso7816_atr_parse_TA1(uint8_t TA1, struct iso7816_atr_info_t* atr_info);
static int iso7816_atr_parse_TB1(uint8_t TB1, struct iso7816_atr_info_t* atr_info);

int iso7816_atr_parse(const uint8_t* atr, size_t atr_len, struct iso7816_atr_info_t* atr_info)
{
	int r;
	size_t atr_idx;
	bool tck_mandatory = false;

	if (!atr) {
		return -1;
	}

	if (!atr_info) {
		return -1;
	}

	if (atr_len < ISO7816_ATR_MIN_SIZE || atr_len > ISO7816_ATR_MAX_SIZE) {
		// Invalid number of ATR bytes
		return 1;
	}

	memset(atr_info, 0, sizeof(*atr_info));
	iso7816_atr_populate_default_parameters(atr_info);

	// Copy ATR bytes
	memcpy(atr_info->atr, atr, atr_len);
	atr_info->atr_len = atr_len;

	// Parse initial byte TS
	atr_info->TS = atr[0];
	if (atr_info->TS != ISO7816_ATR_TS_DIRECT &&
		atr_info->TS != ISO7816_ATR_TS_INVERSE
	) {
		// Unknown encoding convention
		return 2;
	}

	// Parse format byte T0
	atr_info->T0 = atr[1];
	atr_info->K_count = (atr_info->T0 & ISO7816_ATR_Tx_OTHER_MASK);

	// Use T0 for for first set of interface bytes
	atr_idx = 1;

	for (size_t i = 1; i < 5; ++i) {
		uint8_t interface_byte_bits = atr_info->atr[atr_idx++]; // Y[i] value according to ISO7816
		uint8_t protocol = 0; // Default protocol is T=0 if unspecified

		// Parse available interface bytes
		if (interface_byte_bits & ISO7816_ATR_Tx_TAi_PRESENT) {
			atr_info->TA[i] = &atr_info->atr[atr_idx++];

			// Extract interface parameters from interface bytes TAi
			switch (i) {
				// Parse TA1
				case 1: r = iso7816_atr_parse_TA1(*atr_info->TA[i], atr_info); break;
			}
			if (r) {
				return r;
			}
		}
		if (interface_byte_bits & ISO7816_ATR_Tx_TBi_PRESENT) {
			atr_info->TB[i] = &atr_info->atr[atr_idx++];

			// Extract interface parameters from interface bytes TBi
			switch (i) {
				// Parse TB1
				case 1: r = iso7816_atr_parse_TB1(*atr_info->TB[i], atr_info); break;
			}
			if (r) {
				return r;
			}
		}
		if (interface_byte_bits & ISO7816_ATR_Tx_TCi_PRESENT) {
			atr_info->TC[i] = &atr_info->atr[atr_idx++];
		}
		if (interface_byte_bits & ISO7816_ATR_Tx_TDi_PRESENT) {
			atr_info->TD[i] = &atr_info->atr[atr_idx]; // preserve index for next loop iteration

			// If only T=0 is indicated, TCK is absent; otherwise it is mandatory
			protocol = *atr_info->TD[i] & ISO7816_ATR_Tx_OTHER_MASK; // T value according to ISO 7816-3:2006, 8.2.3
			if (protocol != ISO7816_ATR_Tx_PROTOCOL_T0 &&
				protocol != ISO7816_ATR_Tx_GLOBAL
			) {
				tck_mandatory = true;
			}
		} else {
			// No more interface bytes remaining
			break;
		}
	}

	if (atr_idx > atr_info->atr_len) {
		// Insufficient ATR bytes for interface bytes
		return 3;
	}
	if (atr_idx + atr_info->K_count > atr_info->atr_len) {
		// Insufficient ATR bytes for historical bytes
		return 4;
	}

	if (atr_info->K_count) {
		atr_info->T1 = atr_info->atr[atr_idx++];

		// Store pointer to historical payload for later parsing
		atr_info->historical_payload = &atr_info->atr[atr_idx];

		switch (atr_info->T1) {
			case ISO7816_ATR_T1_COMPACT_TLV_SI:
				atr_idx += atr_info->K_count - 1 - 3; // without T1 and status indicator

				// Store pointer to status indicator for later parsing
				atr_info->status_indicator_bytes = &atr_info->atr[atr_idx];
				atr_idx += 3;
				break;

			case ISO7816_ATR_T1_DIR_DATA_REF:
			case ISO7816_ATR_T1_COMPACT_TLV:
				atr_idx += atr_info->K_count - 1; // without T1
				break;

			default:
				// Proprietary historical bytes
				atr_idx += atr_info->K_count - 1; // without T1
				break;
		}
	}

	// Sanity check
	if (atr_idx > atr_info->atr_len) {
		// Internal parsing error
		return 5;
	}

	// Extract and verify TCK, if mandatory
	if (tck_mandatory) {
		if (atr_idx >= atr_info->atr_len) {
			// T=1 is available but TCK is missing
			return 6;
		}

		// Extract TCK
		atr_info->TCK = atr_info->atr[atr_idx++];

		// Verify XOR of T0 to TCK
		uint8_t verify = 0;
		for (size_t i = 1; i < atr_idx; ++i) {
			verify ^= atr_info->atr[i];
		}
		if (verify != 0) {
			// TCK is invalid
			return 7;
		}
	}

	// Extract status indicator, if available
	if (atr_info->status_indicator_bytes) {
		atr_info->status_indicator.LCS = atr_info->status_indicator_bytes[0];
		atr_info->status_indicator.SW1 = atr_info->status_indicator_bytes[1];
		atr_info->status_indicator.SW2 = atr_info->status_indicator_bytes[2];
	}

	return 0;
}

static void iso7816_atr_populate_default_parameters(struct iso7816_atr_info_t* atr_info)
{
	// ISO7816 part 3 indicates these default parameters:
	// - Fmax = 5MHz (from default F parameters)
	// - Fi/Di = 372/1 (from default F and D parameters)
	// - Ipp = 50mV (from default I parameter)
	// - Vpp = 5V (from default P parameter)

	// TA1 default
	iso7816_atr_parse_TA1(0x11, atr_info);

	// TB1 default
	iso7816_atr_parse_TB1(0x25, atr_info);
}

static int iso7816_atr_parse_TA1(uint8_t TA1, struct iso7816_atr_info_t* atr_info)
{
	uint8_t DI = (TA1 & ISO7816_ATR_TA1_DI_MASK);
	uint8_t FI = (TA1 & ISO7816_ATR_TA1_FI_MASK);

	// Decode bit rate adjustment factor Di according to ISO 7816-3:2006, 8.3, table 8
	switch (DI) {
		case 0x01: atr_info->global.Di = 1; break;
		case 0x02: atr_info->global.Di = 2; break;
		case 0x03: atr_info->global.Di = 4; break;
		case 0x04: atr_info->global.Di = 8; break;
		case 0x05: atr_info->global.Di = 16; break;
		case 0x06: atr_info->global.Di = 32; break;
		case 0x07: atr_info->global.Di = 64; break;
		case 0x08: atr_info->global.Di = 12; break;
		case 0x09: atr_info->global.Di = 20; break;

		default:
			return 10;
	}

	// Clock rate conversion factor Fi and maximum clock frequency fmax according to ISO 7816-3:2006, 8.3, table 7
	switch (FI) {
		case 0x00: atr_info->global.Fi = 372; atr_info->global.fmax = 4; break;
		case 0x10: atr_info->global.Fi = 372; atr_info->global.fmax = 5; break;
		case 0x20: atr_info->global.Fi = 558; atr_info->global.fmax = 6; break;
		case 0x30: atr_info->global.Fi = 744; atr_info->global.fmax = 8; break;
		case 0x40: atr_info->global.Fi = 1116; atr_info->global.fmax = 12; break;
		case 0x50: atr_info->global.Fi = 1488; atr_info->global.fmax = 16; break;
		case 0x60: atr_info->global.Fi = 1860; atr_info->global.fmax = 20; break;
		case 0x90: atr_info->global.Fi = 512; atr_info->global.fmax = 5; break;
		case 0xA0: atr_info->global.Fi = 768; atr_info->global.fmax = 7.5f; break;
		case 0xB0: atr_info->global.Fi = 1024; atr_info->global.fmax = 10; break;
		case 0xC0: atr_info->global.Fi = 1536; atr_info->global.fmax = 15; break;
		case 0xD0: atr_info->global.Fi = 2048; atr_info->global.fmax = 20; break;

		default:
			return 11;
	}

	return 0;
}

static int iso7816_atr_parse_TB1(uint8_t TB1, struct iso7816_atr_info_t* atr_info)
{
	uint8_t PI1 = (TB1 & ISO7816_ATR_TB1_PI1_MASK);
	uint8_t II = (TB1 & ISO7816_ATR_TB1_II_MASK);

	// TB1 == 0x00 indicates that Vpp is not connected to C6
	if (TB1 == 0x00) {
		atr_info->global.Vpp_connected = false;

		// No need to parse PI1 and II
		return 0;
	} else {
		atr_info->global.Vpp_connected = true;
	}

	// Programming voltage for active state according to ISO 7816-3:1997; deprecated in ISO 7816-3:2006
	if (PI1 < 5 || PI1 > 25) {
		// PI1 is only valid for values 5 to 25
		return 12;
	}
	// Vpp is in milliVolt and PI1 is in Volt
	atr_info->global.Vpp = PI1 * 1000;

	// Maximum programming current according to ISO 7816-3:1997; deprecated in ISO 7816-3:2006
	switch (II) {
		case 0x00: atr_info->global.Ipp = 25; break;
		case 0x20: atr_info->global.Ipp = 50; break;
		case 0x40: atr_info->global.Ipp = 100; break;
		default:
			return 13;
	}

	return 0;
}

const char* iso7816_atr_TS_get_string(const struct iso7816_atr_info_t* atr_info)
{
	if (!atr_info) {
		return NULL;
	}

	switch (atr_info->TS) {
		case ISO7816_ATR_TS_DIRECT: return "Direct convention";
		case ISO7816_ATR_TS_INVERSE: return "Inverse convention";
		default: return "Unknown";
	}
}

static size_t iso7816_atr_Yi_write_string(const struct iso7816_atr_info_t* atr_info, size_t i, char* const str, size_t str_len)
{
	int r;
	char* str_ptr = str;
	const char* add_comma = "";

	// Yi exists only for Y1 to Y4
	if (i < 1 || i > 4) {
		return 0;
	}

	r = snprintf(str_ptr, str_len, "Y%zu=", i);
	if (r >= str_len) {
		// Not enough space in string buffer; return truncated content
		return str_ptr - str + r;
	}
	str_ptr += r;
	str_len -= r;

	if (atr_info->TA[i]) {
		r = snprintf(str_ptr, str_len, "%s%zu", "TA", i);
		if (r >= str_len) {
			// Not enough space in string buffer; return as snprintf() would
			return str_ptr - str + r;
		}
		str_ptr += r;
		str_len -= r;
		add_comma = ",";
	}

	if (atr_info->TB[i]) {
		r = snprintf(str_ptr, str_len, "%s%s%zu", add_comma, "TB", i);
		if (r >= str_len) {
			// Not enough space in string buffer; return as snprintf() would
			return str_ptr - str + r;
		}
		str_ptr += r;
		str_len -= r;
		add_comma = ",";
	}

	if (atr_info->TC[i]) {
		r = snprintf(str_ptr, str_len, "%s%s%zu", add_comma, "TC", i);
		if (r >= str_len) {
			// Not enough space in string buffer; return as snprintf() would
			return str_ptr - str + r;
		}
		str_ptr += r;
		str_len -= r;
		add_comma = ",";
	}

	if (atr_info->TD[i]) {
		r = snprintf(str_ptr, str_len, "%s%s%zu", add_comma, "TD", i);
		if (r >= str_len) {
			// Not enough space in string buffer; return as snprintf() would
			return str_ptr - str + r;
		}
		str_ptr += r;
		str_len -= r;
	}

	return str_ptr - str;
}

const char* iso7816_atr_T0_get_string(const struct iso7816_atr_info_t* atr_info, char* const str, size_t str_len)
{
	int r;
	char* str_ptr = str;

	if (!atr_info) {
		return NULL;
	}

	// For T0, write Y1
	r = iso7816_atr_Yi_write_string(atr_info, 1, str_ptr, str_len);
	if (r >= str_len) {
		// Not enough space in string buffer; return truncated content
		return str;
	}
	str_ptr += r;
	str_len -= r;

	snprintf(str_ptr, str_len, "; K=%u", atr_info->K_count);

	return str;
}

const char* iso7816_atr_TDi_get_string(const struct iso7816_atr_info_t* atr_info, size_t i, char* str, size_t str_len)
{
	int r;
	char* str_ptr = str;
	uint8_t T;

	if (!atr_info) {
		return NULL;
	}
	if (i < 1 || i > 4) {
		return NULL;
	}
	if (!atr_info->TD[i]) {
		return NULL;
	}

	// For TDi, write Y(i+1)
	r = iso7816_atr_Yi_write_string(atr_info, i + 1, str_ptr, str_len);
	if (r >= str_len) {
		// Not enough space in string buffer; return truncated content
		return str;
	}
	str_ptr += r;
	str_len -= r;

	// Write protocol value
	T = *atr_info->TD[i] & ISO7816_ATR_Tx_OTHER_MASK;
	if (T == ISO7816_ATR_Tx_GLOBAL) {
		r = snprintf(str_ptr, str_len, "; Global (T=%u)", T);
	} else {
		r = snprintf(str_ptr, str_len, "; Protocol T=%u", T);
	}
	if (r >= str_len) {
		// Not enough space in string buffer; return truncated content
		return str;
	}
	str_ptr += r;
	str_len -= r;

	return str;
}
