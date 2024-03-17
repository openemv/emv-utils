/**
 * @file iso7816.c
 * @brief ISO/IEC 7816 definitions and helper functions
 *
 * Copyright (c) 2021, 2023 Leon Lynch
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
#include "iso7816_compact_tlv.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// Helper functions
static void iso7816_atr_populate_default_parameters(struct iso7816_atr_info_t* atr_info);
static int iso7816_atr_parse_TA1(uint8_t TA1, struct iso7816_atr_info_t* atr_info);
static int iso7816_atr_parse_TB1(uint8_t TB1, struct iso7816_atr_info_t* atr_info);
static int iso7816_atr_parse_TC1(uint8_t TC1, struct iso7816_atr_info_t* atr_info);
static int iso7816_atr_parse_TDi(unsigned int i, uint8_t TDi, struct iso7816_atr_info_t* atr_info);
static int iso7816_atr_parse_TA2(uint8_t TA2, struct iso7816_atr_info_t* atr_info);
static int iso7816_atr_parse_TB2(uint8_t TB2, struct iso7816_atr_info_t* atr_info);
static int iso7816_atr_parse_TC2(uint8_t TC2, struct iso7816_atr_info_t* atr_info);
static int iso7816_atr_parse_TAi(uint8_t protocol, unsigned int i, uint8_t TAi, struct iso7816_atr_info_t* atr_info);
static int iso7816_atr_parse_TBi(uint8_t protocol, unsigned int i, uint8_t TBi, struct iso7816_atr_info_t* atr_info);
static int iso7816_atr_parse_TCi(uint8_t protocol, unsigned int i, uint8_t TCi, struct iso7816_atr_info_t* atr_info);
static void iso7816_compute_gt(struct iso7816_atr_info_t* atr_info);
static void iso7816_compute_wt(struct iso7816_atr_info_t* atr_info);
static int iso7816_atr_parse_historical_bytes(const void* historical_bytes, size_t historical_bytes_len, struct iso7816_atr_info_t* atr_info);

int iso7816_atr_parse(const uint8_t* atr, size_t atr_len, struct iso7816_atr_info_t* atr_info)
{
	int r;
	size_t atr_idx;
	uint8_t protocol = 0; // Protocol indicated in latest TDi interface byte
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

	// Populate default parameters
	// These will be overridden by the parsing below
	iso7816_atr_populate_default_parameters(atr_info);

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

	for (unsigned int i = 1; i < 5; ++i) {
		uint8_t interface_byte_bits = atr_info->atr[atr_idx++]; // Y[i] value according to ISO7816

		// Parse available interface bytes
		if (interface_byte_bits & ISO7816_ATR_Tx_TAi_PRESENT) {
			atr_info->TA[i] = &atr_info->atr[atr_idx++];

			// Extract interface parameters from interface bytes TAi
			switch (i) {
				// Parse TA1
				case 1: r = iso7816_atr_parse_TA1(*atr_info->TA[i], atr_info); break;
				// Parse TA2
				case 2: r = iso7816_atr_parse_TA2(*atr_info->TA[i], atr_info); break;
				// Parse TAi for i>=3
				default: r = iso7816_atr_parse_TAi(protocol, i, *atr_info->TA[i], atr_info); break;
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
				// Parse TB2
				case 2: r = iso7816_atr_parse_TB2(*atr_info->TB[i], atr_info); break;
				// Parse TBi for i>=3
				default: r = iso7816_atr_parse_TBi(protocol, i, *atr_info->TB[i], atr_info); break;
			}
			if (r) {
				return r;
			}
		}
		if (interface_byte_bits & ISO7816_ATR_Tx_TCi_PRESENT) {
			atr_info->TC[i] = &atr_info->atr[atr_idx++];

			// Extract interface parameters from interface bytes TCi
			switch (i) {
				// Parse TC1
				case 1: r = iso7816_atr_parse_TC1(*atr_info->TC[i], atr_info); break;
				// Parse TC2
				case 2: r = iso7816_atr_parse_TC2(*atr_info->TC[i], atr_info); break;
				// Parse TCi for i>=3
				default: r = iso7816_atr_parse_TCi(protocol, i, *atr_info->TC[i], atr_info); break;
			}
			if (r) {
				return r;
			}
		}
		if (interface_byte_bits & ISO7816_ATR_Tx_TDi_PRESENT) {
			atr_info->TD[i] = &atr_info->atr[atr_idx]; // preserve index for next loop iteration

			// Extract interface parameters from interface bytes TDi
			r = iso7816_atr_parse_TDi(i, *atr_info->TD[i], atr_info);
			if (r) {
				return r;
			}

			// Update protocol from latest TDi interface byte
			protocol = *atr_info->TD[i] & ISO7816_ATR_Tx_OTHER_MASK; // T value according to ISO 7816-3:2006, 8.2.3

			// If only T=0 is indicated, TCK is absent
			// If T=0 and T=15 are present, TCK is mandatory
			// For all other cases TCK is also mandatory
			// See ISO 7816-3:2006, 8.2.5
			if (protocol != ISO7816_PROTOCOL_T0) {
				tck_mandatory = true;
			}
		} else {
			// No more interface bytes remaining
			break;
		}
	}

	// Compute various derived parameters
	iso7816_compute_gt(atr_info);
	iso7816_compute_wt(atr_info);

	if (atr_idx > atr_info->atr_len) {
		// Insufficient ATR bytes for interface bytes
		return 3;
	}
	if (atr_idx + atr_info->K_count > atr_info->atr_len) {
		// Insufficient ATR bytes for historical bytes
		return 4;
	}

	if (atr_info->K_count) {
		// Category indicator byte
		atr_info->T1 = atr_info->atr[atr_idx++];

		// Store pointer to historical bytes for later parsing
		atr_info->historical_bytes = &atr_info->atr[atr_idx];

		// Compute historical byte length without T1
		atr_info->historical_bytes_len = atr_info->K_count - 1;
		atr_idx += atr_info->historical_bytes_len;

		// Parse historical byte COMPACT-TLV and extract status indicator bytes
		// See ISO 7816-4:2005, 8.1.1
		switch (atr_info->T1) {
			case ISO7816_ATR_T1_COMPACT_TLV_SI:
				// Store status indicator bytes for later parsing
				atr_info->historical_bytes_len -= 3;
				atr_info->status_indicator_bytes = atr_info->historical_bytes + atr_info->historical_bytes_len;
				atr_info->status_indicator_bytes_len = 3;

				// Intentional fallthrough to COMPACT-TLV parsing

			case ISO7816_ATR_T1_COMPACT_TLV:
				r = iso7816_atr_parse_historical_bytes(atr_info->historical_bytes, atr_info->historical_bytes_len, atr_info);
				if (r) {
					return 5;
				}
				break;

			case ISO7816_ATR_T1_DIR_DATA_REF:
				// TODO: implement
				break;

			default:
				// Proprietary historical bytes
				break;
		}
	}

	// Sanity check
	if (atr_idx > atr_info->atr_len) {
		// Internal parsing error
		return 6;
	}

	// Extract and verify TCK, if mandatory
	if (tck_mandatory) {
		if (atr_idx >= atr_info->atr_len) {
			// T=1 is available but TCK is missing
			return 7;
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
			return 8;
		}
	}

	// Extract status indicator, if available
	// See ISO 7816-4:2005, 8.1.1.3
	if (atr_info->status_indicator_bytes) {
		switch (atr_info->status_indicator_bytes_len) {
			case 1:
				atr_info->status_indicator.LCS = atr_info->status_indicator_bytes[0];
				break;

			case 2:
				atr_info->status_indicator.SW1 = atr_info->status_indicator_bytes[0];
				atr_info->status_indicator.SW2 = atr_info->status_indicator_bytes[1];
				break;

			case 3:
				atr_info->status_indicator.LCS = atr_info->status_indicator_bytes[0];
				atr_info->status_indicator.SW1 = atr_info->status_indicator_bytes[1];
				atr_info->status_indicator.SW2 = atr_info->status_indicator_bytes[2];
				break;
		}
	}

	return 0;
}

static void iso7816_atr_populate_default_parameters(struct iso7816_atr_info_t* atr_info)
{
	// ISO 7816-3 indicates these default parameters:
	// - Fmax = 5MHz (from default F parameters)
	// - Fi/Di = 372/1 (from default F and D parameters)
	// - Ipp = 50mV (from default I parameter)
	// - Vpp = 5V (from default P parameter)
	// - Guard time = 12 ETU (from default N parameter)
	// - Preferred protocol T=0
	// - Card class A only
	// - Clock stop not supported
	// - SPU / C6 not used
	// - WI = 10 (from which WT is computed for protocol T=0)
	// - IFSC = 32 (for protocol T=1)
	// - CWI = 13 (from which CWT is computed for protocol T=1)
	// - BWI = 4 (from which BWT is computed for protocol T=1)
	// - Error detection code LRC (for protocol T=1)

	// TA1 default
	iso7816_atr_parse_TA1(0x11, atr_info);

	// TB1 default
	iso7816_atr_parse_TB1(0x25, atr_info);

	// TC1 default
	iso7816_atr_parse_TC1(0x00, atr_info);

	// TD1 default
	iso7816_atr_parse_TDi(1, 0x00, atr_info);

	// TA2 is absent by default

	// TB2 is absent by default

	// TC2 default
	iso7816_atr_parse_TC2(0x0A, atr_info);

	// TA3 default (for protocol T=1)
	iso7816_atr_parse_TAi(ISO7816_PROTOCOL_T1, 3, 0x20, atr_info);

	// TA3 default (for protocol T=15; global)
	iso7816_atr_parse_TAi(ISO7816_PROTOCOL_T15, 3, ISO7816_CARD_CLASS_A_5V, atr_info);

	// TB3 default (for protocol T=1)
	iso7816_atr_parse_TBi(ISO7816_PROTOCOL_T1, 3, 0x4D, atr_info);

	// TC3 default (for protocol T=1)
	iso7816_atr_parse_TCi(ISO7816_PROTOCOL_T1, 3, 0x00, atr_info);
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
	atr_info->global.Vpp_course = PI1 * 1000;

	// Vpp may be overridden by TB2 later
	atr_info->global.Vpp = atr_info->global.Vpp_course;

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

static int iso7816_atr_parse_TC1(uint8_t TC1, struct iso7816_atr_info_t* atr_info)
{
	atr_info->global.N = TC1;

	// NOTE: GT will be computed in iso7816_compute_gt()

	return 0;
}

static int iso7816_atr_parse_TDi(unsigned int i, uint8_t TDi, struct iso7816_atr_info_t* atr_info)
{
	unsigned int T = (TDi & ISO7816_ATR_Tx_OTHER_MASK);

	if (i == 1) {
		// TD1 only allows T=0 or T=1 as the preferred card protocol
		if (T != ISO7816_PROTOCOL_T0 &&
			T != ISO7816_PROTOCOL_T1
		) {
			// Unsupported protocol
			return 14;
		}

		// TD1 indicates the preferred card protocol
		atr_info->global.protocol = T;
	} else {
		if (T != ISO7816_PROTOCOL_T0 &&
			T != ISO7816_PROTOCOL_T1 &&
			T != ISO7816_PROTOCOL_T15
		) {
			// Unsupported protocol
			return 15;
		}
	}

	return 0;
}

static int iso7816_atr_parse_TA2(uint8_t TA2, struct iso7816_atr_info_t* atr_info)
{
	// TA2 is present, therefore specific mode is available
	// When TA2 is absent, only negotiable mode is available
	atr_info->global.specific_mode = true;
	atr_info->global.specific_mode_protocol = (TA2 & ISO7816_ATR_TA2_PROTOCOL_MASK);

	// TA2 indicates whether the ETU duration should be implicitly known by the reader
	// Otherwise Fi/Di provided by TA1 applies
	atr_info->global.etu_is_implicit = (TA2 & ISO7816_ATR_TA2_IMPLICIT);

	// TA2 indicates whether the specific/negotiable mode may change (eg after warm ATR)
	atr_info->global.specific_mode_may_change = !(TA2 & ISO7816_ATR_TA2_MODE);

	return 0;
}

static int iso7816_atr_parse_TB2(uint8_t TB2, struct iso7816_atr_info_t* atr_info)
{
	uint8_t PI2 = TB2;

	// If TB2 is present, TB1 must indicate that Vpp is present
	if (!atr_info->global.Vpp_connected) {
		return 16;
	}

	// Programming voltage for active state according to ISO 7816-3:1997; deprecated in ISO 7816-3:2006
	if (PI2 < 50 || PI2 > 250) {
		return 17;
	}

	// TB2 is present, therefore override Vpp; PI2 is multiples of 100mV
	atr_info->global.Vpp = PI2 * 100;

	return 0;
}

static int iso7816_atr_parse_TC2(uint8_t TC2, struct iso7816_atr_info_t* atr_info)
{
	uint8_t WI = TC2;

	if (WI == 0) {
		// Reserved by ISO 7816-3:2006, 10.2
		return 18;
	}

	atr_info->protocol_T0.WI = WI;

	// NOTE: WT will be computed in iso7816_compute_wt()

	return 0;
}

static int iso7816_atr_parse_TAi(uint8_t protocol, unsigned int i, uint8_t TAi, struct iso7816_atr_info_t* atr_info)
{
	// Global interface parameters
	if (protocol == ISO7816_PROTOCOL_T15) {
		// First TA for T=15 encodes class indicator Y in bits 1 to 6
		// (ISO 7816-3:2006, 8.3, page 20)
		uint8_t Y = (TAi & ISO7816_ATR_TAi_Y_MASK);

		// First TA for T=15 encodes clock stop indicator X in bits 7 and 8
		// (ISO 7816-3:2006, 8.3, page 20)
		uint8_t X = (TAi & ISO7816_ATR_TAi_X_MASK) >> ISO7816_ATR_TAi_X_SHIFT;

		if (Y < 0x01 || Y > 0x07) {
			// Unsupported card classes
			return 19;
		}
		atr_info->global.card_classes = Y;

		if (X > 0x03) {
			// Invalid clock stop indicator
			return 20;
		}
		atr_info->global.clock_stop = X;
	}

	// Protocol T=1 parameters
	if (protocol == ISO7816_PROTOCOL_T1) {
		// First TA for T=1 encodes IFS (ISO 7816-3:2006, 11.4.2)
		uint8_t IFSI = TAi;

		if (IFSI == 0x00 || IFSI == 0xFF) {
			// Reserved by ISO 7816-3:2006, 11.4.2
			return 21;
		}
		atr_info->protocol_T1.IFSI = IFSI;
	}

	return 0;
}

static int iso7816_atr_parse_TBi(uint8_t protocol, unsigned int i, uint8_t TBi, struct iso7816_atr_info_t* atr_info)
{
	// Global interface parameters
	if (protocol == ISO7816_PROTOCOL_T15) {
		// First TB for T=15 indicates the use of SPU by the card (ISO 7816-3:2006, 8.3, page 20)
		if (TBi) {
			if ((TBi & ISO7816_ATR_TBi_SPU_MASK) == 0) {
				atr_info->global.spu = ISO7816_SPU_STANDARD;
			} else {
				atr_info->global.spu = ISO7816_SPU_PROPRIETARY;
			}
		} else {
			atr_info->global.spu = ISO7816_SPU_NOT_USED;
		}
	}

	// Protocol T=1 parameters
	if (protocol == ISO7816_PROTOCOL_T1) {
		// First TB for T=1 encodes CWI and BWI (ISO 7816-3:2006, 11.4.3)
		uint8_t CWI = (TBi & ISO7816_ATR_TBi_CWI_MASK);
		uint8_t BWI = (TBi & ISO7816_ATR_TBi_BWI_MASK) >> ISO7816_ATR_TBi_BWI_SHIFT;

		// Verify CWT according to ISO 7816-3:2006, 11.4.3
		if (CWI > 15) {
			// Invalid CWI value
			return 22;
		}
		atr_info->protocol_T1.CWI = CWI;

		// NOTE: CWT will be computed in iso7816_compute_wt()

		// Verify BWT according to ISO 7816-3:2006, 11.4.3
		if (BWI > 9) {
			// Invalid BWI value
			return 23;
		}
		atr_info->protocol_T1.BWI = BWI;

		// NOTE: BWT will be computed in iso7816_compute_wt()
	}

	return 0;
}

static int iso7816_atr_parse_TCi(uint8_t protocol, unsigned int i, uint8_t TCi, struct iso7816_atr_info_t* atr_info)
{
	// Protocol T=1 parameters
	if (protocol == ISO7816_PROTOCOL_T1) {
		// First TC for T=1 indicates the error detection code to be used (ISO 7816-3:2006, 11.4.4)
		if (TCi & ISO7816_ATR_TCi_ERROR_MASK) {
			atr_info->protocol_T1.error_detection_code = ISO7816_ERROR_DETECTION_CODE_CRC;
		} else {
			atr_info->protocol_T1.error_detection_code = ISO7816_ERROR_DETECTION_CODE_LRC;
		}
	}

	return 0;
}

static void iso7816_compute_gt(struct iso7816_atr_info_t* atr_info)
{
	bool T15_present = false;

	// Determine whether T=15 is present
	for (unsigned int i = 1; i < 5; ++i) {
		if (atr_info->TD[i] &&
			(*atr_info->TD[i] & ISO7816_ATR_Tx_OTHER_MASK) == ISO7816_PROTOCOL_T15
		) {
			T15_present = true;
			break;
		}
	}

	if (atr_info->global.N != 0xFF) {
		// From ISO 7816-3:2006, 8.3, page 19:
		// If N is 0 to 254, then GT = 12 ETU + R x N/f
		// If T=15 is absent in the ATR, R = F/D as used for computing ETU
		// If T=15 is present in the ATR, R = Fi/Di as defined by TA1

		if (!T15_present) {
			// For T=15 absent:
			// GT = 12 ETU + R x N/f
			//    = 12 ETU + F/D x N/f
			// Given 1 ETU = F/D x 1/f (see ISO 7816-3:2006, 7.1):
			// GT = 12 + N x 1 ETU

			// Thus we can simplify it to...
			atr_info->global.GT = 12 + atr_info->global.N;
		} else {
			// For T=15 present:
			// GT = 12 ETU + R x N/f
			//      12 ETU + Fi/Di x N/f
			// Given 1 ETU = F/D x 1/f (see ISO 7816-3:2006, 7.1):
			// GT = 12 ETU + (Fi/Di x N/f) / (F/D x 1/f)
			//    = 12 ETU + (Fi/Di x N/f) x (D/F x f)
			//    = 12 ETU + Fi/Di x N x D/F
			// Technically F and D need to be obtained from the card reader as
			// they may be different from what is indicated in TA1. But for
			// now we'll just assume they are the same. Thus:
			// GT = 12 ETU + N
			// Which looks exactly the same as when T=15 is absent, although
			// this is supposed to be relative to the eventual negotiated ETU,
			// and not just the initial ETU.

			// I don't really know what I'm doing, so if this is wrong, please
			// submit a merge request...
			atr_info->global.GT = 12 + atr_info->global.N;
		}

		// From ISO 7816-3:2006, 11.2:
		// T=1: if N is 0 to 254, then CGT = GT
		atr_info->protocol_T1.CGT = atr_info->global.GT;
	} else {
		// From ISO 7816-3:2006, 8.3, page 19:
		// The use of N=255 is protocol dependent
		// T=0: GT = 12 ETU
		// T=1: GT = 11 ETU (see ISO 7816-3:2006, 11.2)

		if (atr_info->global.protocol == ISO7816_PROTOCOL_T0) {
			atr_info->global.GT = 12;
		}
		if (atr_info->global.protocol == ISO7816_PROTOCOL_T1) {
			atr_info->global.GT = 11;
		}

		// From ISO 7816-3:2006, 11.2:
		// T=1: if N=256, then CGT = 11 ETU
		atr_info->protocol_T1.CGT = 11;
	}
}

static void iso7816_compute_wt(struct iso7816_atr_info_t* atr_info)
{
	unsigned int Di = atr_info->global.Di;
	unsigned int Fi = atr_info->global.Fi;
	unsigned int WI = atr_info->protocol_T0.WI;
	unsigned int CWI = atr_info->protocol_T1.CWI;
	unsigned int BWI = atr_info->protocol_T1.BWI;

	// From ISO 7816-3:2006, 10.2:
	// WT = WI x 960 x Fi/f
	// Given 1 ETU = F/D x 1/f (see ISO 7816-3:2006, 7.1):
	// WT = WI x 960 x Fi/f / (F/D x 1/f) ETU
	//    = WI x 960 x Fi/f x (D/F x f) ETU
	//    = WI x 960 x Fi x D / F ETU
	// And if we assume F is as indicated in TA1, thus Fi, then:
	// WT = WI x 960 x D ETU
	// Which is the same conclusion that EMV comes to below...

	// From EMV Contact Interface Specification v1.0, 9.2.2.1:
	// WWT = 960 x D x WI ETUs (D and WI are returned in TA1 and TC2, respectively)

	// And finally, after all that thinking...
	atr_info->protocol_T0.WT = WI * 960 * Di;

	// Compute CWT according to ISO 7816-3:2006, 11.4.3
	atr_info->protocol_T1.CWT = 11 + (1 << CWI);

	// From ISO 7816-3:2006, 11.4.3:
	// BWT = 11etu + 2^BWI x 960 x Fd / f; where Fd is default F=372 and f is frequency
	// NOTE: This formula specifies the first term of the sum in ETUs, but not the second
	// part. Therefore, to convert the second term of the sum to ETUs, we must divide the
	// second term by ETU, as defined relative to F and D. Thus:
	// Given 1 ETU = F/D x 1/f (see ISO 7816-3:2006, 7.1):
	// BWT = 11etu + (2^BWI x 960 x Fd / f) / (F/D x 1/f)
	//     = 11etu + (2^BWI x 960 x Fd / f) x (D/F x f)
	//     = 11etu + (2^BWI x 960 x Fd) x (D/F)
	//     = 11etu + (2^BWI x 960 x Fd x D / F)
	// And given that Fd is default F=372:
	// BWT = 11etu + (2^BWI x 960 x 372 x D / F)
	// Which is the same conclusion that EMV comes to below...

	// From EMV Contact Interface Specification v1.0, 9.2.4.2.2:
	// BWT = (((2^BWI x 960 x 372 x D / F) + 11)etu; where D is Di and F is Fi

	// And finally, after all that thinking...
	atr_info->protocol_T1.BWT = 11 + (((1 << BWI) * 960 * 372 * Di) / Fi);
}

static int iso7816_atr_parse_historical_bytes(const void* historical_bytes, size_t historical_bytes_len, struct iso7816_atr_info_t* atr_info)
{
	int r;
	struct iso7816_compact_tlv_itr_t itr;
	struct iso7816_compact_tlv_t tlv;

	r = iso7816_compact_tlv_itr_init(historical_bytes, historical_bytes_len, &itr);
	if (r) {
		return 24;
	}

	while ((r = iso7816_compact_tlv_itr_next(&itr, &tlv)) > 0) {
		// Capture status indicator, if available
		if (tlv.tag == ISO7816_COMPACT_TLV_SI) {
			atr_info->status_indicator_bytes = tlv.value;
			atr_info->status_indicator_bytes_len = tlv.length;
		}
	}
	if (r) {
		return 25;
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

const char* iso7816_atr_TAi_get_string(const struct iso7816_atr_info_t* atr_info, size_t i, char* str, size_t str_len)
{
	int r;
	char* str_ptr = str;

	if (!atr_info) {
		return NULL;
	}
	if (i < 1 || i > 4) {
		return NULL;
	}

	// NOTE: It is not necessary to check atr_info->TA[i] here. Even if TAi is
	// absent, atr_info will nonetheless indicate the defaults.

	// For TA1
	if (i == 1) {
		snprintf(str, str_len,
			"Fi=%u; Di=%u; %u cycles/ETU @ max %.1fMHz; max %lu bit/s",
			atr_info->global.Fi,
			atr_info->global.Di,
			atr_info->global.Fi / atr_info->global.Di,
			atr_info->global.fmax,
			((unsigned long)(atr_info->global.fmax * 1000000)) / (atr_info->global.Fi / atr_info->global.Di)
		);

		return str;
	}

	// For TA2
	if (i == 2) {
		if (atr_info->TA[2]) {
			snprintf(str, str_len,
				"Specific mode (%s); ETU%s; protocol T=%u",
				atr_info->global.specific_mode_may_change ? "mutable" : "immutable",
				atr_info->global.etu_is_implicit ? " is implicit" : "=Fi/Di",
				atr_info->global.specific_mode_protocol
			);
		} else {
			snprintf(str, str_len, "Negotiable mode");
		}

		return str;
	}

	// For TAi when i >= 3
	if (i >= 3 && atr_info->TA[i]) {
		// If TA[i] is present, TD[i-1] must have been present
		if (!atr_info->TD[i-1]) {
			// atr_info is invalid
			return NULL;
		}

		// Extract protocol from previous TDi interface byte for subsequent
		// protocol specific interface bytes
		uint8_t T = *atr_info->TD[i-1] & ISO7816_ATR_Tx_OTHER_MASK;

		// For first TA for T=15
		if (T == ISO7816_PROTOCOL_T15) {
			const char* clock_stop_str = "";
			switch (atr_info->global.clock_stop) {
				case ISO7816_CLOCK_STOP_NOT_SUPPORTED: clock_stop_str = "Not supported"; break;
				case ISO7816_CLOCK_STOP_STATE_L: clock_stop_str = "State L"; break;
				case ISO7816_CLOCK_STOP_STATE_H: clock_stop_str = "State H"; break;
				case ISO7816_CLOCK_STOP_NO_PREFERENCE: clock_stop_str = "No preference"; break;
			}

			r = snprintf(str_ptr, str_len, "Clock stop: %s; Class: ", clock_stop_str);
			if (r >= str_len) {
				// Not enough space in string buffer; return truncated content
				return str;
			}
			str_ptr += r;
			str_len -= r;

			const char* add_comma = "";
			if (atr_info->global.card_classes & ISO7816_CARD_CLASS_A_5V) {
				r = snprintf(str_ptr, str_len, "A (5V)");
				if (r >= str_len) {
					// Not enough space in string buffer; return truncated content
					return str;
				}
				str_ptr += r;
				str_len -= r;
				add_comma = ", ";
			}
			if (atr_info->global.card_classes & ISO7816_CARD_CLASS_B_3V) {
				r = snprintf(str_ptr, str_len, "%sB (3V)", add_comma);
				if (r >= str_len) {
					// Not enough space in string buffer; return truncated content
					return str;
				}
				str_ptr += r;
				str_len -= r;
				add_comma = ", ";
			}
			if (atr_info->global.card_classes & ISO7816_CARD_CLASS_C_1V8) {
				r = snprintf(str_ptr, str_len, "%sC (1.8V)", add_comma);
				if (r >= str_len) {
					// Not enough space in string buffer; return truncated content
					return str;
				}
				str_ptr += r;
				str_len -= r;
				add_comma = ", ";
			}

			return str;
		}

		// For first TA for T=1
		if (T == ISO7816_PROTOCOL_T1) {
			snprintf(str, str_len, "IFSI=%u", atr_info->protocol_T1.IFSI);
			return str;
		}
	}

	snprintf(str, str_len, "Unimplemented");
	return str;
}

const char* iso7816_atr_TBi_get_string(const struct iso7816_atr_info_t* atr_info, size_t i, char* str, size_t str_len)
{
	if (!atr_info) {
		return NULL;
	}
	if (i < 1 || i > 4) {
		return NULL;
	}

	// NOTE: It is not necessary to check atr_info->TB[i] here. Even if TBi is
	// absent, atr_info will nonetheless indicate the defaults.

	// For TB1
	if (i == 1) {
		// If TB1 is present and indicates that Vpp is connected
		if (atr_info->TB[1] &&
			atr_info->global.Vpp_connected
		) {
			snprintf(str, str_len,
				"Vpp is connected; Vpp=%umV; Ipp=%umA",
				atr_info->global.Vpp_course,
				atr_info->global.Ipp
			);
		} else {
			snprintf(str, str_len, "Vpp is not connected");
		}

		return str;
	}

	// For TB2
	if (i == 2) {
		// If TB1 and TB2 are both present and Vpp is connected
		if (atr_info->TB[1] &&
			atr_info->TB[2] &&
			atr_info->global.Vpp_connected
		) {
			snprintf(str, str_len,
				"Vpp=%umV",
				atr_info->global.Vpp
			);
		} else {
			snprintf(str, str_len, "Vpp is not connected");
		}

		return str;
	}

	// For TBi when i >= 3
	if (i >= 3 && atr_info->TB[i]) {
		// If TB[i] is present, TD[i-1] must have been present
		if (!atr_info->TD[i-1]) {
			// atr_info is invalid
			return NULL;
		}

		// Extract protocol from previous TDi interface byte for subsequent
		// protocol specific interface bytes
		uint8_t T = *atr_info->TD[i-1] & ISO7816_ATR_Tx_OTHER_MASK;

		// For first TB for T=15
		if (T == ISO7816_PROTOCOL_T15) {
			switch (atr_info->global.spu) {
				case ISO7816_SPU_NOT_USED:
					snprintf(str, str_len, "SPU not used");
					break;

				case ISO7816_SPU_STANDARD:
					snprintf(str, str_len, "Standard usage of SPU");
					break;

				case ISO7816_SPU_PROPRIETARY:
					snprintf(str, str_len, "Proprietary usage of SPU");
					break;
			}

			return str;
		}

		// For first TB for T=1
		if (T == ISO7816_PROTOCOL_T1) {
			snprintf(str, str_len,
				"CWT=%u; BWT=%u",
				atr_info->protocol_T1.CWT,
				atr_info->protocol_T1.BWT
			);
			return str;
		}
	}

	snprintf(str, str_len, "Unimplemented");
	return str;
}

const char* iso7816_atr_TCi_get_string(const struct iso7816_atr_info_t* atr_info, size_t i, char* str, size_t str_len)
{
	if (!atr_info) {
		return NULL;
	}
	if (i < 1 || i > 4) {
		return NULL;
	}

	// NOTE: It is not necessary to check atr_info->TC[i] here. Even if TBi is
	// absent, atr_info will nonetheless indicate the defaults.

	// For TC1
	if (i == 1) {
		snprintf(str, str_len, "N=%u; GT=%u",
			atr_info->global.N,
			atr_info->global.GT
		);

		return str;
	}

	// For TC2
	if (i == 2) {
		snprintf(str, str_len, "WI=%u; WT=%u",
			atr_info->protocol_T0.WI,
			atr_info->protocol_T0.WT
		);

		return str;
	}

	// For TCi when i >= 3
	if (i >= 3 && atr_info->TC[i]) {
		// If TC[i] is present, TD[i-1] must have been present
		if (!atr_info->TD[i-1]) {
			// atr_info is invalid
			return NULL;
		}

		// Extract protocol from previous TDi interface byte for subsequent
		// protocol specific interface bytes
		uint8_t T = *atr_info->TD[i-1] & ISO7816_ATR_Tx_OTHER_MASK;

		// For first TC for T=15
		if (T == ISO7816_PROTOCOL_T1) {
			switch (atr_info->protocol_T1.error_detection_code) {
				case ISO7816_ERROR_DETECTION_CODE_LRC:
					snprintf(str, str_len, "Longitudinal Redundancy Check (LRC)");
					break;

				case ISO7816_ERROR_DETECTION_CODE_CRC:
					snprintf(str, str_len, "Cyclic Redundancy Check (CRC)");
					break;
			}

			return str;
		}
	}

	snprintf(str, str_len, "Unimplemented");
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
	if (T == ISO7816_PROTOCOL_T15) {
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

const char* iso7816_atr_T1_get_string(const struct iso7816_atr_info_t* atr_info)
{
	if (!atr_info) {
		return NULL;
	}
	if (!atr_info->K_count) {
		return NULL;
	}

	// See ISO 7816-4:2005, 8.1.1.1, table 83
	switch (atr_info->T1) {
		case ISO7816_ATR_T1_COMPACT_TLV_SI:
			return "COMPACT-TLV followed by mandatory status indicator";

		case ISO7816_ATR_T1_DIR_DATA_REF:
			return "DIR data reference";

		case ISO7816_ATR_T1_COMPACT_TLV:
			return "COMPACT-TLV including optional status indicator";
	}

	if (atr_info->T1 > ISO7816_ATR_T1_COMPACT_TLV &&
		atr_info->T1 <= 0x8F
	) {
		return "RFU";
	}

	return "Proprietary";
}
