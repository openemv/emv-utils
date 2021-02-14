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

int iso7816_atr_parse(const uint8_t* atr, size_t atr_len, struct iso7816_atr_info_t* atr_info)
{
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
		}
		if (interface_byte_bits & ISO7816_ATR_Tx_TBi_PRESENT) {
			atr_info->TB[i] = &atr_info->atr[atr_idx++];
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
