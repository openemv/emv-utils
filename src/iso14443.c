/**
 * @file iso14443.c
 * @brief ISO/IEC 14443 definitions and helper functions
 *
 * Copyright 2026 Leon Lynch
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

#include "iso14443.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// Helper functions
static void iso14443_ats_populate_default_parameters(struct iso14443_ats_info_t* ats_info);
static int iso14443_ats_parse_T0(uint8_t T0, struct iso14443_ats_info_t* ats_info);
static int iso14443_ats_parse_TA1(uint8_t TA1, struct iso14443_ats_info_t* ats_info);
static int iso14443_ats_parse_TB1(uint8_t TB1, struct iso14443_ats_info_t* ats_info);
static int iso14443_ats_parse_TC1(uint8_t TC1, struct iso14443_ats_info_t* ats_info);

int iso14443_ats_parse(const uint8_t* ats, size_t ats_len, struct iso14443_ats_info_t* ats_info)
{
	int r;
	size_t ats_idx;

	if (!ats) {
		return -1;
	}

	if (!ats_info) {
		return -1;
	}

	if (ats_len < ISO14443_ATS_MIN_SIZE || ats_len > ISO14443_ATS_MAX_SIZE) {
		// Invalid number of ATS bytes
		return 1;
	}

	memset(ats_info, 0, sizeof(*ats_info));

	// Copy ATS bytes
	memcpy(ats_info->ats, ats, ats_len);
	ats_info->ats_len = ats_len;

	// Populate default parameters
	// These will be overridden by the parsing below
	iso14443_ats_populate_default_parameters(ats_info);

	// Parse TL (mandatory length byte)
	// See ISO 14443-4:2008, 5.2.2
	ats_info->TL = ats_info->ats[0];

	if (ats_info->TL > ats_info->ats_len) {
		// TL claims more bytes than were provided
		return 2;
	}

	if (ats_info->TL < ISO14443_ATS_MIN_SIZE) {
		// TL = 0 is invalid; TL includes itself so minimum is 1
		return 3;
	}

	ats_idx = 1;

	// Parse T0 (optional format byte)
	// See ISO 14443-4:2008, 5.2.3
	// T0 is present as soon as the length is greater than 1
	if (ats_idx >= ats_info->TL) {
		// Only TL present; no other fields
		return 0;
	}

	ats_info->T0 = &ats_info->ats[ats_idx++];
	r = iso14443_ats_parse_T0(*ats_info->T0, ats_info);
	if (r) {
		return r;
	}

	// Parse TA(1) if indicated by T0
	// See ISO 14443-4:2008, 5.2.4
	if (*ats_info->T0 & ISO14443_ATS_T0_TA1_PRESENT) {
		if (ats_idx >= ats_info->TL) {
			return 4;
		}
		ats_info->TA1 = &ats_info->ats[ats_idx++];
		r = iso14443_ats_parse_TA1(*ats_info->TA1, ats_info);
		if (r) {
			return r;
		}
	}

	// Parse TB(1) if indicated by T0
	// See ISO 14443-4:2008, 5.2.5
	if (*ats_info->T0 & ISO14443_ATS_T0_TB1_PRESENT) {
		if (ats_idx >= ats_info->TL) {
			return 5;
		}
		ats_info->TB1 = &ats_info->ats[ats_idx++];
		r = iso14443_ats_parse_TB1(*ats_info->TB1, ats_info);
		if (r) {
			return r;
		}
	}

	// Parse TC(1) if indicated by T0
	// See ISO 14443-4:2008, 5.2.6
	if (*ats_info->T0 & ISO14443_ATS_T0_TC1_PRESENT) {
		if (ats_idx >= ats_info->TL) {
			return 6;
		}
		ats_info->TC1 = &ats_info->ats[ats_idx++];
		r = iso14443_ats_parse_TC1(*ats_info->TC1, ats_info);
		if (r) {
			return r;
		}
	}

	// Remaining bytes are historical bytes
	// See ISO 14443-4:2008, 5.2.7
	if (ats_idx < ats_info->TL) {
		ats_info->historical_bytes = &ats_info->ats[ats_idx];
		ats_info->historical_bytes_len = ats_info->TL - ats_idx;
		ats_idx += ats_info->historical_bytes_len;
	}

	// Sanity check
	if (ats_idx > ats_info->ats_len) {
		// Internal parsing error
		return 7;
	}

	return 0;
}

static void iso14443_ats_populate_default_parameters(struct iso14443_ats_info_t* ats_info)
{
	// ISO 14443-4 indicates these default parameters when fields are absent:
	// - FSCI = 2 (FSC = 32 bytes)
	// - DS = 0, DR = 0 (only D=1, 106 kbit/s)
	// - FWI = 4
	// - SFGI = 0 (no SFGT needed)
	// - CID supported, NAD not supported

	// T0 default (see ISO 14443-4:2008, 5.2.3)
	iso14443_ats_parse_T0(0x02, ats_info);

	// TA1 default (ISO 14443-4:2008, 5.2.4)
	iso14443_ats_parse_TA1(0x00, ats_info);

	// TB1 default (ISO 14443-4:2008, 5.2.5 and 7.2)
	iso14443_ats_parse_TB1(0x40, ats_info);

	// TC1 default (ISO 14443-4:2008, 5.2.6)
	iso14443_ats_parse_TC1(0x02, ats_info);
}

static int iso14443_ats_parse_T0(uint8_t T0, struct iso14443_ats_info_t* ats_info)
{
	uint8_t FSCI = T0 & ISO14443_ATS_T0_FSCI_MASK;

	// Convert FSCI to FSC (frame size in bytes)
	// See ISO 14443-4:2008, 5.2.3
	// See ISO 14443-4:2008, 5.1, table 1
	// EMV Level 1 Contactless Interface Specification v3.2, 5.7.2, table 5.17
	switch (FSCI) {
		case 0x0: ats_info->FSC = 16; break;
		case 0x1: ats_info->FSC = 24; break;
		case 0x2: ats_info->FSC = 32; break;
		case 0x3: ats_info->FSC = 40; break;
		case 0x4: ats_info->FSC = 48; break;
		case 0x5: ats_info->FSC = 64; break;
		case 0x6: ats_info->FSC = 96; break;
		case 0x7: ats_info->FSC = 128; break;
		case 0x8: ats_info->FSC = 256; break;
		// Although ISO 14443-4:2008, 5.2.3 considers FSCI values 9 to F as
		// non-compliant and interpreted as FSCI=8 (FSC=256), EMV allows
		// values 9 to C, and interprets values D to F as FSCI=C (FSC=4096)
		case 0x9: ats_info->FSC = 512; break;
		case 0xA: ats_info->FSC = 1024; break;
		case 0xB: ats_info->FSC = 2048; break;
		case 0xC: ats_info->FSC = 4096; break;
		default: ats_info->FSC = 4096; break;
	}

	return 0;
}

static int iso14443_ats_parse_TA1(uint8_t TA1, struct iso14443_ats_info_t* ats_info)
{
	// Bit 4 is RFU but if set then interpret entire TA(1) as 0x00
	// See ISO 14443-4:2008, 5.2.4
	if (TA1 & ISO14443_ATS_TA1_RFU) {
		TA1 = 0;
	}

	// Bit 8 indicates whether only the same D must be used for both directions
	// See ISO 14443-4:2008, 5.2.4
	ats_info->same_d_required = (TA1 & ISO14443_ATS_TA1_SAME_D) != 0;

	// DS (supported divisors from PICC to PCD) encoded in bits 5 to 7
	// See ISO 14443-4:2008, 5.2.4
	ats_info->DS = (TA1 & ISO14443_ATS_TA1_DS_MASK) >> ISO14443_ATS_TA1_DS_SHIFT;

	// DR (supported divisors from PCD to PICC) encoded in bits 1 to 3
	// See ISO 14443-4:2008, 5.2.4
	ats_info->DR = TA1 & ISO14443_ATS_TA1_DR_MASK;

	return 0;
}

static int iso14443_ats_parse_TB1(uint8_t TB1, struct iso14443_ats_info_t* ats_info)
{
	// FWI encodes FWT according to ISO 14443-4:2008, 5.2.5
	ats_info->FWI = (TB1 & ISO14443_ATS_TB1_FWI_MASK) >> ISO14443_ATS_TB1_FWI_SHIFT;
	if (ats_info->FWI == 15) {
		// FWI = 15 is RFU and interpreted as FWI = 4
		// See ISO 14443-4:2008, 5.2.5
		ats_info->FWI = 4;
	}

	// SFGI encodes a multiplier for SFGT according to ISO 14443-4:2008, 5.2.5
	ats_info->SFGI = TB1 & ISO14443_ATS_TB1_SFGI_MASK;
	if (ats_info->SFGI == 15) {
		// SFGI = 15 is and interpreted as SFGI = 0
		// See ISO 14443-4:2008, 5.2.5
		ats_info->SFGI = 0;
	}

	return 0;
}

static int iso14443_ats_parse_TC1(uint8_t TC1, struct iso14443_ats_info_t* ats_info)
{
	// CID and NAD support flags override the defaults set in
	// iso14443_ats_populate_default_parameters() when TC(1) is present
	// See ISO 14443-4:2008, 5.2.6
	ats_info->CID_supported = (TC1 & ISO14443_ATS_TC1_CID) != 0;
	ats_info->NAD_supported = (TC1 & ISO14443_ATS_TC1_NAD) != 0;

	return 0;
}
