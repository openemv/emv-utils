/**
 * @file emv_atqb_parse_test.c
 * @brief Unit tests for EMV ATQB parsing
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

static const uint8_t fsci_min_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x01, 0x71,
};

static const uint8_t fsci_e_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0xE1, 0x71,
};

static const uint8_t fwi_sfgi_max_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x21, 0xE1,
	0xE0,
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

static const uint8_t all_divisors_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x77, 0x21, 0x71,
};

static const uint8_t not_iso14443_4_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x20, 0x71,
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

static const uint8_t pi2_rfu_atqb[] = {
	0x50,
	0x11, 0x22, 0x33, 0x44,
	0x55, 0x66, 0x77, 0x88,
	0x00, 0x29, 0x71,
};

int main(void)
{
	int r;

	// Enable debug output
	r = emv_debug_init(EMV_DEBUG_SOURCE_ALL, EMV_DEBUG_LEVEL_ALL, &print_emv_debug);
	if (r) {
		fprintf(stderr, "emv_debug_init() failed; r=%d\n", r);
		return 1;
	}

	printf("Testing NULL ATQB\n");
	r = emv_atqb_parse(NULL, 12);
	if (r >= 0) {
		fprintf(stderr, "emv_atqb_parse() should fail for NULL; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing zero-length ATQB\n");
	r = emv_atqb_parse(basic_atqb, 0);
	if (r >= 0) {
		fprintf(stderr, "emv_atqb_parse() should fail for zero length; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing basic ATQB (12 bytes)\n");
	r = emv_atqb_parse(basic_atqb, sizeof(basic_atqb));
	if (r) {
		fprintf(stderr, "emv_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing extended ATQB (13 bytes)\n");
	r = emv_atqb_parse(extended_atqb, sizeof(extended_atqb));
	if (r) {
		fprintf(stderr, "emv_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with FSCI=0 (FSC=16, minimum)\n");
	r = emv_atqb_parse(fsci_min_atqb, sizeof(fsci_min_atqb));
	if (r) {
		fprintf(stderr, "emv_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with FSCI='E' treated as 'C'\n");
	r = emv_atqb_parse(fsci_e_atqb, sizeof(fsci_e_atqb));
	if (r) {
		fprintf(stderr, "emv_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with FWI=14 and SFGI=14 (PCD maximums)\n");
	r = emv_atqb_parse(fwi_sfgi_max_atqb, sizeof(fwi_sfgi_max_atqb));
	if (r) {
		fprintf(stderr, "emv_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with FWI=15 (treated as 4)\n");
	r = emv_atqb_parse(fwi_15_atqb, sizeof(fwi_15_atqb));
	if (r) {
		fprintf(stderr, "emv_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with SFGI=15 (treated as 0)\n");
	r = emv_atqb_parse(sfgi_15_atqb, sizeof(sfgi_15_atqb));
	if (r) {
		fprintf(stderr, "emv_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with PI(1) RFU bit set (byte normalised to 0)\n");
	r = emv_atqb_parse(pi1_rfu_atqb, sizeof(pi1_rfu_atqb));
	if (r) {
		fprintf(stderr, "emv_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with all bit-rate divisors\n");
	r = emv_atqb_parse(all_divisors_atqb, sizeof(all_divisors_atqb));
	if (r) {
		fprintf(stderr, "emv_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB not compliant with ISO 14443-4 (PCD accepts)\n");
	r = emv_atqb_parse(not_iso14443_4_atqb, sizeof(not_iso14443_4_atqb));
	if (r) {
		fprintf(stderr, "emv_atqb_parse() failed; r=%d\n", r);
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with length < 12\n");
	r = emv_atqb_parse(too_short_atqb, sizeof(too_short_atqb));
	if (r == 0) {
		fprintf(stderr, "emv_atqb_parse() succeeded for invalid ATQB\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with length > 13\n");
	r = emv_atqb_parse(too_long_atqb, sizeof(too_long_atqb));
	if (r == 0) {
		fprintf(stderr, "emv_atqb_parse() succeeded for invalid ATQB\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with missing 0x50 marker\n");
	r = emv_atqb_parse(missing_marker_atqb, sizeof(missing_marker_atqb));
	if (r == 0) {
		fprintf(stderr, "emv_atqb_parse() succeeded for invalid ATQB\n");
		return 1;
	}
	printf("Success\n");

	printf("Testing ATQB with Protocol_Type RFU bit set\n");
	r = emv_atqb_parse(pi2_rfu_atqb, sizeof(pi2_rfu_atqb));
	if (r == 0) {
		fprintf(stderr, "emv_atqb_parse() succeeded for invalid ATQB\n");
		return 1;
	}
	printf("Success\n");

	return 0;
}
