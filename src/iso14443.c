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
#include "iso7816_compact_tlv.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Helper functions
static void iso14443_ats_populate_default_parameters(struct iso14443_ats_info_t* ats_info);
static int iso14443_ats_parse_T0(uint8_t T0, struct iso14443_ats_info_t* ats_info);
static int iso14443_ats_parse_TA1(uint8_t TA1, struct iso14443_ats_info_t* ats_info);
static int iso14443_ats_parse_TB1(uint8_t TB1, struct iso14443_ats_info_t* ats_info);
static int iso14443_ats_parse_TC1(uint8_t TC1, struct iso14443_ats_info_t* ats_info);
static int iso14443_ats_parse_historical_bytes(const void* historical_bytes, size_t historical_bytes_len, struct iso14443_ats_info_t* ats_info);
static void iso14443_atqb_populate_default_parameters(struct iso14443_atqb_info_t* atqb_info);
static int iso14443_atqb_parse_PI1(uint8_t PI1, struct iso14443_atqb_info_t* atqb_info);
static int iso14443_atqb_parse_PI2(uint8_t PI2, struct iso14443_atqb_info_t* atqb_info);
static int iso14443_atqb_parse_PI3(uint8_t PI3, struct iso14443_atqb_info_t* atqb_info);
static int iso14443_atqb_parse_PI4(uint8_t PI4, struct iso14443_atqb_info_t* atqb_info);
static int iso14443_atqb_parse_application_data(const uint8_t* application_data, struct iso14443_atqb_info_t* atqb_info);

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
	// See ISO 7816-4:2005, 8.1.1
	if (ats_idx < ats_info->TL) {
		ats_info->K_count = ats_info->TL - ats_idx;

		// Category indicator byte
		ats_info->T1 = ats_info->ats[ats_idx++];

		// Store pointer to historical bytes for later parsing
		ats_info->historical_bytes = &ats_info->ats[ats_idx];

		// Compute historical byte length without T1
		ats_info->historical_bytes_len = ats_info->K_count - 1;
		ats_idx += ats_info->historical_bytes_len;

		// Parse historical byte COMPACT-TLV and extract status indicator bytes
		// See ISO 7816-4:2005, 8.1.1
		switch (ats_info->T1) {
			case ISO14443_ATS_T1_COMPACT_TLV_SI:
				if (ats_info->historical_bytes_len < 3) {
					// Insufficient historical bytes for status indicator
					return 8;
				}

				// Store status indicator bytes for later parsing
				ats_info->historical_bytes_len -= 3;
				ats_info->status_indicator_bytes = ats_info->historical_bytes + ats_info->historical_bytes_len;
				ats_info->status_indicator_bytes_len = 3;

				// Intentional fallthrough to COMPACT-TLV parsing

			case ISO14443_ATS_T1_COMPACT_TLV:
				r = iso14443_ats_parse_historical_bytes(ats_info->historical_bytes, ats_info->historical_bytes_len, ats_info);
				if (r) {
					return r;
				}
				break;

			case ISO14443_ATS_T1_DIR_DATA_REF:
				// TODO: implement
				break;

			default:
				// Proprietary historical bytes
				break;
		}
	}

	// Sanity check
	if (ats_idx > ats_info->ats_len) {
		// Internal parsing error
		return 7;
	}

	// Extract status indicator, if available
	// See ISO 7816-4:2005, 8.1.1.3
	if (ats_info->status_indicator_bytes) {
		switch (ats_info->status_indicator_bytes_len) {
			case 1:
				ats_info->status_indicator.LCS = ats_info->status_indicator_bytes[0];
				break;

			case 2:
				ats_info->status_indicator.SW1 = ats_info->status_indicator_bytes[0];
				ats_info->status_indicator.SW2 = ats_info->status_indicator_bytes[1];
				break;

			case 3:
				ats_info->status_indicator.LCS = ats_info->status_indicator_bytes[0];
				ats_info->status_indicator.SW1 = ats_info->status_indicator_bytes[1];
				ats_info->status_indicator.SW2 = ats_info->status_indicator_bytes[2];
				break;
		}
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
	ats_info->same_d_required = (TA1 & ISO14443_ATS_TA1_SAME_D);

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
		// SFGI = 15 is RFU and interpreted as SFGI = 0
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
	ats_info->CID_supported = (TC1 & ISO14443_ATS_TC1_CID);
	ats_info->NAD_supported = (TC1 & ISO14443_ATS_TC1_NAD);

	return 0;
}

