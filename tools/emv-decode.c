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
#include "emv_strings.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <argp.h>

// Helper functions
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state);
static int parse_hex(const char* hex, void* bin, size_t bin_len);
static void* load_from_file(FILE* file, size_t* len);

// Input data
static uint8_t* data = NULL;
static size_t data_len = 0;

// Decoding modes
enum emv_decode_mode_t {
	EMV_DECODE_NONE = 0,
	EMV_DECODE_ATR,
	EMV_DECODE_SW1SW2,
	EMV_DECODE_BER,
	EMV_DECODE_TLV,
	EMV_DECODE_DOL,
	EMV_DECODE_TAG_LIST,
	EMV_DECODE_TERM_TYPE,
	EMV_DECODE_TERM_CAPS,
	EMV_DECODE_ADDL_TERM_CAPS,
	EMV_DECODE_CVM_LIST,
	EMV_DECODE_CVM_RESULTS,
	EMV_DECODE_TVR,
	EMV_DECODE_TSI,
};
static enum emv_decode_mode_t emv_decode_mode = EMV_DECODE_NONE;

// argp option structure
static struct argp_option argp_options[] = {
	{ NULL, 0, NULL, 0, "ISO 7816:", 1 },
	{ "atr", EMV_DECODE_ATR, NULL, 0, "Decode ISO 7816 Answer-To-Reset (ATR), including initial character TS" },
	{ "sw1sw2", EMV_DECODE_SW1SW2, NULL, 0, "Decode ISO 7816 Status bytes SW1-SW2, eg 9000" },

	{ NULL, 0, NULL, 0, "TLV data:", 2 },
	{ "ber", EMV_DECODE_BER, NULL, 0, "Decode ISO 8825-1 BER encoded data" },
	{ "tlv", EMV_DECODE_TLV, NULL, 0, "Decode EMV TLV data" },
	{ "dol", EMV_DECODE_DOL, NULL, 0, "Decode EMV Data Object List (DOL)" },
	{ "tag-list", EMV_DECODE_TAG_LIST, NULL, 0, "Decode EMV Tag List" },

	{ NULL, 0, NULL, 0, "Individual EMV fields:", 3 },
	{ "term-type", EMV_DECODE_TERM_TYPE, NULL, 0, "Decode Terminal Type (field 9F35)" },
	{ "9F35", EMV_DECODE_TERM_TYPE, NULL, OPTION_ALIAS },
	{ "term-caps", EMV_DECODE_TERM_CAPS, NULL, 0, "Decode Terminal Capabilities (field 9F33)" },
	{ "9F33", EMV_DECODE_TERM_CAPS, NULL, OPTION_ALIAS },
	{ "addl-term-caps", EMV_DECODE_ADDL_TERM_CAPS, NULL, 0, "Decode Additional Terminal Capabilities (field 9F40)" },
	{ "9F40", EMV_DECODE_ADDL_TERM_CAPS, NULL, OPTION_ALIAS },
	{ "cvm-list", EMV_DECODE_CVM_LIST, NULL, 0, "Decode Cardholder Verification Method (CVM) List (field 8E)" },
	{ "8E", EMV_DECODE_CVM_LIST, NULL, OPTION_ALIAS },
	{ "cvm-results", EMV_DECODE_CVM_RESULTS, NULL, 0, "Decode Cardholder Verification Method (CVM) Results (field 9F34)" },
	{ "9F34", EMV_DECODE_CVM_RESULTS, NULL, OPTION_ALIAS },
	{ "tvr", EMV_DECODE_TVR, NULL, 0, "Decode Terminal Verification Results (field 95)" },
	{ "95", EMV_DECODE_TVR, NULL, OPTION_ALIAS },
	{ "tsi", EMV_DECODE_TSI, NULL, 0, "Decode Transaction Status Information (field 9B)" },
	{ "9B", EMV_DECODE_TSI, NULL, OPTION_ALIAS },

	{ 0, 0, NULL, 0, "OPTION may only be _one_ of the above." },
	{ 0, 0, NULL, 0, "INPUT is either a string of hex digits representing binary data, or \"-\" to read from stdin" },
	{ 0 },
};

// argp configuration
static struct argp argp_config = {
	argp_options,
	argp_parser_helper,
	"INPUT",
	"Decode data and print it in a human readable format.",
};

