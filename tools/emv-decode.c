/**
 * @file emv-decode.c
 * @brief Simple EMV decoding tool
 *
 * Copyright (c) 2021, 2022, 2023 Leon Lynch
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

#include "iso7816.h"
#include "print_helpers.h"
#include "emv_strings.h"
#include "isocodes_lookup.h"

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
static char* arg_str = NULL;
static size_t arg_str_len = 0;

// Decoding modes
enum emv_decode_mode_t {
	EMV_DECODE_NONE = 0,
	EMV_DECODE_ATR,
	EMV_DECODE_SW1SW2,
	EMV_DECODE_BER,
	EMV_DECODE_TLV,
	EMV_DECODE_DOL,
	EMV_DECODE_TAG_LIST,
	EMV_DECODE_MCC,
	EMV_DECODE_TERM_TYPE,
	EMV_DECODE_TERM_CAPS,
	EMV_DECODE_ADDL_TERM_CAPS,
	EMV_DECODE_CVM_LIST,
	EMV_DECODE_CVM_RESULTS,
	EMV_DECODE_TVR,
	EMV_DECODE_TSI,
	EMV_DECODE_IAD,
	EMV_DECODE_TTQ,
	EMV_DECODE_CTQ,
	EMV_DECODE_AMEX_CL_READER_CAPS,
	EMV_DECODE_MASTERCARD_THIRD_PARTY_DATA,
	EMV_DECODE_FFI,
	EMV_DECODE_AMEX_ENH_CL_READER_CAPS,
	EMV_DECODE_ISO3166_1,
	EMV_DECODE_ISO4217,
	EMV_DECODE_ISO639,
	EMV_DECODE_VERSION,
	EMV_DECODE_OVERRIDE_MCC_JSON,
};
static enum emv_decode_mode_t emv_decode_mode = EMV_DECODE_NONE;

// Testing parameters
static char* mcc_json = NULL;

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
	{ "mcc", EMV_DECODE_MCC, NULL, 0, "Decode Merchant Category Code (field 9F15)" },
	{ "9F15", EMV_DECODE_MCC, NULL, OPTION_ALIAS },
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
	{ "issuer-app-data", EMV_DECODE_IAD, NULL, 0, "Decode Issuer Application Data (field 9F10)" },
	{ "9F10", EMV_DECODE_IAD, NULL, OPTION_ALIAS },
	{ "ttq", EMV_DECODE_TTQ, NULL, 0, "Decode Terminal Transaction Qualifiers (field 9F66)" },
	{ "9F66", EMV_DECODE_TTQ, NULL, OPTION_ALIAS },
	{ "ctq", EMV_DECODE_CTQ, NULL, 0, "Decode Card Transaction Qualifiers (field 9F6C)" },
	{ "9F6C", EMV_DECODE_CTQ, NULL, OPTION_ALIAS },
	{ "amex-cl-reader-caps", EMV_DECODE_AMEX_CL_READER_CAPS, NULL, 0, "Decode Amex Contactless Reader Capabilities (field 9F6D)" },
	{ "9F6D", EMV_DECODE_AMEX_CL_READER_CAPS, NULL, OPTION_ALIAS },
	{ "mastercard-third-party-data", EMV_DECODE_MASTERCARD_THIRD_PARTY_DATA, NULL, 0, "Decode Mastercard Third Party Data (field 9F6E)" },
	{ "visa-ffi", EMV_DECODE_FFI, NULL, 0, "Decode Visa Form Factor Indicator (field 9F6E)" },
	{ "amex-enh-cl-reader-caps", EMV_DECODE_AMEX_ENH_CL_READER_CAPS, NULL, 0, "Decode Amex Enhanced Contactless Reader Capabilities (field 9F6E)" },

	{ NULL, 0, NULL, 0, "Other:", 4 },
	{ "country", EMV_DECODE_ISO3166_1, NULL, 0, "Lookup country name by ISO 3166-1 alpha-2, alpha-3 or numeric code" },
	{ "iso3166-1", EMV_DECODE_ISO3166_1, NULL, OPTION_ALIAS },
	{ "currency", EMV_DECODE_ISO4217, NULL, 0, "Lookup currency name by ISO 4217 alpha-3 or numeric code" },
	{ "iso4217", EMV_DECODE_ISO4217, NULL, OPTION_ALIAS },
	{ "language", EMV_DECODE_ISO639, NULL, 0, "Lookup language name by ISO 639 alpha-2 or alpha-3 code" },
	{ "iso639", EMV_DECODE_ISO639, NULL, OPTION_ALIAS },

	{ 0, 0, NULL, 0, "OPTION may only be _one_ of the above." },
	{ 0, 0, NULL, 0, "INPUT is either a string of hex digits representing binary data, or \"-\" to read from stdin" },

	{ "version", EMV_DECODE_VERSION, NULL, 0, "Display emv-utils version" },

	// Hidden option for testing
	{ "mcc-json", EMV_DECODE_OVERRIDE_MCC_JSON, "path", OPTION_HIDDEN, "Override path of mcc-codes JSON file" },

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
			if (emv_decode_mode == EMV_DECODE_ISO3166_1 ||
				emv_decode_mode == EMV_DECODE_ISO4217 ||
				emv_decode_mode == EMV_DECODE_ISO639
			) {
				// Country, currency and language lookups use the verbatim string input
				arg_str = strdup(arg);
				arg_str_len = strlen(arg);
				return 0;
			}

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
		case EMV_DECODE_MCC:
		case EMV_DECODE_TERM_TYPE:
		case EMV_DECODE_TERM_CAPS:
		case EMV_DECODE_ADDL_TERM_CAPS:
		case EMV_DECODE_CVM_LIST:
		case EMV_DECODE_CVM_RESULTS:
		case EMV_DECODE_TVR:
		case EMV_DECODE_TSI:
		case EMV_DECODE_IAD:
		case EMV_DECODE_TTQ:
		case EMV_DECODE_CTQ:
		case EMV_DECODE_AMEX_CL_READER_CAPS:
		case EMV_DECODE_MASTERCARD_THIRD_PARTY_DATA:
		case EMV_DECODE_FFI:
		case EMV_DECODE_AMEX_ENH_CL_READER_CAPS:
		case EMV_DECODE_ISO3166_1:
		case EMV_DECODE_ISO4217:
		case EMV_DECODE_ISO639:
			if (emv_decode_mode != EMV_DECODE_NONE) {
				argp_error(state, "Only one decoding OPTION may be specified");
			}

			emv_decode_mode = key;
			return 0;

		case EMV_DECODE_VERSION: {
			printf("%s\n", EMV_UTILS_VERSION_STRING);
			exit(EXIT_SUCCESS);
			return 0;
		}

		case EMV_DECODE_OVERRIDE_MCC_JSON: {
			mcc_json = strdup(arg);
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

	r = emv_strings_init(NULL, mcc_json);
	if (r < 0) {
		fprintf(stderr, "Failed to initialise EMV strings\n");
		return 2;
	}
	if (r > 0) {
		fprintf(stderr, "Failed to find iso-codes data; currency, country and language lookups will not be possible\n");
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

		case EMV_DECODE_MCC: {
			char str[1024];

			if (data_len != 2) {
				fprintf(stderr, "Merchant Category Code (MCC) must be 4-digit numeric code\n");
				break;
			}

			r = emv_mcc_get_string(data, data_len, str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse Merchant Category Code (MCC)\n");
				break;
			}

			if (!str[0]) {
				fprintf(stderr, "Unknown\n");
				break;
			}
			printf("%s\n", str);

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

		case EMV_DECODE_IAD: {
			char str[2048];

			if (data_len > 32) {
				fprintf(stderr, "EMV Issuer Application Data (field 9F10) may be up to 32 bytes\n");
				break;
			}

			r = emv_iad_get_string_list(data, data_len, str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse EMV Issuer Application Data (field 9F10)\n");
				break;
			}
			printf("%s", str); // No \n required for string list

			break;
		}

		case EMV_DECODE_TTQ: {
			char str[2048];

			if (data_len != 4) {
				fprintf(stderr, "EMV Terminal Transaction Qualifiers (field 9F66) must be exactly 4 bytes\n");
				break;
			}

			r = emv_ttq_get_string_list(data, data_len, str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse EMV Terminal Transaction Qualifiers (field 9F66)\n");
				break;
			}
			printf("%s", str); // No \n required for string list

			break;
		}

		case EMV_DECODE_CTQ: {
			char str[2048];

			if (data_len != 2) {
				fprintf(stderr, "EMV Card Transaction Qualifiers (field 9F6C) must be exactly 2 bytes\n");
				break;
			}

			r = emv_ctq_get_string_list(data, data_len, str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse EMV Card Transaction Qualifiers (field 9F6C)\n");
				break;
			}
			printf("%s", str); // No \n required for string list

			break;
		}

		case EMV_DECODE_AMEX_CL_READER_CAPS: {
			char str[1024];

			if (data_len != 1) {
				fprintf(stderr, "Amex Contactless Reader Capabilities (field 9F6D) must be exactly 1 byte\n");
				break;
			}

			r = emv_amex_cl_reader_caps_get_string(data[0], str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse Amex Contactless Reader Capabilities (field 9F6D)\n");
				break;
			}
			printf("%s\n", str);

			break;
		}

		case EMV_DECODE_MASTERCARD_THIRD_PARTY_DATA: {
			char str[2048];

			if (data_len < 5 || data_len > 32) {
				fprintf(stderr, "Mastercard Third Party Data (field 9F6E) must be 5 to 32 bytes\n");
				break;
			}

			r = emv_mastercard_third_party_data_get_string_list(data, data_len, str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse Mastercard Third Party Data (field 9F6E)\n");
				break;
			}
			printf("%s", str); // No \n required for string list

			break;
		}

		case EMV_DECODE_FFI: {
			char str[2048];

			if (data_len != 4) {
				fprintf(stderr, "Visa Form Factor Indicator (field 9F6E) must be exactly 4 bytes\n");
				break;
			}

			r = emv_visa_form_factor_indicator_get_string_list(data, data_len, str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse Visa Form Factor Indicator (field 9F6E)\n");
				break;
			}
			printf("%s", str); // No \n required for string list

			break;
		}

		case EMV_DECODE_AMEX_ENH_CL_READER_CAPS: {
			char str[2048];

			if (data_len != 4) {
				fprintf(stderr, "Amex Enhanced Contactless Reader Capabilities (field 9F6E) must be exactly 4 bytes\n");
				break;
			}

			r = emv_amex_enh_cl_reader_caps_get_string_list(data, data_len, str, sizeof(str));
			if (r) {
				fprintf(stderr, "Failed to parse Amex Enhanced Contactless Reader Capabilities (field 9F6E)\n");
				break;
			}
			printf("%s", str); // No \n required for string list

			break;
		}

		case EMV_DECODE_ISO3166_1: {
			const char* country;

			if (arg_str_len != 2 && arg_str_len != 3) {
				fprintf(stderr, "ISO 3166-1 country code must be alpha-2, alpha-3 or 3-digit numeric code\n");
				break;
			}

			if (arg_str_len == 2) {
				country = isocodes_lookup_country_by_alpha2(arg_str);
			} else {
				country = isocodes_lookup_country_by_alpha3(arg_str);
			}
			if (!country) {
				unsigned int country_code;
				char* endptr = arg_str;

				country_code = strtoul(arg_str, &endptr, 10);
				if (!arg_str[0] || *endptr) {
					fprintf(stderr, "Invalid ISO 3166-1 country code\n");
					break;
				}

				country = isocodes_lookup_country_by_numeric(country_code);
			}

			if (!country) {
				fprintf(stderr, "Unknown\n");
				break;
			}

			printf("%s\n", country);

			break;
		}

		case EMV_DECODE_ISO4217: {
			const char* currency;

			if (arg_str_len != 3) {
				fprintf(stderr, "ISO 4217 currency code must be alpha-3 or 3-digit numeric code\n");
				break;
			}

			currency = isocodes_lookup_currency_by_alpha3(arg_str);
			if (!currency) {
				unsigned int currency_code;
				char* endptr = arg_str;

				currency_code = strtoul(arg_str, &endptr, 10);
				if (!arg_str[0] || *endptr) {
					fprintf(stderr, "Invalid ISO 4217 currency code\n");
					break;
				}

				currency = isocodes_lookup_currency_by_numeric(currency_code);
			}

			if (!currency) {
				fprintf(stderr, "Unknown\n");
				break;
			}

			printf("%s\n", currency);

			break;
		}

		case EMV_DECODE_ISO639: {
			const char* language;

			if (arg_str_len != 2 && arg_str_len != 3) {
				fprintf(stderr, "ISO 639 currency code must be alpha-2 or alpha-3 code\n");
				break;
			}

			if (arg_str_len == 2) {
				language = isocodes_lookup_language_by_alpha2(arg_str);
			} else {
				language = isocodes_lookup_language_by_alpha3(arg_str);
			}
			if (!language) {
				fprintf(stderr, "Unknown\n");
				break;
			}

			printf("%s\n", language);

			break;
		}

		case EMV_DECODE_VERSION:
		case EMV_DECODE_OVERRIDE_MCC_JSON:
			// Implemented in argp_parser_helper()
			break;
	}

	if (data) {
		free(data);
	}
	if (arg_str) {
		free(arg_str);
	}
	if (mcc_json) {
		free(mcc_json);
	}

	return 0;
}