static int iso14443_ats_parse_historical_bytes(const void* historical_bytes, size_t historical_bytes_len, struct iso14443_ats_info_t* ats_info)
{
	int r;
	struct iso7816_compact_tlv_itr_t itr;
	struct iso7816_compact_tlv_t tlv;

	r = iso7816_compact_tlv_itr_init(historical_bytes, historical_bytes_len, &itr);
	if (r) {
		return 9;
	}

	while ((r = iso7816_compact_tlv_itr_next(&itr, &tlv)) > 0) {
		// Capture status indicator, if available
		if (tlv.tag == ISO7816_COMPACT_TLV_SI) {
			ats_info->status_indicator_bytes = tlv.value;
			ats_info->status_indicator_bytes_len = tlv.length;
		}
	}
	if (r) {
		return 10;
	}

	return 0;
}

const char* iso14443_ats_T0_get_string(const struct iso14443_ats_info_t* ats_info, char* str, size_t str_len)
{
	int r;
	char* str_ptr = str;
	const char* add_comma = "";

	if (!ats_info) {
		return NULL;
	}

	// List which interface bytes are announced by T0
	// See ISO 14443-4:2008, 5.2.3
	if (ats_info->T0) {
		if (*ats_info->T0 & ISO14443_ATS_T0_TA1_PRESENT) {
			r = snprintf(str_ptr, str_len, "%s%s", add_comma, "TA(1)");
			if (r >= str_len) {
				// Not enough space in string buffer; return truncated content
				return str;
			}
			str_ptr += r;
			str_len -= r;
			add_comma = ",";
		}
		if (*ats_info->T0 & ISO14443_ATS_T0_TB1_PRESENT) {
			r = snprintf(str_ptr, str_len, "%s%s", add_comma, "TB(1)");
			if (r >= str_len) {
				// Not enough space in string buffer; return truncated content
				return str;
			}
			str_ptr += r;
			str_len -= r;
			add_comma = ",";
		}
		if (*ats_info->T0 & ISO14443_ATS_T0_TC1_PRESENT) {
			r = snprintf(str_ptr, str_len, "%s%s", add_comma, "TC(1)");
			if (r >= str_len) {
				// Not enough space in string buffer; return truncated content
				return str;
			}
			str_ptr += r;
			str_len -= r;
			add_comma = ",";
		}
	}

	// Separator before FSC if any interface byte was listed
	if (add_comma[0] != '\0') {
		r = snprintf(str_ptr, str_len, "; ");
		if (r >= str_len) {
			// Not enough space in string buffer; return truncated content
			return str;
		}
		str_ptr += r;
		str_len -= r;
	}

	snprintf(str_ptr, str_len, "FSC=%u", ats_info->FSC);

	return str;
}

const char* iso14443_ats_TA1_get_string(const struct iso14443_ats_info_t* ats_info, char* str, size_t str_len)
{
	if (!ats_info) {
		return NULL;
	}

	// NOTE: It is not necessary to check ats_info->TA1 here. Even if TA(1) is
	// absent, ats_info will nonetheless indicate the defaults.

	snprintf(str, str_len,
		"DS=106%s%s%s kbit/s; DR=106%s%s%s kbit/s%s",
		ats_info->DS & ISO14443_D2_SUPPORTED ? ", 212" : "",
		ats_info->DS & ISO14443_D4_SUPPORTED ? ", 424" : "",
		ats_info->DS & ISO14443_D8_SUPPORTED ? ", 848" : "",
		ats_info->DR & ISO14443_D2_SUPPORTED ? ", 212" : "",
		ats_info->DR & ISO14443_D4_SUPPORTED ? ", 424" : "",
		ats_info->DR & ISO14443_D8_SUPPORTED ? ", 848" : "",
		ats_info->same_d_required ? "; same D required" : ""
	);

	return str;
}

const char* iso14443_ats_TB1_get_string(const struct iso14443_ats_info_t* ats_info, char* str, size_t str_len)
{
	if (!ats_info) {
		return NULL;
	}

	// NOTE: It is not necessary to check ats_info->TB1 here. Even if TB(1) is
	// absent, ats_info will nonetheless indicate the defaults.

	snprintf(str, str_len, "FWI=%u; SFGI=%u",
		ats_info->FWI, ats_info->SFGI
	);

	return str;
}

