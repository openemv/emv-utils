/**
 * @file iso14443_ats_parse_test.c
 * @brief Unit tests for ISO 14443 ATS parsing
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

static const uint8_t minimal_ats[] = { 0x01 };
static const uint8_t t0_only_ats[] = { 0x02, 0x02 };
static const uint8_t all_interface_ats[] = { 0x05, 0x72, 0x77, 0x35, 0x03 };
static const uint8_t hist_proprietary_ats[] = { 0x05, 0x02, 0x42, 0x43, 0x44 };
static const uint8_t si_ats[] = { 0x06, 0x02, 0x00, 0x05, 0x90, 0x00 };
static const uint8_t fsci_min_ats[] = { 0x02, 0x00 };
static const uint8_t fsci_max_ats[] = { 0x02, 0x0C };
static const uint8_t fsci_e_ats[] = { 0x02, 0x0E };
static const uint8_t ta1_rfu_ats[] = { 0x03, 0x12, 0x88 };
static const uint8_t fwi_15_ats[] = { 0x03, 0x22, 0xF0 };
static const uint8_t sfgi_15_ats[] = { 0x03, 0x22, 0x4F };
static const uint8_t tl_too_large_ats[] = {
	0x15,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t tl_exceeds_buf_ats[] = { 0x0A, 0x02, 0x00 };
static const uint8_t tl_zero_ats[] = { 0x00 };
static const uint8_t truncated_ta1_ats[] = { 0x02, 0x12 };
static const uint8_t truncated_tb1_ats[] = { 0x02, 0x22 };
static const uint8_t truncated_tc1_ats[] = { 0x02, 0x42 };
static const uint8_t insufficient_si_ats[] = { 0x03, 0x02, 0x00 };

int main(void)
{
	int r;
	struct iso14443_ats_info_t ats_info;

	printf("Testing minimal ATS with defaults\n");
	r = iso14443_ats_parse(minimal_ats, sizeof(minimal_ats), &ats_info);
	if (r) {
		fprintf(stderr, "iso14443_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	if (ats_info.TL != 1) {
		fprintf(stderr, "Incorrect TL; expected 1; found %u\n", ats_info.TL);
		return 1;
	}
	if (ats_info.T0 != NULL) {
		fprintf(stderr, "T0 unexpectedly non-NULL\n");
		return 1;
	}
	if (ats_info.TA1 != NULL) {
		fprintf(stderr, "TA(1) unexpectedly non-NULL\n");
		return 1;
	}
	if (ats_info.TB1 != NULL) {
		fprintf(stderr, "TB(1) unexpectedly non-NULL\n");
		return 1;
	}
	if (ats_info.TC1 != NULL) {
		fprintf(stderr, "TC(1) unexpectedly non-NULL\n");
		return 1;
	}
	if (ats_info.K_count != 0) {
		fprintf(stderr, "Incorrect K_count; expected 0; found %u\n", ats_info.K_count);
		return 1;
	}
	if (ats_info.FSC != 32) {
		fprintf(stderr, "Incorrect FSC; expected 32; found %u\n", ats_info.FSC);
		return 1;
	}
	if (ats_info.same_d_required) {
		fprintf(stderr, "same_d_required unexpectedly true\n");
		return 1;
	}
	if (ats_info.DS != 0) {
		fprintf(stderr, "Incorrect DS; expected 0; found %u\n", ats_info.DS);
		return 1;
	}
	if (ats_info.DR != 0) {
		fprintf(stderr, "Incorrect DR; expected 0; found %u\n", ats_info.DR);
		return 1;
	}
	if (ats_info.FWI != 4) {
		fprintf(stderr, "Incorrect FWI; expected 4; found %u\n", ats_info.FWI);
		return 1;
	}
	if (ats_info.SFGI != 0) {
		fprintf(stderr, "Incorrect SFGI; expected 0; found %u\n", ats_info.SFGI);
		return 1;
	}
	if (!ats_info.CID_supported) {
		fprintf(stderr, "CID_supported unexpectedly false\n");
		return 1;
	}
	if (ats_info.NAD_supported) {
		fprintf(stderr, "NAD_supported unexpectedly true\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with T0 only\n");
	r = iso14443_ats_parse(t0_only_ats, sizeof(t0_only_ats), &ats_info);
	if (r) {
		fprintf(stderr, "iso14443_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	if (ats_info.T0 == NULL) {
		fprintf(stderr, "T0 unexpectedly NULL\n");
		return 1;
	}
	if (*ats_info.T0 != 0x02) {
		fprintf(stderr, "Incorrect T0; expected 0x02; found 0x%02X\n", *ats_info.T0);
		return 1;
	}
	if (ats_info.TA1 != NULL) {
		fprintf(stderr, "TA(1) unexpectedly non-NULL\n");
		return 1;
	}
	if (ats_info.TB1 != NULL) {
		fprintf(stderr, "TB(1) unexpectedly non-NULL\n");
		return 1;
	}
	if (ats_info.TC1 != NULL) {
		fprintf(stderr, "TC(1) unexpectedly non-NULL\n");
		return 1;
	}
	if (ats_info.K_count != 0) {
		fprintf(stderr, "Incorrect K_count; expected 0; found %u\n", ats_info.K_count);
		return 1;
	}
	if (ats_info.FSC != 32) {
		fprintf(stderr, "Incorrect FSC; expected 32; found %u\n", ats_info.FSC);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with all interface bytes\n");
	r = iso14443_ats_parse(all_interface_ats, sizeof(all_interface_ats), &ats_info);
	if (r) {
		fprintf(stderr, "iso14443_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	if (ats_info.T0 == NULL) {
		fprintf(stderr, "T0 unexpectedly NULL\n");
		return 1;
	}
	if (*ats_info.T0 != 0x72) {
		fprintf(stderr, "Incorrect T0; expected 0x72; found 0x%02X\n", *ats_info.T0);
		return 1;
	}
	if (ats_info.TA1 == NULL) {
		fprintf(stderr, "TA(1) unexpectedly NULL\n");
		return 1;
	}
	if (*ats_info.TA1 != 0x77) {
		fprintf(stderr, "Incorrect TA(1); expected 0x77; found 0x%02X\n", *ats_info.TA1);
		return 1;
	}
	if (ats_info.TB1 == NULL) {
		fprintf(stderr, "TB(1) unexpectedly NULL\n");
		return 1;
	}
	if (*ats_info.TB1 != 0x35) {
		fprintf(stderr, "Incorrect TB(1); expected 0x35; found 0x%02X\n", *ats_info.TB1);
		return 1;
	}
	if (ats_info.TC1 == NULL) {
		fprintf(stderr, "TC(1) unexpectedly NULL\n");
		return 1;
	}
	if (*ats_info.TC1 != 0x03) {
		fprintf(stderr, "Incorrect TC(1); expected 0x03; found 0x%02X\n", *ats_info.TC1);
		return 1;
	}
	if (ats_info.K_count != 0) {
		fprintf(stderr, "Incorrect K_count; expected 0; found %u\n", ats_info.K_count);
		return 1;
	}
	if (ats_info.FSC != 32) {
		fprintf(stderr, "Incorrect FSC; expected 32; found %u\n", ats_info.FSC);
		return 1;
	}
	if (ats_info.same_d_required) {
		fprintf(stderr, "same_d_required unexpectedly true\n");
		return 1;
	}
	if (ats_info.DS != 7) {
		fprintf(stderr, "Incorrect DS; expected 7; found %u\n", ats_info.DS);
		return 1;
	}
	if (ats_info.DR != 7) {
		fprintf(stderr, "Incorrect DR; expected 7; found %u\n", ats_info.DR);
		return 1;
	}
	if (ats_info.FWI != 3) {
		fprintf(stderr, "Incorrect FWI; expected 3; found %u\n", ats_info.FWI);
		return 1;
	}
	if (ats_info.SFGI != 5) {
		fprintf(stderr, "Incorrect SFGI; expected 5; found %u\n", ats_info.SFGI);
		return 1;
	}
	if (!ats_info.CID_supported) {
		fprintf(stderr, "CID_supported unexpectedly false\n");
		return 1;
	}
	if (!ats_info.NAD_supported) {
		fprintf(stderr, "NAD_supported unexpectedly false\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with proprietary historical bytes\n");
	r = iso14443_ats_parse(hist_proprietary_ats, sizeof(hist_proprietary_ats), &ats_info);
	if (r) {
		fprintf(stderr, "iso14443_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	if (ats_info.K_count != 3) {
		fprintf(stderr, "Incorrect K_count; expected 3; found %u\n", ats_info.K_count);
		return 1;
	}
	if (ats_info.T1 != 0x42) {
		fprintf(stderr, "Incorrect T1; expected 0x42; found 0x%02X\n", ats_info.T1);
		return 1;
	}
	if (ats_info.historical_bytes == NULL) {
		fprintf(stderr, "historical_bytes unexpectedly NULL\n");
		return 1;
	}
	if (ats_info.historical_bytes_len != 2) {
		fprintf(stderr, "Incorrect historical_bytes_len; expected 2; found %zu\n", ats_info.historical_bytes_len);
		return 1;
	}
	if (ats_info.status_indicator_bytes != NULL) {
		fprintf(stderr, "status_indicator_bytes unexpectedly non-NULL\n");
		return 1;
	}
	if (ats_info.status_indicator_bytes_len != 0) {
		fprintf(stderr, "Incorrect status_indicator_bytes_len; expected 0; found %zu\n", ats_info.status_indicator_bytes_len);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with status indicator\n");
	r = iso14443_ats_parse(si_ats, sizeof(si_ats), &ats_info);
	if (r) {
		fprintf(stderr, "iso14443_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	if (ats_info.K_count != 4) {
		fprintf(stderr, "Incorrect K_count; expected 4; found %u\n", ats_info.K_count);
		return 1;
	}
	if (ats_info.T1 != 0x00) {
		fprintf(stderr, "Incorrect T1; expected 0x00; found 0x%02X\n", ats_info.T1);
		return 1;
	}
	if (ats_info.historical_bytes_len != 0) {
		fprintf(stderr, "Incorrect historical_bytes_len; expected 0; found %zu\n", ats_info.historical_bytes_len);
		return 1;
	}
	if (ats_info.status_indicator_bytes == NULL) {
		fprintf(stderr, "status_indicator_bytes unexpectedly NULL\n");
		return 1;
	}
	if (ats_info.status_indicator_bytes_len != 3) {
		fprintf(stderr, "Incorrect status_indicator_bytes_len; expected 3; found %zu\n", ats_info.status_indicator_bytes_len);
		return 1;
	}
	if (ats_info.status_indicator.LCS != 0x05) {
		fprintf(stderr, "Incorrect LCS; expected 0x05; found 0x%02X\n", ats_info.status_indicator.LCS);
		return 1;
	}
	if (ats_info.status_indicator.SW1 != 0x90) {
		fprintf(stderr, "Incorrect SW1; expected 0x90; found 0x%02X\n", ats_info.status_indicator.SW1);
		return 1;
	}
	if (ats_info.status_indicator.SW2 != 0x00) {
		fprintf(stderr, "Incorrect SW2; expected 0x00; found 0x%02X\n", ats_info.status_indicator.SW2);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with FSCI=0 (FSC=16)\n");
	r = iso14443_ats_parse(fsci_min_ats, sizeof(fsci_min_ats), &ats_info);
	if (r) {
		fprintf(stderr, "iso14443_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	if (ats_info.FSC != 16) {
		fprintf(stderr, "Incorrect FSC; expected 16; found %u\n", ats_info.FSC);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with FSCI='C' (FSC=4096)\n");
	r = iso14443_ats_parse(fsci_max_ats, sizeof(fsci_max_ats), &ats_info);
	if (r) {
		fprintf(stderr, "iso14443_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	if (ats_info.FSC != 4096) {
		fprintf(stderr, "Incorrect FSC; expected 4096; found %u\n", ats_info.FSC);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with FSCI='E' normalised to 'C'\n");
	r = iso14443_ats_parse(fsci_e_ats, sizeof(fsci_e_ats), &ats_info);
	if (r) {
		fprintf(stderr, "iso14443_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	if (ats_info.FSC != 4096) {
		fprintf(stderr, "Incorrect FSC; expected 4096; found %u\n", ats_info.FSC);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with TA(1) RFU bit set\n");
	r = iso14443_ats_parse(ta1_rfu_ats, sizeof(ta1_rfu_ats), &ats_info);
	if (r) {
		fprintf(stderr, "iso14443_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	if (ats_info.TA1 == NULL) {
		fprintf(stderr, "TA(1) unexpectedly NULL\n");
		return 1;
	}
	if (*ats_info.TA1 != 0x88) {
		fprintf(stderr, "Incorrect TA(1); expected 0x88; found 0x%02X\n", *ats_info.TA1);
		return 1;
	}
	if (ats_info.same_d_required) {
		fprintf(stderr, "same_d_required unexpectedly true\n");
		return 1;
	}
	if (ats_info.DS != 0) {
		fprintf(stderr, "Incorrect DS; expected 0; found %u\n", ats_info.DS);
		return 1;
	}
	if (ats_info.DR != 0) {
		fprintf(stderr, "Incorrect DR; expected 0; found %u\n", ats_info.DR);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with FWI=15 normalised to 4\n");
	r = iso14443_ats_parse(fwi_15_ats, sizeof(fwi_15_ats), &ats_info);
	if (r) {
		fprintf(stderr, "iso14443_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	if (ats_info.FWI != 4) {
		fprintf(stderr, "Incorrect FWI; expected 4; found %u\n", ats_info.FWI);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with SFGI=15 normalised to 0\n");
	r = iso14443_ats_parse(sfgi_15_ats, sizeof(sfgi_15_ats), &ats_info);
	if (r) {
		fprintf(stderr, "iso14443_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	if (ats_info.SFGI != 0) {
		fprintf(stderr, "Incorrect SFGI; expected 0; found %u\n", ats_info.SFGI);
		return 1;
	}
	printf("Success\n");

	printf("Testing NULL ATS\n");
	r = iso14443_ats_parse(NULL, sizeof(minimal_ats), &ats_info);
	if (r >= 0) {
		fprintf(stderr, "iso14443_ats_parse() succeeded for NULL ats\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing NULL ats_info\n");
	r = iso14443_ats_parse(minimal_ats, sizeof(minimal_ats), NULL);
	if (r >= 0) {
		fprintf(stderr, "iso14443_ats_parse() succeeded for NULL ats_info\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing zero-length ATS\n");
	r = iso14443_ats_parse(minimal_ats, 0, &ats_info);
	if (r == 0) {
		fprintf(stderr, "iso14443_ats_parse() succeeded for zero-length\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with length above maximum\n");
	r = iso14443_ats_parse(tl_too_large_ats, sizeof(tl_too_large_ats), &ats_info);
	if (r == 0) {
		fprintf(stderr, "iso14443_ats_parse() succeeded for oversized buffer\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with TL=0\n");
	r = iso14443_ats_parse(tl_zero_ats, sizeof(tl_zero_ats), &ats_info);
	if (r == 0) {
		fprintf(stderr, "iso14443_ats_parse() succeeded for TL=0\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with TL greater than buffer\n");
	r = iso14443_ats_parse(tl_exceeds_buf_ats, sizeof(tl_exceeds_buf_ats), &ats_info);
	if (r == 0) {
		fprintf(stderr, "iso14443_ats_parse() succeeded for TL > buffer\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with truncated TA(1)\n");
	r = iso14443_ats_parse(truncated_ta1_ats, sizeof(truncated_ta1_ats), &ats_info);
	if (r == 0) {
		fprintf(stderr, "iso14443_ats_parse() succeeded for truncated TA(1)\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with truncated TB(1)\n");
	r = iso14443_ats_parse(truncated_tb1_ats, sizeof(truncated_tb1_ats), &ats_info);
	if (r == 0) {
		fprintf(stderr, "iso14443_ats_parse() succeeded for truncated TB(1)\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with truncated TC(1)\n");
	r = iso14443_ats_parse(truncated_tc1_ats, sizeof(truncated_tc1_ats), &ats_info);
	if (r == 0) {
		fprintf(stderr, "iso14443_ats_parse() succeeded for truncated TC(1)\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with insufficient status indicator bytes\n");
	r = iso14443_ats_parse(insufficient_si_ats, sizeof(insufficient_si_ats), &ats_info);
	if (r == 0) {
		fprintf(stderr, "iso14443_ats_parse() succeeded for insufficient SI\n");
		return 1;
	}
	printf("Success\n");

	return 0;
}
