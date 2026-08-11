/**
 * @file emv_ats_parse_test.c
 * @brief Unit tests for EMV ATS parsing
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

#include "emv.h"
#include "emv_debug.h"

#include <stdint.h>
#include <stdio.h>

// For debug output
#include "print_helpers.h"

static const uint8_t minimal_ats[] = { 0x01 };
static const uint8_t t0_only_ats[] = { 0x02, 0x02 };
static const uint8_t tb1_only_ats[] = { 0x03, 0x22, 0x40 };
static const uint8_t hist_ats[] = { 0x04, 0x02, 0x41, 0x42 };
static const uint8_t full_ats[] = { 0x06, 0x72, 0x00, 0x40, 0x00, 0x42 };
static const uint8_t max_hist_ats[] = {
	0x11, 0x02,
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
};
static const uint8_t fsci_min_ats[] = { 0x02, 0x00 };
static const uint8_t fsci_e_ats[] = { 0x02, 0x0E };
static const uint8_t t0_rfu_ats[] = { 0x03, 0x82, 0x40 };
static const uint8_t fwi_sfgi_max_ats[] = { 0x03, 0x22, 0xEE };
static const uint8_t fwi_15_ats[] = { 0x03, 0x22, 0xF0 };
static const uint8_t sfgi_15_ats[] = { 0x03, 0x22, 0x4F };
static const uint8_t tl_too_large_ats[] = {
	0x15,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t too_many_hist_ats[] = {
	0x12, 0x02,
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
	0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
};
static const uint8_t truncated_ats[] = { 0x02, 0x22 };

int main(void)
{
	int r;

	// Enable debug output
	r = emv_debug_init(EMV_DEBUG_SOURCE_ALL, EMV_DEBUG_LEVEL_ALL, &print_emv_debug);
	if (r) {
		fprintf(stderr, "emv_debug_init() failed; r=%d\n", r);
		return 1;
	}

	printf("Testing NULL ATS\n");
	r = emv_ats_parse(NULL, 1);
	if (r >= 0) {
		fprintf(stderr, "emv_ats_parse() should fail for NULL; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing zero-length ATS\n");
	r = emv_ats_parse(minimal_ats, 0);
	if (r >= 0) {
		fprintf(stderr, "emv_ats_parse() should fail for zero length; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing minimal ATS\n");
	r = emv_ats_parse(minimal_ats, sizeof(minimal_ats));
	if (r) {
		fprintf(stderr, "emv_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with T0 only\n");
	r = emv_ats_parse(t0_only_ats, sizeof(t0_only_ats));
	if (r) {
		fprintf(stderr, "emv_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with TB(1) only\n");
	r = emv_ats_parse(tb1_only_ats, sizeof(tb1_only_ats));
	if (r) {
		fprintf(stderr, "emv_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with historical bytes\n");
	r = emv_ats_parse(hist_ats, sizeof(hist_ats));
	if (r) {
		fprintf(stderr, "emv_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing full ATS\n");
	r = emv_ats_parse(full_ats, sizeof(full_ats));
	if (r) {
		fprintf(stderr, "emv_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with maximum historical bytes\n");
	r = emv_ats_parse(max_hist_ats, sizeof(max_hist_ats));
	if (r) {
		fprintf(stderr, "emv_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with minimum FSCI\n");
	r = emv_ats_parse(fsci_min_ats, sizeof(fsci_min_ats));
	if (r) {
		fprintf(stderr, "emv_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with FSCI 'E' normalised to 'C'\n");
	r = emv_ats_parse(fsci_e_ats, sizeof(fsci_e_ats));
	if (r) {
		fprintf(stderr, "emv_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with T0 RFU bit set\n");
	r = emv_ats_parse(t0_rfu_ats, sizeof(t0_rfu_ats));
	if (r) {
		fprintf(stderr, "emv_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with maximum FWI and SFGI\n");
	r = emv_ats_parse(fwi_sfgi_max_ats, sizeof(fwi_sfgi_max_ats));
	if (r) {
		fprintf(stderr, "emv_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with FWI=15 normalised to 4\n");
	r = emv_ats_parse(fwi_15_ats, sizeof(fwi_15_ats));
	if (r) {
		fprintf(stderr, "emv_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with SFGI=15 normalised to 0\n");
	r = emv_ats_parse(sfgi_15_ats, sizeof(sfgi_15_ats));
	if (r) {
		fprintf(stderr, "emv_ats_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with invalid TL\n");
	r = emv_ats_parse(tl_too_large_ats, sizeof(tl_too_large_ats));
	if (r == 0) {
		fprintf(stderr, "emv_ats_parse() succeeded for invalid ATS\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATS with too many historical bytes\n");
	r = emv_ats_parse(too_many_hist_ats, sizeof(too_many_hist_ats));
	if (r == 0) {
		fprintf(stderr, "emv_ats_parse() succeeded for invalid ATS\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing truncated ATS\n");
	r = emv_ats_parse(truncated_ats, sizeof(truncated_ats));
	if (r == 0) {
		fprintf(stderr, "emv_ats_parse() succeeded for invalid ATS\n");
		return 1;
	}
	printf("Success\n");

	return 0;
}