const char* iso14443_ats_TC1_get_string(const struct iso14443_ats_info_t* ats_info, char* str, size_t str_len)
{
	if (!ats_info) {
		return NULL;
	}

	// NOTE: It is not necessary to check ats_info->TC1 here. Even if TC(1) is
	// absent, ats_info will nonetheless indicate the defaults.

	snprintf(str, str_len, "CID %ssupported; NAD %ssupported",
		ats_info->CID_supported ? "" : "not ",
		ats_info->NAD_supported ? "" : "not "
	);

	return str;
}

const char* iso14443_ats_T1_get_string(const struct iso14443_ats_info_t* ats_info)
{
	if (!ats_info) {
		return NULL;
	}
	if (!ats_info->K_count) {
		return NULL;
	}

	// See ISO 7816-4:2005, 8.1.1.1, table 83
	switch (ats_info->T1) {
		case ISO14443_ATS_T1_COMPACT_TLV_SI:
			return "COMPACT-TLV followed by mandatory status indicator";

		case ISO14443_ATS_T1_DIR_DATA_REF:
			return "DIR data reference";

		case ISO14443_ATS_T1_COMPACT_TLV:
			return "COMPACT-TLV including optional status indicator";
	}

	if (ats_info->T1 > ISO14443_ATS_T1_COMPACT_TLV &&
		ats_info->T1 <= 0x8F
	) {
		return "RFU";
	}

	return "Proprietary";
}

int iso14443_atqb_parse(const uint8_t* atqb, size_t atqb_len, struct iso14443_atqb_info_t* atqb_info)
{
	int r;

	if (!atqb) {
		return -1;
	}

	if (!atqb_info) {
		return -1;
	}

	if (atqb_len < ISO14443_ATQB_MIN_SIZE || atqb_len > ISO14443_ATQB_MAX_SIZE) {
		// Invalid number of ATQB bytes
		return 1;
	}

	memset(atqb_info, 0, sizeof(*atqb_info));

	// Copy ATQB bytes
	memcpy(atqb_info->atqb, atqb, atqb_len);
	atqb_info->atqb_len = atqb_len;

	// Validate 0x50 marker byte
	// See ISO 14443-3:2011, 7.9.1
	if (atqb_info->atqb[0] != ISO14443_ATQB_MARKER) {
		return 2;
	}

	// Populate default parameters
	// These will be overridden by the parsing below
	iso14443_atqb_populate_default_parameters(atqb_info);

	// Populate Pseudo-Unique PICC Identifier (PUPI)
	// See ISO 14443-3:2011, 7.9.2
	atqb_info->pupi = &atqb_info->atqb[1];

	// Populate Application Data
	// It will be parsed once Application Data Coding (ADC) is available
	// See ISO 14443-3:2011, 7.9.3
	atqb_info->application_data = &atqb_info->atqb[5];

	// Parse Protocol Info byte 1
	// See ISO 14443-3:2011, 7.9.4.6
	atqb_info->PI1 = &atqb_info->atqb[9];
	r = iso14443_atqb_parse_PI1(*atqb_info->PI1, atqb_info);
	if (r) {
		return r;
	}

	// Parse Protocol Info byte 2
	// See ISO 14443-3:2011, 7.9.4.4 and 7.9.4.5
	atqb_info->PI2 = &atqb_info->atqb[10];
	r = iso14443_atqb_parse_PI2(*atqb_info->PI2, atqb_info);
	if (r) {
		return r;
	}

	// Parse Protocol Info byte 3
	// See ISO 14443-3:2011, 7.9.4.1 - 7.9.4.3
	atqb_info->PI3 = &atqb_info->atqb[11];
	r = iso14443_atqb_parse_PI3(*atqb_info->PI3, atqb_info);
	if (r) {
		return r;
	}

	// Parse optional Protocol Info byte 4 (extended ATQB)
	// See ISO 14443-3:2011, 7.9.4.7
	if (atqb_info->atqb_len == ISO14443_ATQB_MAX_SIZE) {
		atqb_info->PI4 = &atqb_info->atqb[12];
		r = iso14443_atqb_parse_PI4(*atqb_info->PI4, atqb_info);
		if (r) {
			return r;
		}
	}

	// Unpack Application Data subfields; requires ADC from PI(3)
	// See ISO 14443-3:2011, 7.9.3
	r = iso14443_atqb_parse_application_data(atqb_info->application_data, atqb_info);
	if (r) {
		return r;
	}

	return 0;
}

