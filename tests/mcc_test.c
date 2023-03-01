/**
 * @file mcc_test.c
 * @brief Unit tests for Merchant Category Code (MCC) lookups
 *
 * Copyright (c) 2023 Leon Lynch
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

#include "mcc_lookup.h"
#include "mcc_config.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
	int r;
	const char* mcc_str;

	// Let unit tests use build path, not install path, for JSON file
	r = mcc_init(MCC_JSON_BUILD_PATH);
	if (r) {
		fprintf(stderr, "mcc_init() failed; r=%d\n", r);
		return 1;
	}

	mcc_str = mcc_lookup(0);
	if (mcc_str) {
		fprintf(stderr, "mcc_lookup() found unexpected MCC '%s'\n", mcc_str);
		return 1;
	}

	mcc_str = mcc_lookup(1);
	if (mcc_str) {
		fprintf(stderr, "mcc_lookup() found unexpected MCC '%s'\n", mcc_str);
		return 1;
	}

	mcc_str = mcc_lookup(5999);
	if (!mcc_str) {
		fprintf(stderr, "mcc_lookup() failed\n");
		return 1;
	}
	if (strcmp(mcc_str, "Miscellaneous and Specialty Retail Stores") != 0) {
		fprintf(stderr, "mcc_lookup() found unexpected MCC '%s'\n", mcc_str);
		return 1;
	}

	mcc_str = mcc_lookup(7629);
	if (!mcc_str) {
		fprintf(stderr, "mcc_lookup() failed\n");
		return 1;
	}
	if (strcmp(mcc_str, "Electrical And Small Appliance Repair Shops") != 0) {
		fprintf(stderr, "mcc_lookup() found unexpected MCC '%s'\n", mcc_str);
		return 1;
	}

	printf("Success\n");

	return 0;
}
