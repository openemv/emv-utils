/**
 * @file emv-decode.c
 * @brief Simple EMV decoding tool
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
#include "print_helpers.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <argp.h>

// Helper functions
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state);
static int parse_hex(const char* hex, void* bin, size_t bin_len);

// argp parsing keys
enum {
	EMV_DECODE_ATR = 1,
};

// argp option structure
static struct argp_option argp_options[] = {
	{ "atr", EMV_DECODE_ATR, "answer-to-reset", 0, "ISO 7816 Answer-To-Reset (ATR), including initial character TS" },
	{ 0 },
};

// argp configuration
static struct argp argp_config = {
	argp_options,
	argp_parser_helper,
	NULL,
	NULL,
};

// argp parser helper function
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state)
{
	int r;

	switch (key) {
		case EMV_DECODE_ATR: {
			size_t arg_len = strlen(arg);
			uint8_t atr[ISO7816_ATR_MAX_SIZE];
			size_t atr_len = arg_len / 2;
			struct iso7816_atr_info_t atr_info;

			if (atr_len < ISO7816_ATR_MIN_SIZE) {
				argp_error(state, "ATR may not have less than %u digits (thus %u bytes)", ISO7816_ATR_MIN_SIZE * 2, ISO7816_ATR_MIN_SIZE);
			}
			if (atr_len > sizeof(atr)) {
				argp_error(state, "ATR may not have more than %zu digits (thus %zu bytes)", sizeof(atr) * 2, sizeof(atr));
			}
			if (arg_len % 2 != 0) {
				argp_error(state, "ATR must have even number of digits");
			}

			r = parse_hex(arg, atr, atr_len);
			if (r) {
				argp_error(state, "ATR must must consist of hex digits");
			}

			r = iso7816_atr_parse(atr, atr_len, &atr_info);
			if (r) {
				argp_error(state, "Failed to parse ATR");
			}

			print_atr(&atr_info);

			return 0;
		}

		default:
			return ARGP_ERR_UNKNOWN;
	}
}

// Hex parser helper function
static int parse_hex(const char* hex, void* bin, size_t bin_len)
{
	size_t hex_len = bin_len * 2;

	for (size_t i = 0; i < hex_len; ++i) {
		if (!isxdigit(hex[i])) {
			return -1;
		}
	}

	while (*hex && bin_len--) {
		uint8_t* ptr = bin;

		char str[3];
		strncpy(str, hex, 2);
		str[2] = 0;

		*ptr = strtoul(str, NULL, 16);

		hex += 2;
		++bin;
	}

	return 0;
}

int main(int argc, char** argv)
{
	int r;

	if (argc == 1) {
		// No command line arguments
		argp_help(&argp_config, stdout, ARGP_HELP_STD_HELP, argv[0]);
		return 1;
	}

	r = argp_parse(&argp_config, argc, argv, 0, 0, 0);
	if (r) {
		fprintf(stderr, "Failed to parse command line\n");
		return 1;
	}

	return 0;
}