static void iso14443_atqb_populate_default_parameters(struct iso14443_atqb_info_t* atqb_info)
{
	// ISO 14443-3 indicates these default parameters when Protocol Info byte 4
	// is absent (basic ATQB):
	// - SFGI = 0 (no SFGT needed)

	// PI(4) default (see ISO 14443-3:2011, 7.9.4.7)
	iso14443_atqb_parse_PI4(0x00, atqb_info);
}

static int iso14443_atqb_parse_PI1(uint8_t PI1, struct iso14443_atqb_info_t* atqb_info)
{
	// Bit 4 is RFU and if set then interpret entire PI(1) as 0x00
	// See ISO 14443-3:2011, 7.9.4.6
	if (PI1 & ISO14443_ATQB_PI1_RFU) {
		PI1 = 0;
	}

	// Bit 8 indicates whether only the same D must be used for both directions
	// See ISO 14443-3:2011, 7.9.4.6
	atqb_info->same_d_required = (PI1 & ISO14443_ATQB_PI1_SAME_D);

	// DS (supported divisors from PICC to PCD) encoded in bits 5 to 7
	// See ISO 14443-3:2011, 7.9.4.6
	atqb_info->DS = (PI1 & ISO14443_ATQB_PI1_DS_MASK) >> ISO14443_ATQB_PI1_DS_SHIFT;

	// DR (supported divisors from PCD to PICC) encoded in bits 1 to 3
	// See ISO 14443-3:2011, 7.9.4.6
	atqb_info->DR = PI1 & ISO14443_ATQB_PI1_DR_MASK;

	return 0;
}

static int iso14443_atqb_parse_PI2(uint8_t PI2, struct iso14443_atqb_info_t* atqb_info)
{
	uint8_t FSCI = (PI2 & ISO14443_ATQB_PI2_FSCI_MASK) >> ISO14443_ATQB_PI2_FSCI_SHIFT;

	// Convert FSCI to FSC (frame size in bytes)
	// See ISO 14443-3:2011, 7.9.4.5
	// See ISO 14443-4:2008, 5.1, table 1
	// EMV Level 1 Contactless Interface Specification v3.2, 6.3.2.6-6.3.2.7, table 6.7
	switch (FSCI) {
		case 0x0: atqb_info->FSC = 16; break;
		case 0x1: atqb_info->FSC = 24; break;
		case 0x2: atqb_info->FSC = 32; break;
		case 0x3: atqb_info->FSC = 40; break;
		case 0x4: atqb_info->FSC = 48; break;
		case 0x5: atqb_info->FSC = 64; break;
		case 0x6: atqb_info->FSC = 96; break;
		case 0x7: atqb_info->FSC = 128; break;
		case 0x8: atqb_info->FSC = 256; break;
		// Although ISO 14443-3:2011, 7.9.4.5 considers FSCI values 9 to F as
		// RFU, EMV allows values 9 to C, and interprets values D to F as
		// FSCI=C (FSC=4096)
		case 0x9: atqb_info->FSC = 512; break;
		case 0xA: atqb_info->FSC = 1024; break;
		case 0xB: atqb_info->FSC = 2048; break;
		case 0xC: atqb_info->FSC = 4096; break;
		default: atqb_info->FSC = 4096; break;
	}

	// Protocol_Type bit 4 is RFU and indicates that PCD should not continue
	// communicating with PICC
	// See ISO 14443-3:2011, 7.9.4.4
	atqb_info->protocol_type_rfu = (PI2 & ISO14443_ATQB_PI2_PROTO_RFU);

	// Protocol_Type bits 2-3 encode minimum TR2
	// See ISO 14443-3:2011, 7.9.4.4
	atqb_info->min_TR2 = (PI2 & ISO14443_ATQB_PI2_PROTO_MIN_TR2_MASK) >> ISO14443_ATQB_PI2_PROTO_MIN_TR2_SHIFT;

	// Protocol_Type bit 1 indicates PICC compliant with ISO/IEC 14443-4
	// See ISO 14443-3:2011, 7.9.4.4
	atqb_info->iso14443_4_compliant = (PI2 & ISO14443_ATQB_PI2_PROTO_ISO14443_4);

	return 0;
}