// argp parser helper function
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state)
{
	int r;

	switch (key) {
		case ARGP_KEY_ARG: {
			// Parse INPUT argument
			size_t arg_len = strlen(arg);

			// If INPUT is "-"
			if (arg_len == 1 && *arg == '-') {
				// Read INPUT from stdin
				data = load_from_file(stdin, &data_len);
				if (!data || !data_len) {
					argp_error(state, "Failed to read INPUT from stdin");
				}
			} else {
				// Read INPUT as hex data
				if (arg_len % 2 != 0) {
					argp_error(state, "INPUT must have even number of digits");
				}

				data_len = arg_len / 2;
				data = malloc(data_len);

				r = parse_hex(arg, data, data_len);
				if (r) {
					argp_error(state, "INPUT must must consist of hex digits");
				}
			}

			return 0;
		}

		case ARGP_KEY_NO_ARGS: {
			argp_error(state, "INPUT is missing");
			return ARGP_ERR_UNKNOWN;
		}

		case EMV_DECODE_ATR:
		case EMV_DECODE_SW1SW2:
		case EMV_DECODE_BER:
		case EMV_DECODE_TLV:
		case EMV_DECODE_DOL:
		case EMV_DECODE_TAG_LIST:
		case EMV_DECODE_TERM_TYPE:
		case EMV_DECODE_TERM_CAPS:
		case EMV_DECODE_ADDL_TERM_CAPS:
		case EMV_DECODE_CVM_LIST:
		case EMV_DECODE_CVM_RESULTS:
		case EMV_DECODE_TVR:
		case EMV_DECODE_TSI:
			if (emv_decode_mode != EMV_DECODE_NONE) {
				argp_error(state, "Only one decoding OPTION may be specified");
			}

			emv_decode_mode = key;
			return 0;

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

// File loader helper function
static void* load_from_file(FILE* file, size_t* len)
{
	const size_t block_size = 4096; // Use common page size
	void* buf = NULL;
	size_t buf_len = 0;
	size_t total_len = 0;

	if (!file) {
		*len = 0;
		return NULL;
	}

	do {
		// Grow buffer
		buf_len += block_size;
		buf = realloc(buf, buf_len);

		// Read next block
		total_len += fread(buf + total_len, 1, block_size, file);
		if (ferror(file)) {
			free(buf);
			*len = 0;
			return NULL;
		}
	} while (!feof(file));

	*len = total_len;
	return buf;
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

	switch (emv_decode_mode) {
		case EMV_DECODE_NONE: {
			// No command line arguments
			argp_help(&argp_config, stdout, ARGP_HELP_STD_HELP, argv[0]);
			break;
		}

		case EMV_DECODE_ATR: {
			struct iso7816_atr_info_t atr_info;

			if (data_len < ISO7816_ATR_MIN_SIZE) {
				fprintf(stderr, "ATR may not have less than %u digits (thus %u bytes)\n", ISO7816_ATR_MIN_SIZE * 2, ISO7816_ATR_MIN_SIZE);
				break;
			}
			if (data_len > ISO7816_ATR_MAX_SIZE) {
				fprintf(stderr, "ATR may not have more than %u digits (thus %u bytes)\n", ISO7816_ATR_MAX_SIZE * 2, ISO7816_ATR_MAX_SIZE);
				break;
			}

			r = iso7816_atr_parse(data, data_len, &atr_info);
			if (r) {
				fprintf(stderr, "Failed to parse ATR\n");
				break;
			}

			print_atr(&atr_info);
			break;
		}

		case EMV_DECODE_SW1SW2: {
			if (data_len != 2) {
				fprintf(stderr, "SW1SW2 must consist of 4 hex digits\n");
				break;
			}

			print_sw1sw2(data[0], data[1]);
			break;
		}

		case EMV_DECODE_BER: {
			print_ber_buf(data, data_len, "  ", 0);
			break;
		}

		case EMV_DECODE_TLV: {
			print_emv_buf(data, data_len, "  ", 0);
			break;
		}

		case EMV_DECODE_DOL: {
			print_emv_dol(data, data_len, "  ", 0);
			break;
		}

		case EMV_DECODE_TAG_LIST: {
			print_emv_tag_list(data, data_len, "  ", 0);
			break;
		}

		case EMV_DECODE_TERM_TYPE: {
			char str[1024];

			if (data_len != 1) {
				fprintf(stderr, "EMV Terminal Type (field 9F35) must be exactly 1 byte\n");
				break;
			}

			r = emv_term_type_get_string_list(data[0], str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse EMV Terminal Type (field 9F35)\n");
				break;
			}
			printf("%s", str); // No \n required for string list

			break;
		}

		case EMV_DECODE_TERM_CAPS: {
			char str[1024];

			if (data_len != 3) {
				fprintf(stderr, "EMV Terminal Capabilities (field 9F33) must be exactly 3 bytes\n");
				break;
			}

			r = emv_term_caps_get_string_list(data, data_len, str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse EMV Terminal Capabilities (field 9F33)\n");
				break;
			}
			printf("%s", str); // No \n required for string list

			break;
		}

		case EMV_DECODE_ADDL_TERM_CAPS: {
			char str[1024];

			if (data_len != 5) {
				fprintf(stderr, "EMV Additional Terminal Capabilities (field 9F40) must be exactly 5 bytes\n");
				break;
			}

			r = emv_addl_term_caps_get_string_list(data, data_len, str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse EMV Additional Terminal Capabilities (field 9F40)\n");
				break;
			}
			printf("%s", str); // No \n required for string list

			break;
		}

		case EMV_DECODE_CVM_LIST: {
			char str[2048];

			r = emv_cvm_list_get_string_list(data, data_len, str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse EMV Cardholder Verification Method (CVM) List (field 8E)\n");
				break;
			}
			printf("%s", str); // No \n required for string list

			break;
		}

		case EMV_DECODE_CVM_RESULTS: {
			char str[1024];

			if (data_len != 3) {
				fprintf(stderr, "EMV Cardholder Verification Method (CVM) Results (field 9F34) must be exactly 3 bytes\n");
				break;
			}

			r = emv_cvm_results_get_string_list(data, data_len, str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse EMV Cardholder Verification Method (CVM) Results (field 9F34)\n");
				break;
			}
			printf("%s", str); // No \n required for string list

			break;
		}

		case EMV_DECODE_TVR: {
			char str[2048];

			if (data_len != 5) {
				fprintf(stderr, "EMV Terminal Verification Results (field 95) must be exactly 5 bytes\n");
				break;
			}

			r = emv_tvr_get_string_list(data, data_len, str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse EMV Terminal Verification Results (field 95)\n");
				break;
			}
			printf("%s", str); // No \n required for string list

			break;
		}

		case EMV_DECODE_TSI: {
			char str[2048];

			if (data_len != 2) {
				fprintf(stderr, "EMV Transaction Status Information (field 9B) must be exactly 2 bytes\n");
				break;
			}

			r = emv_tsi_get_string_list(data, data_len, str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse EMV Transaction Status Information (field 9B)\n");
				break;
			}
			printf("%s", str); // No \n required for string list

			break;
		}
	}

	if (data) {
		free(data);
	}

	return 0;
}
