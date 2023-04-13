/**
 * @file emv_debug_test.c
 * @brief Unit tests for EMV debug implementation
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

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_APP
#include "emv_debug.h"
#include "print_helpers.h"

#include <stdint.h>
#include <stdio.h>

int main(void)
{
	int r;
	size_t asdf = 1337;
	uint8_t foo[] = { 0xde, 0xad, 0xbe, 0xef };

	// Enable debug output
	r = emv_debug_init(EMV_DEBUG_SOURCE_ALL, EMV_DEBUG_ALL, &print_emv_debug);
	if (r) {
		fprintf(stderr, "emv_debug_init() failed; r=%d\n", r);
		return 1;
	}

	// Test whether compiler supports length modifier z both at compile-time and runtime
	emv_debug_error("asdf=%zu", asdf);

	// Test trace logging and data output
	emv_debug_trace_data("This is a trace message", foo, sizeof(foo));
}