static int iso14443_atqb_parse_PI3(uint8_t PI3, struct iso14443_atqb_info_t* atqb_info)
{
	// FWI encodes FWT according to ISO 14443-3:2011, 7.9.4.3
	atqb_info->FWI = (PI3 & ISO14443_ATQB_PI3_FWI_MASK) >> ISO14443_ATQB_PI3_FWI_SHIFT;
	if (atqb_info->FWI == 15) {
		// FWI = 15 is RFU and interpreted as FWI = 4
		// See ISO 14443-3:2011, 7.9.4.3
		atqb_info->FWI = 4;
	}

	// ADC encodes the Application Data coding used in the Application Data
	// field
	// See ISO 14443-3:2011, 7.9.4.2
	atqb_info->ADC = (PI3 & ISO14443_ATQB_PI3_ADC_MASK) >> ISO14443_ATQB_PI3_ADC_SHIFT;

	// FO indicates whether CID and NAD are supported by the PICC
	// See ISO 14443-3:2011, 7.9.4.1
	atqb_info->CID_supported = (PI3 & ISO14443_ATQB_PI3_FO_CID);
	atqb_info->NAD_supported = (PI3 & ISO14443_ATQB_PI3_FO_NAD);

	return 0;
}

static int iso14443_atqb_parse_PI4(uint8_t PI4, struct iso14443_atqb_info_t* atqb_info)
{
	// SFGI encodes a multiplier for SFGT according to ISO 14443-3:2011, 7.9.4.7
	atqb_info->SFGI = (PI4 & ISO14443_ATQB_PI4_SFGI_MASK) >> ISO14443_ATQB_PI4_SFGI_SHIFT;
	if (atqb_info->SFGI == 15) {
		// SFGI = 15 is RFU and interpreted as SFGI = 0
		// See ISO 14443-3:2011, 7.9.4.7
		atqb_info->SFGI = 0;
	}

	return 0;
}

static int iso14443_atqb_parse_application_data(const uint8_t* application_data, struct iso14443_atqb_info_t* atqb_info)
{
	// Application Data field layout depends on ADC
	// See ISO 14443-3:2011, 7.9.3
	if (atqb_info->ADC != ISO14443_ADC_ISO14443_3) {
		// Proprietary or RFU coding; leave extracted subfields unset
		return 0;
	}

	// Application Family Identifier (AFI)
	// See ISO 14443-3:2011, 7.9.3.1
	atqb_info->AFI = application_data[0];

	// CRC_B(AID)
	// See ISO 14443-3:2011, 7.9.3.2
	atqb_info->CRC_B_AID = application_data[1] | (application_data[2] << 8);

	// Number of Applications matching AFI and total
	// See ISO 14443-3:2011, 7.9.3.3
	atqb_info->num_apps_matching_afi = (application_data[3] & ISO14443_NUM_APPS_MATCHING_MASK) >> ISO14443_NUM_APPS_MATCHING_SHIFT;
	atqb_info->num_apps_total = application_data[3] & ISO14443_NUM_APPS_TOTAL_MASK;

	return 0;
}

const char* iso14443_atqb_application_data_get_string(const struct iso14443_atqb_info_t* atqb_info, char* str, size_t str_len)
{
	const char* family_str;
	char matching_str[8];
	char total_str[8];

	if (!atqb_info) {
		return NULL;
	}

	if (atqb_info->ADC != ISO14443_ADC_ISO14443_3) {
		// Proprietary encoding of Application Data
		snprintf(str, str_len, "%02X %02X %02X %02X (proprietary)",
			atqb_info->application_data[0],
			atqb_info->application_data[1],
			atqb_info->application_data[2],
			atqb_info->application_data[3]
		);
		return str;
	}

	// See ISO 14443-3:2011, table 22
	switch (atqb_info->AFI & ISO14443_AFI_FAMILY_MASK) {
		case 0x00: family_str = "All families"; break;
		case 0x10: family_str = "Transport"; break;
		case 0x20: family_str = "Financial"; break;
		case 0x30: family_str = "Identification"; break;
		case 0x40: family_str = "Telecommunication"; break;
		case 0x50: family_str = "Medical"; break;
		case 0x60: family_str = "Multimedia"; break;
		case 0x70: family_str = "Gaming"; break;
		case 0x80: family_str = "Data Storage"; break;
		case 0xE0: family_str = "Machine Readable Travel Documents"; break;
		default: family_str = "RFU"; break;
	}

	// See ISO 14443-3:2011, 7.9.3.3
	if (atqb_info->num_apps_matching_afi == ISO14443_NUM_APPS_MANY) {
		snprintf(matching_str, sizeof(matching_str), "15+");
	} else {
		snprintf(matching_str, sizeof(matching_str), "%u", atqb_info->num_apps_matching_afi);
	}
	if (atqb_info->num_apps_total == ISO14443_NUM_APPS_MANY) {
		snprintf(total_str, sizeof(total_str), "15+");
	} else {
		snprintf(total_str, sizeof(total_str), "%u", atqb_info->num_apps_total);
	}

	snprintf(str, str_len,
		"AFI=%02X (%s); CRC_B(AID)=%04X; Applications: %s matching / %s total",
		atqb_info->AFI,
		family_str,
		atqb_info->CRC_B_AID,
		matching_str,
		total_str
	);

	return str;
}

