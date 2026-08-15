/**
 * @file iso14443_atqb_parse_test.c
 * @brief Unit tests for ISO 14443 ATQB parsing
 *
 * Copyright 2026 Leon Lynch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "iso14443.h"

#include <stdint.h>
#include <stdio.h>

static const uint8_t basic_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x21, 0x71,
};

static const uint8_t extended_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x21, 0x71,
	0x40,
};

static const uint8_t all_divisors_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x77, 0x21, 0x71,
};

static const uint8_t protocol_type_rfu_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x29, 0x71,
};

static const uint8_t not_iso14443_4_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x20, 0x71,
};

static const uint8_t min_tr2_3_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x27, 0x71,
};

static const uint8_t adc_iso14443_3_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x20, 0xAA, 0xBB, 0x35,
	0x00, 0x21, 0x75,
};

static const uint8_t adc_iso14443_3_many_apps_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x20, 0xAA, 0xBB, 0xFF,
	0x00, 0x21, 0x75,
};

static const uint8_t cid_nad_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x21, 0x73,
};

static const uint8_t pi4_rfu_ignored_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x21, 0x71,
	0x45,
};

static const uint8_t fsci_d_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0xD1, 0x71,
};

static const uint8_t fsci_f_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0xF1, 0x71,
};

static const uint8_t fwi_15_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x21, 0xF1,
};

static const uint8_t sfgi_15_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x21, 0x71,
	0xF0,
};

static const uint8_t pi1_rfu_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x88, 0x21, 0x71,
};

static const uint8_t too_short_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x21,
};

static const uint8_t too_long_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x21, 0x71,
	0x40, 0x00,
};

static const uint8_t missing_marker_atqb[] = {
	0x00,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x21, 0x71,
};

int main(void)
{
	int r;
	struct iso14443_atqb_info_t atqb_info;

	printf("Testing basic ATQB layout and default parameters\n");
	r = iso14443_atqb_parse(basic_atqb, sizeof(basic_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (atqb_info.atqb_len != sizeof(basic_atqb)) {
		fprintf(stderr, "Incorrect atqb_len; expected %zu; found %zu\n", sizeof(basic_atqb), atqb_info.atqb_len);
		return 1;
	}
	if (atqb_info.pupi != &atqb_info.atqb[1]) {
		fprintf(stderr, "Incorrect pupi pointer\n");
		return 1;
	}
	if (atqb_info.application_data != &atqb_info.atqb[5]) {
		fprintf(stderr, "Incorrect application_data pointer\n");
		return 1;
	}
	if (atqb_info.PI1 != &atqb_info.atqb[9]) {
		fprintf(stderr, "Incorrect PI(1) pointer\n");
		return 1;
	}
	if (atqb_info.PI2 != &atqb_info.atqb[10]) {
		fprintf(stderr, "Incorrect PI(2) pointer\n");
		return 1;
	}
	if (atqb_info.PI3 != &atqb_info.atqb[11]) {
		fprintf(stderr, "Incorrect PI(3) pointer\n");
		return 1;
	}
	if (atqb_info.PI4 != NULL) {
		fprintf(stderr, "PI(4) unexpectedly non-NULL\n");
		return 1;
	}
	if (atqb_info.same_d_required) {
		fprintf(stderr, "same_d_required unexpectedly true\n");
		return 1;
	}
	if (atqb_info.DS != 0) {
		fprintf(stderr, "Incorrect DS; expected 0; found %u\n", atqb_info.DS);
		return 1;
	}
	if (atqb_info.DR != 0) {
		fprintf(stderr, "Incorrect DR; expected 0; found %u\n", atqb_info.DR);
		return 1;
	}
	if (atqb_info.FSC != 32) {
		fprintf(stderr, "Incorrect FSC; expected 32; found %u\n", atqb_info.FSC);
		return 1;
	}
	if (atqb_info.protocol_type_rfu) {
		fprintf(stderr, "protocol_type_rfu unexpectedly true\n");
		return 1;
	}
	if (atqb_info.min_TR2 != 0) {
		fprintf(stderr, "Incorrect min_TR2; expected 0; found %u\n", atqb_info.min_TR2);
		return 1;
	}
	if (!atqb_info.iso14443_4_compliant) {
		fprintf(stderr, "iso14443_4_compliant unexpectedly false\n");
		return 1;
	}
	if (atqb_info.FWI != 7) {
		fprintf(stderr, "Incorrect FWI; expected 7; found %u\n", atqb_info.FWI);
		return 1;
	}
	if (atqb_info.ADC != 0) {
		fprintf(stderr, "Incorrect ADC; expected 0; found %u\n", atqb_info.ADC);
		return 1;
	}
	if (!atqb_info.CID_supported) {
		fprintf(stderr, "CID_supported unexpectedly false\n");
		return 1;
	}
	if (atqb_info.NAD_supported) {
		fprintf(stderr, "NAD_supported unexpectedly true\n");
		return 1;
	}
	if (atqb_info.AFI != 0) {
		fprintf(stderr, "Incorrect AFI; expected 0; found 0x%02X\n", atqb_info.AFI);
		return 1;
	}
	if (atqb_info.CRC_B_AID != 0) {
		fprintf(stderr, "Incorrect CRC_B_AID; expected 0; found 0x%04X\n", atqb_info.CRC_B_AID);
		return 1;
	}
	if (atqb_info.num_apps_matching_afi != 0) {
		fprintf(stderr, "Incorrect num_apps_matching_afi; expected 0; found %u\n", atqb_info.num_apps_matching_afi);
		return 1;
	}
	if (atqb_info.num_apps_total != 0) {
		fprintf(stderr, "Incorrect num_apps_total; expected 0; found %u\n", atqb_info.num_apps_total);
		return 1;
	}
	if (atqb_info.SFGI != 0) {
		fprintf(stderr, "Incorrect SFGI; expected 0; found %u\n", atqb_info.SFGI);
		return 1;
	}
	printf("Success\n");

	printf("Testing extended ATQB with PI(4)\n");
	r = iso14443_atqb_parse(extended_atqb, sizeof(extended_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (atqb_info.atqb_len != sizeof(extended_atqb)) {
		fprintf(stderr, "Incorrect atqb_len; expected %zu; found %zu\n", sizeof(extended_atqb), atqb_info.atqb_len);
		return 1;
	}
	if (atqb_info.PI4 == NULL) {
		fprintf(stderr, "PI(4) unexpectedly NULL\n");
		return 1;
	}
	if (atqb_info.PI4 != &atqb_info.atqb[12]) {
		fprintf(stderr, "Incorrect PI(4) pointer\n");
		return 1;
	}
	if (atqb_info.SFGI != 4) {
		fprintf(stderr, "Incorrect SFGI; expected 4; found %u\n", atqb_info.SFGI);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with all bit-rate divisors\n");
	r = iso14443_atqb_parse(all_divisors_atqb, sizeof(all_divisors_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (atqb_info.same_d_required) {
		fprintf(stderr, "same_d_required unexpectedly true\n");
		return 1;
	}
	if (atqb_info.DS != 7) {
		fprintf(stderr, "Incorrect DS; expected 7; found %u\n", atqb_info.DS);
		return 1;
	}
	if (atqb_info.DR != 7) {
		fprintf(stderr, "Incorrect DR; expected 7; found %u\n", atqb_info.DR);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with Protocol_Type RFU bit set\n");
	r = iso14443_atqb_parse(protocol_type_rfu_atqb, sizeof(protocol_type_rfu_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (!atqb_info.protocol_type_rfu) {
		fprintf(stderr, "protocol_type_rfu unexpectedly false\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB not compliant with ISO 14443-4\n");
	r = iso14443_atqb_parse(not_iso14443_4_atqb, sizeof(not_iso14443_4_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (atqb_info.iso14443_4_compliant) {
		fprintf(stderr, "iso14443_4_compliant unexpectedly true\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with min_TR2=3\n");
	r = iso14443_atqb_parse(min_tr2_3_atqb, sizeof(min_tr2_3_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (atqb_info.min_TR2 != 3) {
		fprintf(stderr, "Incorrect min_TR2; expected 3; found %u\n", atqb_info.min_TR2);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with ADC=ISO 14443-3 and Application Data subfields\n");
	r = iso14443_atqb_parse(adc_iso14443_3_atqb, sizeof(adc_iso14443_3_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (atqb_info.ADC != 1) {
		fprintf(stderr, "Incorrect ADC; expected 1; found %u\n", atqb_info.ADC);
		return 1;
	}
	if (atqb_info.AFI != 0x20) {
		fprintf(stderr, "Incorrect AFI; expected 0x20; found 0x%02X\n", atqb_info.AFI);
		return 1;
	}
	if (atqb_info.CRC_B_AID != 0xBBAA) {
		fprintf(stderr, "Incorrect CRC_B_AID; expected 0xBBAA; found 0x%04X\n", atqb_info.CRC_B_AID);
		return 1;
	}
	if (atqb_info.num_apps_matching_afi != 3) {
		fprintf(stderr, "Incorrect num_apps_matching_afi; expected 3; found %u\n", atqb_info.num_apps_matching_afi);
		return 1;
	}
	if (atqb_info.num_apps_total != 5) {
		fprintf(stderr, "Incorrect num_apps_total; expected 5; found %u\n", atqb_info.num_apps_total);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with Number of Applications = 15+\n");
	r = iso14443_atqb_parse(adc_iso14443_3_many_apps_atqb, sizeof(adc_iso14443_3_many_apps_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (atqb_info.num_apps_matching_afi != ISO14443_NUM_APPS_MANY) {
		fprintf(stderr, "Incorrect num_apps_matching_afi; expected %u; found %u\n", ISO14443_NUM_APPS_MANY, atqb_info.num_apps_matching_afi);
		return 1;
	}
	if (atqb_info.num_apps_total != ISO14443_NUM_APPS_MANY) {
		fprintf(stderr, "Incorrect num_apps_total; expected %u; found %u\n", ISO14443_NUM_APPS_MANY, atqb_info.num_apps_total);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with CID and NAD supported\n");
	r = iso14443_atqb_parse(cid_nad_atqb, sizeof(cid_nad_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (!atqb_info.CID_supported) {
		fprintf(stderr, "CID_supported unexpectedly false\n");
		return 1;
	}
	if (!atqb_info.NAD_supported) {
		fprintf(stderr, "NAD_supported unexpectedly false\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with PI(4) low nibble RFU ignored\n");
	r = iso14443_atqb_parse(pi4_rfu_ignored_atqb, sizeof(pi4_rfu_ignored_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (atqb_info.SFGI != 4) {
		fprintf(stderr, "Incorrect SFGI; expected 4; found %u\n", atqb_info.SFGI);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with FSCI='D' normalised to 'C'\n");
	r = iso14443_atqb_parse(fsci_d_atqb, sizeof(fsci_d_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (atqb_info.FSC != 4096) {
		fprintf(stderr, "Incorrect FSC; expected 4096; found %u\n", atqb_info.FSC);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with FSCI='F' normalised to 'C'\n");
	r = iso14443_atqb_parse(fsci_f_atqb, sizeof(fsci_f_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (atqb_info.FSC != 4096) {
		fprintf(stderr, "Incorrect FSC; expected 4096; found %u\n", atqb_info.FSC);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with FWI=15 normalised to 4\n");
	r = iso14443_atqb_parse(fwi_15_atqb, sizeof(fwi_15_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (atqb_info.FWI != 4) {
		fprintf(stderr, "Incorrect FWI; expected 4; found %u\n", atqb_info.FWI);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with SFGI=15 normalised to 0\n");
	r = iso14443_atqb_parse(sfgi_15_atqb, sizeof(sfgi_15_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (atqb_info.SFGI != 0) {
		fprintf(stderr, "Incorrect SFGI; expected 0; found %u\n", atqb_info.SFGI);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with PI(1) RFU bit set\n");
	r = iso14443_atqb_parse(pi1_rfu_atqb, sizeof(pi1_rfu_atqb), &atqb_info);
	if (r) {
		fprintf(stderr, "iso14443_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	if (atqb_info.same_d_required) {
		fprintf(stderr, "same_d_required unexpectedly true\n");
		return 1;
	}
	if (atqb_info.DS != 0) {
		fprintf(stderr, "Incorrect DS; expected 0; found %u\n", atqb_info.DS);
		return 1;
	}
	if (atqb_info.DR != 0) {
		fprintf(stderr, "Incorrect DR; expected 0; found %u\n", atqb_info.DR);
		return 1;
	}
	printf("Success\n");

	printf("Testing NULL ATQB\n");
	r = iso14443_atqb_parse(NULL, sizeof(basic_atqb), &atqb_info);
	if (r >= 0) {
		fprintf(stderr, "iso14443_atqb_parse() succeeded for NULL atqb\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing NULL atqb_info\n");
	r = iso14443_atqb_parse(basic_atqb, sizeof(basic_atqb), NULL);
	if (r >= 0) {
		fprintf(stderr, "iso14443_atqb_parse() succeeded for NULL atqb_info\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing zero-length ATQB\n");
	r = iso14443_atqb_parse(basic_atqb, 0, &atqb_info);
	if (r == 0) {
		fprintf(stderr, "iso14443_atqb_parse() succeeded for zero-length\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with length below minimum\n");
	r = iso14443_atqb_parse(too_short_atqb, sizeof(too_short_atqb), &atqb_info);
	if (r == 0) {
		fprintf(stderr, "iso14443_atqb_parse() succeeded for length below minimum\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with length above maximum\n");
	r = iso14443_atqb_parse(too_long_atqb, sizeof(too_long_atqb), &atqb_info);
	if (r == 0) {
		fprintf(stderr, "iso14443_atqb_parse() succeeded for length above maximum\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with missing 0x50 marker\n");
	r = iso14443_atqb_parse(missing_marker_atqb, sizeof(missing_marker_atqb), &atqb_info);
	if (r == 0) {
		fprintf(stderr, "iso14443_atqb_parse() succeeded for missing marker\n");
		return 1;
	}
	printf("Success\n");

	return 0;
}