const char* iso14443_atqb_PI1_get_string(const struct iso14443_atqb_info_t* atqb_info, char* str, size_t str_len)
{
	if (!atqb_info) {
		return NULL;
	}

	snprintf(str, str_len,
		"DS=106%s%s%s kbit/s; DR=106%s%s%s kbit/s%s",
		atqb_info->DS & ISO14443_D2_SUPPORTED ? ", 212" : "",
		atqb_info->DS & ISO14443_D4_SUPPORTED ? ", 424" : "",
		atqb_info->DS & ISO14443_D8_SUPPORTED ? ", 848" : "",
		atqb_info->DR & ISO14443_D2_SUPPORTED ? ", 212" : "",
		atqb_info->DR & ISO14443_D4_SUPPORTED ? ", 424" : "",
		atqb_info->DR & ISO14443_D8_SUPPORTED ? ", 848" : "",
		atqb_info->same_d_required ? "; same D required" : ""
	);

	return str;
}

const char* iso14443_atqb_PI2_get_string(const struct iso14443_atqb_info_t* atqb_info, char* str, size_t str_len)
{
	const char* min_TR2_str;

	if (!atqb_info) {
		return NULL;
	}

	// See ISO 14443-3:2011, 7.9.4.4, table 27
	switch (atqb_info->min_TR2) {
		case 0: min_TR2_str = "10 etu + 32/fs"; break;
		case 1: min_TR2_str = "10 etu + 128/fs"; break;
		case 2: min_TR2_str = "10 etu + 256/fs"; break;
		case 3: min_TR2_str = "10 etu + 512/fs"; break;
		default: min_TR2_str = "unknown"; break;
	}

	snprintf(str, str_len, "FSC=%u; min_TR2=%s%s%s",
		atqb_info->FSC,
		min_TR2_str,
		atqb_info->iso14443_4_compliant ? "; ISO 14443-4 compliant PICC" : "",
		atqb_info->protocol_type_rfu ? "; PCD should not continue" : ""
	);

	return str;
}

const char* iso14443_atqb_PI3_get_string(const struct iso14443_atqb_info_t* atqb_info, char* str, size_t str_len)
{
	const char* adc_str;

	if (!atqb_info) {
		return NULL;
	}

	// See ISO 14443-3:2011, 7.9.4.2
	switch (atqb_info->ADC) {
		case ISO14443_ADC_PROPRIETARY: adc_str = "proprietary"; break;
		case ISO14443_ADC_ISO14443_3: adc_str = "ISO 14443-3"; break;
		default: adc_str = "RFU"; break;
	}

	snprintf(str, str_len,
		"FWI=%u; ADC=%u (%s); CID %ssupported; NAD %ssupported",
		atqb_info->FWI,
		atqb_info->ADC,
		adc_str,
		atqb_info->CID_supported ? "" : "not ",
		atqb_info->NAD_supported ? "" : "not "
	);

	return str;
}

const char* iso14443_atqb_PI4_get_string(const struct iso14443_atqb_info_t* atqb_info, char* str, size_t str_len)
{
	if (!atqb_info) {
		return NULL;
	}

	// NOTE: It is not necessary to check atqb_info->PI4 here. Even if PI(4) is
	// absent, atqb_info will nonetheless indicate the defaults.

	snprintf(str, str_len, "SFGI=%u", atqb_info->SFGI);

	return str;
}
