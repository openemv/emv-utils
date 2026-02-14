/**
 * @file emv-tool.c
 * @brief Simple EMV processing tool
 *
 * Copyright 2021, 2023-2026 Leon Lynch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "emv.h"
#include "pcsc.h"
#include "print_helpers.h"
#include "emv_ttl.h"
#include "emv_tags.h"
#include "emv_fields.h"
#include "emv_strings.h"
#include "emv_tlv.h"
#include "emv_app.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_APP
#include "emv_debug.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <argp.h>

// Forward declarations
struct emv_txn_t;

// Helper functions
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state);
static const char* pcsc_get_reader_state_string(unsigned int reader_state);
static void print_pcsc_readers(pcsc_ctx_t pcsc);
static void emv_txn_load_params(struct emv_ctx_t* emv, uint32_t txn_seq_cnt, uint8_t txn_type, uint32_t amount, uint32_t amount_other);
static void emv_txn_load_config(struct emv_ctx_t* emv);

// argp option keys
enum emv_tool_param_t {
	EMV_TOOL_PARAM_ODA = -255, // Negative value to avoid short options
	EMV_TOOL_PARAM_TXN_DATE,
	EMV_TOOL_PARAM_TXN_TIME,
	EMV_TOOL_PARAM_TXN_TYPE,
	EMV_TOOL_PARAM_TXN_AMOUNT,
	EMV_TOOL_PARAM_TXN_AMOUNT_OTHER,
	EMV_TOOL_PARAM_DEBUG_VERBOSE,
	EMV_TOOL_PARAM_DEBUG_SOURCES_MASK,
	EMV_TOOL_PARAM_DEBUG_LEVEL,
	EMV_TOOL_VERSION,
	EMV_TOOL_OVERRIDE_ISOCODES_PATH,
	EMV_TOOL_OVERRIDE_MCC_JSON,
};

// argp option structure
static struct argp_option argp_options[] = {
	{ NULL, 0, NULL, 0, "EMV configuration options", 1 },
	{ "oda", EMV_TOOL_PARAM_ODA, "SDA,DDA,CDA", 0, "Comma separated list of supported Offline Data Authentication (ODA) methods. Default is SDA,DDA,CDA." },

	{ NULL, 0, NULL, 0, "Transaction parameters", 2 },
	{ "txn-date", EMV_TOOL_PARAM_TXN_DATE, "YYYY-MM-DD", 0, "Transaction date (YYYY-MM-DD). Default is current date." },
	{ "txn-time", EMV_TOOL_PARAM_TXN_TIME, "hh:mm:ss", 0, "Transaction time (hh:mm:ss). Default is current time." },
	{ "txn-type", EMV_TOOL_PARAM_TXN_TYPE, "VALUE", 0, "Transaction type (two numeric digits, according to ISO 8583:1987 Processing Code)" },
	{ "txn-amount", EMV_TOOL_PARAM_TXN_AMOUNT, "AMOUNT", 0, "Transaction amount (without decimal separator)" },
	{ "txn-amount-other", EMV_TOOL_PARAM_TXN_AMOUNT_OTHER, "AMOUNT", 0, "Secondary transaction amount associated with cashback (without decimal separator)" },

	{ NULL, 0, NULL, 0, "Debug options", 3 },
	{ "debug-verbose", EMV_TOOL_PARAM_DEBUG_VERBOSE, NULL, 0, "Enable verbose debug output. This will include the timestamp, debug source and debug level in the debug output." },
	{ "debug-source", EMV_TOOL_PARAM_DEBUG_SOURCES_MASK, "x,y,z...", 0, "Comma separated list of debug sources. Allowed values are TTL, TAL, ODA, EMV, APP, ALL. Default is ALL." },
	{ "debug-level", EMV_TOOL_PARAM_DEBUG_LEVEL, "LEVEL", 0, "Maximum debug level. Allowed values are NONE, ERROR, INFO, CARD, TRACE, ALL. Default is INFO." },

	{ "version", EMV_TOOL_VERSION, NULL, 0, "Display emv-utils version" },

	// Hidden options for testing
	{ "isocodes-path", EMV_TOOL_OVERRIDE_ISOCODES_PATH, "path", OPTION_HIDDEN, "Override directory path of iso-codes JSON files" },
	{ "mcc-json", EMV_TOOL_OVERRIDE_MCC_JSON, "path", OPTION_HIDDEN, "Override path of mcc-codes JSON file" },

	{ 0 },
};

// argp configuration
static struct argp argp_config = {
	argp_options,
	argp_parser_helper,
	NULL,
	"Perform EMV transaction",
};

// EMV configuration
static uint8_t term_caps_sec =
	EMV_TERM_CAPS_SECURITY_SDA |
	EMV_TERM_CAPS_SECURITY_DDA |
	EMV_TERM_CAPS_SECURITY_CDA;

// Transaction parameters
static uint8_t txn_date[3] = { 0xFF }; // Default is current date
static uint8_t txn_time[3] = { 0xFF }; // Default is current time
static uint8_t txn_type = EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES;
static uint32_t txn_amount = 0;
static uint32_t txn_amount_other = 0;

// Debug parameters
static bool debug_verbose = false;
static struct {
	const char* str;
	enum emv_debug_source_t src;
} debug_source_opt[] = {
	{ "TTL", EMV_DEBUG_SOURCE_TTL },
	{ "TAL", EMV_DEBUG_SOURCE_TAL },
	{ "ODA", EMV_DEBUG_SOURCE_ODA },
	{ "EMV", EMV_DEBUG_SOURCE_EMV },
	{ "APP", EMV_DEBUG_SOURCE_APP },
	{ "ALL", EMV_DEBUG_SOURCE_ALL },
};
static unsigned int debug_sources_mask = EMV_DEBUG_SOURCE_ALL;
static const char* debug_level_str[] = {
	"NONE",
	"ERROR",
	"INFO",
	"CARD",
	"TRACE",
	"ALL",
};
static enum emv_debug_level_t debug_level = EMV_DEBUG_LEVEL_INFO;

// Testing parameters
static char* isocodes_path = NULL;
static char* mcc_json = NULL;


static error_t argp_parser_helper(int key, char* arg, struct argp_state* state)
{
	switch (key) {
		case EMV_TOOL_PARAM_ODA: {
			const char* str;

			// Parse comma separated list and set the appropriate security
			// capability bits in the Terminal Capabilities (field 9F33)
			term_caps_sec = 0;
			while ((str = strtok(arg, ","))) {
				if (strcasecmp(str, "SDA") == 0) {
					term_caps_sec |= EMV_TERM_CAPS_SECURITY_SDA;
				}
				if (strcasecmp(str, "DDA") == 0) {
					term_caps_sec |= EMV_TERM_CAPS_SECURITY_DDA;
				}
				if (strcasecmp(str, "CDA") == 0) {
					term_caps_sec |= EMV_TERM_CAPS_SECURITY_CDA;
				}
			}

			return 0;
		}

		case EMV_TOOL_PARAM_TXN_DATE: {
			int r;
			int year;
			int year_short;
			int month;
			int day;
			r = sscanf(arg, "%4d-%2d-%2d", &year, &month, &day);
			if (r != 3) {
				argp_error(state, "Transaction date (--txn-date) argument must be YYYY-MM-DD");
			}
			if (year < 1950 || year > 2049 ||
				month < 1 || month > 12 ||
				day < 1 || day > 31
			) {
				argp_error(state, "Transaction date (--txn-date) argument must contain a valid date");
			}
			if (year < 2000) { // See EMV 4.4 Book 4, 6.7.3
				year_short = year - 1900;
			} else {
				year_short = year - 2000;
			}
			txn_date[0] = ((year_short / 10) << 4) | (year_short % 10);
			txn_date[1] = ((month / 10) << 4) | (month % 10);
			txn_date[2] = ((day / 10) << 4) | (day % 10);

			return 0;
		}

		case EMV_TOOL_PARAM_TXN_TIME: {
			int r;
			int hours;
			int minutes;
			int seconds;
			r = sscanf(arg, "%2d:%2d:%2d", &hours, &minutes, &seconds);
			if (r != 3) {
				argp_error(state, "Transaction time (--txn-time) argument must be hh:mm:ss");
			}
			if (hours < 0 || hours > 23 ||
				minutes < 0 || minutes > 59 ||
				seconds < 0 || seconds > 59
			) {
				argp_error(state, "Transaction time (--txn-time) argument must contain a valid time");
			}
			txn_time[0] = ((hours / 10) << 4) | (hours % 10);
			txn_time[1] = ((minutes / 10) << 4) | (minutes % 10);
			txn_time[2] = ((seconds / 10) << 4) | (seconds % 10);

			return 0;
		}

		case EMV_TOOL_PARAM_TXN_TYPE: {
			size_t arg_len = strlen(arg);
			unsigned long value;
			char* endptr = arg;

			if (arg_len != 2) {
				argp_error(state, "Transaction type (--txn-type) argument must be 2 numeric digits");
			}

			// Transaction Type (field 9C) is EMV format "n", so parse as hex
			value = strtoul(arg, &endptr, 16);
			if (!arg[0] || *endptr) {
				argp_error(state, "Transaction type (--txn-type) argument must be 2 numeric digits");
			}
			txn_type = value;

			return 0;
		}

		case EMV_TOOL_PARAM_TXN_AMOUNT: {
			size_t arg_len = strlen(arg);
			unsigned long value;
			char* endptr = arg;

			if (arg_len == 0) {
				argp_error(state, "Transaction amount (--txn-amount) argument must be numeric digits");
			}

			// Amount, Authorised (field 81) is EMV format "b", so parse as decimal
			value = strtoul(arg, &endptr, 10);
			if (!arg[0] || *endptr) {
				argp_error(state, "Transaction amount (--txn-amount) argument must be numeric digits");
			}

			if (value > 0xFFFFFFFF) {
				argp_error(state, "Transaction amount (--txn-amount) argument must fit in a 32-bit field");
			}
			txn_amount = value;

			return 0;
		}

		case EMV_TOOL_PARAM_TXN_AMOUNT_OTHER: {
			size_t arg_len = strlen(arg);
			unsigned long value;
			char* endptr = arg;

			if (arg_len == 0) {
				argp_error(state, "Secondary transaction amount (--txn-amount-other) argument must be numeric digits");
			}

			// Amount, Other (field 9F04) is EMV format "b", so parse as decimal
			value = strtoul(arg, &endptr, 10);
			if (!arg[0] || *endptr) {
				argp_error(state, "Secondary transaction amount (--txn-amount-other) argument must be numeric digits");
			}

			if (value > 0xFFFFFFFF) {
				argp_error(state, "Secondary transaction amount (--txn-amount-other) argument must fit in a 32-bit field");
			}
			txn_amount_other = value;

			return 0;
		}

		case EMV_TOOL_PARAM_DEBUG_VERBOSE: {
			debug_verbose = true;
			return 0;
		}

		case EMV_TOOL_PARAM_DEBUG_SOURCES_MASK: {
			debug_sources_mask = 0;
			const char* str;

			// Parse comma separated list
			while ((str = strtok(arg, ","))) {
				size_t i;
				for (i = 0; i < sizeof(debug_source_opt) / sizeof(debug_source_opt[0]); ++i) {
					if (strcasecmp(str, debug_source_opt[i].str) == 0) {
						break;
					}
				}

				if (i >= sizeof(debug_source_opt) / sizeof(debug_source_opt[0])) {
					// Failed to find debug source string in list
					argp_error(state, "Unknown debug source (--debug-source) argument \"%s\"", str);
				}

				// Found debug source string in list; set in mask
				debug_sources_mask |= debug_source_opt[i].src;

				arg = NULL;
			}

			return 0;
		}

		case EMV_TOOL_PARAM_DEBUG_LEVEL: {
			for (size_t i = 0; i < sizeof(debug_level_str) / sizeof(debug_level_str[0]); ++i) {
				if (strcasecmp(arg, debug_level_str[i]) == 0) {
					// Found debug level string in list; use index as debug level
					debug_level = i;
					return 0;
				}
			}

			// Failed to find debug level string in list
			argp_error(state, "Unknown debug level (--debug-level) argument \"%s\"", arg);
		}

		case EMV_TOOL_VERSION: {
			const char* version;

			version = emv_lib_version_string();
			if (version) {
				printf("%s\n", version);
			} else {
				printf("Unknown\n");
			}
			exit(EXIT_SUCCESS);
			return 0;
		}

		case EMV_TOOL_OVERRIDE_ISOCODES_PATH: {
			isocodes_path = strdup(arg);
			return 0;
		}

		case EMV_TOOL_OVERRIDE_MCC_JSON: {
			mcc_json = strdup(arg);
			return 0;
		}

		default:
			return ARGP_ERR_UNKNOWN;
	}
}

static const char* pcsc_get_reader_state_string(unsigned int reader_state)
{
	if (reader_state & PCSC_STATE_UNAVAILABLE) {
		return "Status unavailable";
	}
	if (reader_state & PCSC_STATE_EMPTY) {
		return "No card";
	}
	if (reader_state & PCSC_STATE_PRESENT) {
		if (reader_state & PCSC_STATE_MUTE) {
			return "Unresponsive card";
		}
		if (reader_state & PCSC_STATE_UNPOWERED) {
			return "Unpowered card";
		}

		return "Card present";
	}

	return NULL;
}

static void print_pcsc_readers(pcsc_ctx_t pcsc)
{
	int r;
	size_t pcsc_count;

	pcsc_count = pcsc_get_reader_count(pcsc);
	if (!pcsc_count) {
		// Nothing to print
		return;
	}

	printf("\nPC/SC readers:\n");
	for (size_t i = 0; i < pcsc_count; ++i) {
		pcsc_reader_ctx_t reader;
		bool print_features;
		bool print_properties;
		unsigned int reader_state;
		const char* reader_state_str;

		reader = pcsc_get_reader(pcsc, i);
		if (!reader) {
			// Invalid reader; skip
			continue;
		}
		printf("Reader %zu: %s\n", i, pcsc_reader_get_name(reader));

		// Recognised features
		struct {
			unsigned int feature;
			const char* str;
		} feature_list[] = {
			{ PCSC_FEATURE_VERIFY_PIN_DIRECT, "PIN verification" },
			{ PCSC_FEATURE_MODIFY_PIN_DIRECT, "PIN modification" },
			{ PCSC_FEATURE_MCT_READER_DIRECT, "MCT direct" },
			{ PCSC_FEATURE_MCT_UNIVERSAL, "MCT universal" },
		};

		// Determine whether there are any recognisable features
		print_features = false;
		for (size_t j = 0; j < sizeof(feature_list) / sizeof(feature_list[0]); ++j) {
			if (pcsc_reader_has_feature(reader, feature_list[j].feature)) {
				print_features = true;
				break;
			}
		}

		// If so, print recognised features
		if (print_features) {
			bool feature_comma = false;

			printf("\tFeatures: ");
			for (size_t j = 0; j < sizeof(feature_list) / sizeof(feature_list[0]); ++j) {
				if (pcsc_reader_has_feature(reader, feature_list[j].feature)) {
					if (feature_comma) {
						printf(", ");
					} else {
						feature_comma = true;
					}

					printf("%s", feature_list[j].str);
				}
			}
			printf("\n");
		}

		// Recognised properties
		unsigned int property_list[] = {
			PCSC_PROPERTY_wLcdLayout,
			PCSC_PROPERTY_wLcdMaxCharacters,
			PCSC_PROPERTY_wLcdMaxLines,
			PCSC_PROPERTY_bMinPINSize,
			PCSC_PROPERTY_bMaxPINSize,
			PCSC_PROPERTY_wIdVendor,
			PCSC_PROPERTY_wIdProduct,
		};

		// Determine whether there are any recognisable properties
		print_properties = false;
		for (size_t j = 0; j < sizeof(property_list) / sizeof(property_list[0]); ++j) {
			uint8_t value[256];
			size_t value_len = sizeof(value);
			r = pcsc_reader_get_property(reader, property_list[j], value, &value_len);
			if (r == 0) {
				print_properties = true;
				break;
			}
		}

		// If so, print recognised properties
		if (print_properties) {
			bool property_comma = false;

			printf("\tProperties: ");

			{
				uint8_t value[2];
				size_t value_len = sizeof(value);

				r = pcsc_reader_get_property(reader, PCSC_PROPERTY_wLcdLayout, value, &value_len);
				if (r == 0 && value_len == 2) {
					if (property_comma) {
						printf(", ");
					}

					if (value[0] || value[1]) {
						printf("LCD %u x %u", value[0], value[1]);
					} else {
						printf("No LCD");
					}

					property_comma = true;
				}
			}

			{
				uint16_t wLcdMaxCharacters;
				size_t wLcdMaxCharacters_len = sizeof(wLcdMaxCharacters);
				uint16_t wLcdMaxLines;
				size_t wLcdMaxLines_len = sizeof(wLcdMaxLines);

				r = pcsc_reader_get_property(
					reader,
					PCSC_PROPERTY_wLcdMaxCharacters,
					&wLcdMaxCharacters,
					&wLcdMaxCharacters_len
				);
				if (r || wLcdMaxCharacters_len != 2) {
					// If property not found or invalid, zero property value
					wLcdMaxCharacters = 0;
				}

				r = pcsc_reader_get_property(
					reader,
					PCSC_PROPERTY_wLcdMaxLines,
					&wLcdMaxLines,
					&wLcdMaxLines_len
				);
				if (r || wLcdMaxLines_len != 2) {
					// If property not found or invalid, zero property value
					wLcdMaxLines = 0;
				}

				if ((wLcdMaxCharacters && wLcdMaxLines) ||
					wLcdMaxCharacters_len || wLcdMaxLines_len
				) {
					if (property_comma) {
						printf(", ");
					}

					if (wLcdMaxCharacters && wLcdMaxLines) {
						printf("LCD %u x %u", wLcdMaxCharacters, wLcdMaxLines);
					} else {
						printf("LCD");
					}

					property_comma = true;
				}
			}

			{
				uint8_t bMinPINSize;
				size_t bMinPINSize_len = sizeof(bMinPINSize);
				uint8_t bMaxPINSize;
				size_t bMaxPINSize_len = sizeof(bMaxPINSize);

				r = pcsc_reader_get_property(
					reader,
					PCSC_PROPERTY_bMinPINSize,
					&bMinPINSize,
					&bMinPINSize_len
				);
				if (r || bMinPINSize_len != 1) {
					// If property not found or invalid, zero property value
					bMinPINSize = 0;
				}

				r = pcsc_reader_get_property(
					reader,
					PCSC_PROPERTY_bMaxPINSize,
					&bMaxPINSize,
					&bMaxPINSize_len
				);
				if (r || bMaxPINSize_len != 1) {
					// If property not found or invalid, zero property value
					bMaxPINSize = 0;
				}

				if (bMinPINSize || bMaxPINSize) {
					if (property_comma) {
						printf(", ");
					}

					if (bMaxPINSize) {
						printf("PIN size %u-%u", bMinPINSize, bMaxPINSize);
					} else {
						printf("PIN size %u+", bMinPINSize);
					}

					property_comma = true;
				}
			}

			{
				uint16_t wIdVendor;
				size_t wIdVendor_len = sizeof(wIdVendor);
				uint16_t wIdProduct;
				size_t wIdProduct_len = sizeof(wIdProduct);

				r = pcsc_reader_get_property(
					reader,
					PCSC_PROPERTY_wIdVendor,
					&wIdVendor,
					&wIdVendor_len
				);
				if (r || wIdVendor_len != 2) {
					// If property not found or invalid, zero property value
					wIdVendor = 0;
				}

				r = pcsc_reader_get_property(
					reader,
					PCSC_PROPERTY_wIdProduct,
					&wIdProduct,
					&wIdProduct_len
				);
				if (r || wIdProduct_len != 2) {
					// If property not found or invalid, zero property value
					wIdProduct = 0;
				}

				if (wIdVendor && wIdProduct) {
					if (property_comma) {
						printf(", ");
					}

					printf("USB device %04x:%04x", wIdVendor, wIdProduct);

					property_comma = true;
				}
			}

			printf("\n");
		}

		printf("\tState: ");
		reader_state = 0;
		reader_state_str = NULL;
		r = pcsc_reader_get_state(reader, &reader_state);
		if (r == 0) {
			reader_state_str = pcsc_get_reader_state_string(reader_state);
		}
		if (reader_state_str) {
			printf("%s", reader_state_str);
		} else {
			printf("Unknown");
		}
		printf("\n");
	}
}

static void emv_txn_load_params(struct emv_ctx_t* emv, uint32_t txn_seq_cnt, uint8_t txn_type, uint32_t amount, uint32_t amount_other)
{
	time_t t = time(NULL);
	struct tm* tm = localtime(&t);
	uint8_t buf[6];

	// Transaction sequence counter
	// See EMV 4.4 Book 4, 6.5.5
	emv_tlv_list_push(&emv->params, EMV_TAG_9F41_TRANSACTION_SEQUENCE_COUNTER, 4, emv_uint_to_format_n(txn_seq_cnt, buf, 4), 0);

	// Transaction date
	if (txn_date[0] == 0xFF) {
		// Use current date
		txn_date[0] = (((tm->tm_year / 10) % 10) << 4) | (tm->tm_year % 10);
		txn_date[1] = (((tm->tm_mon + 1) / 10) << 4) | ((tm->tm_mon + 1) % 10);
		txn_date[2] = ((tm->tm_mday / 10) << 4) | (tm->tm_mday % 10);
	}
	emv_tlv_list_push(&emv->params, EMV_TAG_9A_TRANSACTION_DATE, 3, txn_date, 0);

	// Transaction time
	if (txn_time[0] == 0xFF) {
		// Use current time
		txn_time[0] = ((tm->tm_hour / 10) << 4) | (tm->tm_hour % 10);
		txn_time[1] = ((tm->tm_min / 10) << 4) | (tm->tm_min % 10);
		txn_time[2] = ((tm->tm_sec / 10) << 4) | (tm->tm_sec % 10);
	}
	emv_tlv_list_push(&emv->params, EMV_TAG_9F21_TRANSACTION_TIME, 3, txn_time, 0);

	// Transaction currency
	emv_tlv_list_push(&emv->params, EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0); // Euro (978)
	emv_tlv_list_push(&emv->params, EMV_TAG_5F36_TRANSACTION_CURRENCY_EXPONENT, 1, (uint8_t[]){ 0x02 }, 0); // Currency has 2 decimal places

	// Transaction type and amount(s)
	emv_tlv_list_push(&emv->params, EMV_TAG_9C_TRANSACTION_TYPE, 1, &txn_type, 0);
	emv_tlv_list_push(&emv->params, EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, emv_uint_to_format_n(amount, buf, 6), 0);
	emv_tlv_list_push(&emv->params, EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, emv_uint_to_format_b(amount, buf, 4), 0);
	emv_tlv_list_push(&emv->params, EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC, 6, emv_uint_to_format_n(amount_other, buf, 6), 0);
	emv_tlv_list_push(&emv->params, EMV_TAG_9F04_AMOUNT_OTHER_BINARY, 4, emv_uint_to_format_b(amount_other, buf, 4), 0);
}

static void emv_txn_load_config(struct emv_ctx_t* emv)
{
	// Terminal / merchant config
	emv_tlv_list_push(&emv->config, EMV_TAG_9F01_ACQUIRER_IDENTIFIER, 6, (uint8_t[]){ 0x00, 0x01, 0x23, 0x45, 0x67, 0x89 }, 0 ); // Unique acquirer identifier
	emv_tlv_list_push(&emv->config, EMV_TAG_9F15_MCC, 2, (uint8_t[]){ 0x59, 0x99 }, 0); // Miscellaneous and Specialty Retail Stores
	emv_tlv_list_push(&emv->config, EMV_TAG_9F16_MERCHANT_IDENTIFIER, 15, (const uint8_t*)"0987654321     ", 0); // Unique merchant identifier
	emv_tlv_list_push(&emv->config, EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0); // Netherlands
	emv_tlv_list_push(&emv->config, EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0); // 10000
	emv_tlv_list_push(&emv->config, EMV_TAG_9F1C_TERMINAL_IDENTIFICATION, 8, (const uint8_t*)"TID12345", 0); // Unique location of terminal at merchant
	emv_tlv_list_push(&emv->config, EMV_TAG_9F1E_IFD_SERIAL_NUMBER, 8, (const uint8_t*)"12345678", 0); // Serial number
	emv_tlv_list_push(&emv->config, EMV_TAG_9F4E_MERCHANT_NAME_AND_LOCATION, 12, (const uint8_t*)"ACME Peanuts", 0); // Merchant Name and Location
	emv_tlv_list_push_asn1_object(&emv->config, &ASN1_OID(url), // Merchant URL
		13, (uint8_t[]){ 0x0C, 0x0B, 0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x2E, 0x63, 0x6F, 0x6D } // ASN.1 UTF-8 string "example.com"
	);
	emv_tlv_list_push_asn1_object(&emv->config, &ASN1_OID(emailAddress), // Merchant email address
		22, (uint8_t[]){ 0x0C, 0x14, 0x6A, 0x6F, 0x68, 0x6E, 0x2E, 0x64, 0x6F, 0x65, 0x40, 0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C, 0x65, 0x2E, 0x63, 0x6F, 0x6D } // ASN.1 UTF-8 string "john.doe@example.com"
	);

	// Terminal Capabilities:
	// - Card Data Input Capability: IC with Contacts
	// - CVM Capability: Plaintext offline PIN, Enciphered online PIN, Signature, Enciphered offline PIN, No CVM
	// - Security Capability: Set using --oda option. Default is SDA, DDA, CDA.
	emv_tlv_list_push(&emv->config, EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){ 0x20, 0xF8, term_caps_sec }, 0);

	// Terminal Type: Merchant attended, offline with online capability
	emv_tlv_list_push(&emv->config, EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){ 0x22 }, 0);

	// Additional Terminal Capabilities:
	// - Transaction Type Capability: Goods, Services, Cashback, Cash, Inquiry, Payment
	// - Terminal Data Input Capability: Numeric, Alphabetic and special character keys, Command keys, Function keys
	// - Terminal Data Output Capability: Attended print, Attended display, Code table 1 to 10
	emv_tlv_list_push(&emv->config, EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, 5, (uint8_t[]){ 0xFA, 0x00, 0xF0, 0xA3, 0xFF }, 0);

	// Default Dynamic Data Authentication Data Object List (DDOL)
	emv_tlv_list_push(&emv->config, EMV_TAG_9F49_DDOL, 3, (uint8_t[]){ 0x9F, 0x37, 0x04 }, 0);

	// Supported applications
	emv_tlv_list_push(&emv->supported_aids, EMV_TAG_9F06_AID, 6, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10 }, EMV_ASI_PARTIAL_MATCH); // Visa
	emv_tlv_list_push(&emv->supported_aids, EMV_TAG_9F06_AID, 7, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x10 }, EMV_ASI_EXACT_MATCH); // Visa Electron
	emv_tlv_list_push(&emv->supported_aids, EMV_TAG_9F06_AID, 7, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x20 }, EMV_ASI_EXACT_MATCH); // V Pay
	emv_tlv_list_push(&emv->supported_aids, EMV_TAG_9F06_AID, 6, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10 }, EMV_ASI_PARTIAL_MATCH); // Mastercard
	emv_tlv_list_push(&emv->supported_aids, EMV_TAG_9F06_AID, 7, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x04, 0x30, 0x60 }, EMV_ASI_PARTIAL_MATCH); // Maestro
	emv_tlv_list_push(&emv->supported_aids, EMV_TAG_9F06_AID, 5, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x25 }, EMV_ASI_PARTIAL_MATCH); // Amex

	// Random transaction selection
	emv->random_selection_percentage = 25;
	emv->random_selection_max_percentage = 50;
	emv->random_selection_threshold = 5000; // Because floor limit is 10000
}

int main(int argc, char** argv)
{
	int r;
	pcsc_ctx_t pcsc;
	size_t pcsc_count;
	pcsc_reader_ctx_t reader;
	unsigned int reader_state;
	const char* reader_state_str;
	size_t reader_idx;
	uint8_t pos_entry_mode;
	uint8_t atr[PCSC_MAX_ATR_SIZE];
	size_t atr_len = 0;
	struct emv_ttl_t ttl;
	struct emv_ctx_t emv;
	struct emv_app_list_t app_list = EMV_APP_LIST_INIT; // Candidate list
	bool application_selection_required;

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

	if (txn_type != EMV_TRANSACTION_TYPE_INQUIRY &&
		txn_amount == 0
	) {
		fprintf(stderr, "Transaction amount (--txn-amount) argument must be non-zero\n");
		argp_help(&argp_config, stdout, ARGP_HELP_STD_HELP, argv[0]);
	}

	if (txn_type == EMV_TRANSACTION_TYPE_CASHBACK &&
		txn_amount_other == 0
	) {
		fprintf(stderr, "Secondary transaction amount (--txn-amount-other) must be non-zero for cashback transaction\n");
		argp_help(&argp_config, stdout, ARGP_HELP_STD_HELP, argv[0]);
	}

	print_set_verbose(debug_verbose);

	r = emv_strings_init(isocodes_path, mcc_json);
	if (r < 0) {
		fprintf(stderr, "Failed to initialise EMV strings\n");
		return 2;
	}
	if (r > 0) {
		fprintf(stderr, "Failed to load iso-codes data or mcc-codes data; currency, country, language or MCC lookups may not be possible\n");
	}

	r = emv_debug_init(
		debug_sources_mask,
		debug_level,
		debug_verbose ? &print_emv_debug_verbose : &print_emv_debug
	);
	if (r) {
		printf("Failed to initialise EMV debugging\n");
		return 1;
	}
	emv_debug_trace_msg("Debugging enabled; debug_verbose=%d; debug_sources_mask=0x%02X; debug_level=%u", debug_verbose, debug_sources_mask, debug_level);

	r = pcsc_init(&pcsc);
	if (r) {
		printf("PC/SC initialisation failed\n");
		goto pcsc_exit;
	}

	pcsc_count = pcsc_get_reader_count(pcsc);
	if (!pcsc_count) {
		printf("No PC/SC readers detected\n");
		goto pcsc_exit;
	}

	// List readers
	print_pcsc_readers(pcsc);

	// Wait for card presentation
	printf("\nPresent card\n");
	reader_idx = PCSC_READER_ANY;
	r = pcsc_wait_for_card(pcsc, 5000, &reader_idx);
	if (r < 0) {
		printf("PC/SC error\n");
		goto pcsc_exit;
	}
	if (r > 0) {
		printf("No card; exiting\n");
		goto pcsc_exit;
	}

	reader = pcsc_get_reader(pcsc, reader_idx);
	printf("Reader %zu: %s", reader_idx, pcsc_reader_get_name(reader));
	reader_state = 0;
	reader_state_str = NULL;
	r = pcsc_reader_get_state(reader, &reader_state);
	if (r == 0) {
		reader_state_str = pcsc_get_reader_state_string(reader_state);
	}
	if (reader_state_str) {
		printf("; %s", reader_state_str);
	} else {
		printf("; Unknown state");
	}
	printf("\nCard detected\n\n");

	r = pcsc_reader_connect(reader);
	if (r < 0) {
		printf("PC/SC reader activation failed\n");
		goto pcsc_exit;
	}
	printf("Card activated\n");

	switch (r) {
		case PCSC_CARD_TYPE_CONTACT:
			pos_entry_mode = EMV_POS_ENTRY_MODE_ICC_WITH_CVV;
			break;

		case PCSC_CARD_TYPE_CONTACTLESS:
			pos_entry_mode = EMV_POS_ENTRY_MODE_CONTACTLESS_EMV;
			printf("Contactless not (yet) supported\n");
			goto pcsc_exit;

		default:
			printf("Unknown card type\n");
			goto pcsc_exit;
	}

	r = pcsc_reader_get_atr(reader, atr, &atr_len);
	if (r) {
		printf("Failed to retrieve ATR\n");
		goto pcsc_exit;
	}
	emv_debug_trace_data("ATR", atr, atr_len);

	r = emv_atr_parse(atr, atr_len);
	if (r < 0) {
		printf("ERROR: %s\n", emv_error_get_string(r));
		goto pcsc_exit;
	}
	if (r > 0) {
		printf("OUTCOME: %s\n", emv_outcome_get_string(r));
		goto pcsc_exit;
	}

	// Prepare for EMV transaction
	memset(&ttl, 0, sizeof(ttl));
	ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
	ttl.cardreader.ctx = reader;
	ttl.cardreader.trx = &pcsc_reader_trx;
	r = emv_ctx_init(&emv, &ttl);
	if (r) {
		fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
		goto pcsc_exit;
	}
	print_set_sources_from_ctx(&emv);
	emv_txn_load_config(&emv);
	emv_txn_load_params(
		&emv,
		42, // Transaction Sequence Counter
		txn_type, // Transaction Type
		txn_amount, // Transaction Amount
		txn_amount_other // Transaction Amount, Other
	);

	printf("\nTerminal config:\n");
	print_emv_tlv_list(&emv.config);

	printf("\nSupported AIDs:\n");
	print_emv_tlv_list(&emv.supported_aids);

	printf("\nTransaction parameters:\n");
	print_emv_tlv_list(&emv.params);

	printf("\nBuild candidate list\n");
	r = emv_build_candidate_list(&emv, &app_list);
	if (r < 0) {
		printf("ERROR: %s\n", emv_error_get_string(r));
		goto emv_exit;
	}
	if (r > 0) {
		printf("OUTCOME: %s\n", emv_outcome_get_string(r));
		goto emv_exit;
	}

	printf("Candidate applications:\n");
	for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
		print_emv_app(app);
	}

	application_selection_required = emv_app_list_selection_is_required(&app_list);
	if (application_selection_required) {
		printf("Cardholder selection is required\n");
	}

	do {
		unsigned int index;

		if (application_selection_required) {
			unsigned int app_count = 0;
			int r;
			char s[4]; // two digits, newline and null
			unsigned int input = 0;

			printf("\nSelect application:\n");
			for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
				++app_count;
				printf("%u - %s\n", app_count, app->display_name);
			}
			printf("Enter number: ");
			if (!fgets(s, sizeof(s), stdin)) {
				printf("Invalid input. Try again.\n");
				continue;
			}
			r = sscanf(s, "%u", &input);
			if (r != 1) {
				printf("Invalid input. Try again.\n");
				continue;
			}
			if (!input || input > app_count) {
				printf("Invalid input. Try again.\n");
				continue;
			}

			index = input - 1;

		} else {
			// Use first application
			printf("\nSelect first application\n");
			index = 0;
		}

		r = emv_select_application(&emv, &app_list, index);
		if (r < 0) {
			printf("ERROR: %s\n", emv_error_get_string(r));
			goto emv_exit;
		}
		if (r > 0) {
			printf("OUTCOME: %s\n", emv_outcome_get_string(r));
			if (r == EMV_OUTCOME_TRY_AGAIN) {
				// Return to cardholder application selection/confirmation
				// See EMV 4.4 Book 4, 11.3
				continue;
			}
			goto emv_exit;
		}
		if (!emv.selected_app) {
			fprintf(stderr, "selected_app unexpectedly NULL\n");
			goto emv_exit;
		}

		printf("\nInitiate application processing\n");
		r = emv_initiate_application_processing(&emv, pos_entry_mode);
		if (r < 0) {
			printf("ERROR: %s\n", emv_error_get_string(r));
			goto emv_exit;
		}
		if (r > 0) {
			printf("OUTCOME: %s\n", emv_outcome_get_string(r));
			if (r == EMV_OUTCOME_GPO_NOT_ACCEPTED && !emv_app_list_is_empty(&app_list)) {
				// Return to cardholder application selection/confirmation
				// See EMV 4.4 Book 4, 6.3.1
				continue;
			}
			goto emv_exit;
		}

		// Application processing successfully initiated
		break;

	} while (true);

	// Application selection has been successful and the application list
	// is no longer needed.
	emv_app_list_clear(&app_list);

	printf("\nRead application data\n");
	r = emv_read_application_data(&emv);
	if (r < 0) {
		printf("ERROR: %s\n", emv_error_get_string(r));
		goto emv_exit;
	}
	if (r > 0) {
		printf("OUTCOME: %s\n", emv_outcome_get_string(r));
		goto emv_exit;
	}

	printf("\nOffline data authentication\n");
	r = emv_offline_data_authentication(&emv);
	if (r < 0) {
		printf("ERROR: %s\n", emv_error_get_string(r));
		goto emv_exit;
	}
	if (r > 0) {
		printf("OUTCOME: %s\n", emv_outcome_get_string(r));
		goto emv_exit;
	}

	printf("\nProcessing restrictions\n");
	// HACK HACK HACK:
	// Add scheme-specific Application Version Number (field 9F09)
	// configuration because per-AID configuration is not implemented yet.
	{
		struct emv_aid_info_t info;

		r = emv_aid_get_info(
			emv.selected_app->aid->value,
			emv.selected_app->aid->length,
			&info
		);
		if (r) {
			fprintf(stderr, "emv_aid_get_info() failed; r=%d\n", r);
			goto emv_exit;
		}

		switch (info.scheme) {
			case EMV_CARD_SCHEME_VISA:
				// See Visa Terminal Acceptance Device Guide (TADG) version 3.2, January 2020, 4.6, Processing Restrictions
				emv_tlv_list_push(&emv.config, EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x00, 0xA0 }, 0);
				break;

			case EMV_CARD_SCHEME_MASTERCARD:
				// See M/Chip Requirements for Contact and Contactless, 28 November 2023, Chapter 5, Application Version Number
				emv_tlv_list_push(&emv.config, EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x00, 0x02 }, 0);
				break;

			case EMV_CARD_SCHEME_AMEX:
				// See Amex Live Terminal Parameters Guide (October 2024), 2.4
				emv_tlv_list_push(&emv.config, EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x00, 0x01 }, 0);
				break;

			default:
				// Unsupported scheme
				emv_tlv_list_push(&emv.config, EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x00, 0x00 }, 0);
				break;
		}
	}
	r = emv_processing_restrictions(&emv);
	if (r < 0) {
		printf("ERROR: %s\n", emv_error_get_string(r));
		goto emv_exit;
	}
	if (r > 0) {
		printf("OUTCOME: %s\n", emv_outcome_get_string(r));
		goto emv_exit;
	}

	printf("\nTerminal risk management\n");
	r = emv_terminal_risk_management(&emv, NULL, 0);
	if (r < 0) {
		printf("ERROR: %s\n", emv_error_get_string(r));
		goto emv_exit;
	}
	if (r > 0) {
		printf("OUTCOME: %s\n", emv_outcome_get_string(r));
		goto emv_exit;
	}

	printf("\nCard action analysis\n");
	r = emv_card_action_analysis(&emv);
	if (r < 0) {
		printf("ERROR: %s\n", emv_error_get_string(r));
		goto emv_exit;
	}
	if (r > 0) {
		printf("OUTCOME: %s\n", emv_outcome_get_string(r));
		goto emv_exit;
	}

	printf("\nICC data:\n");
	print_emv_tlv_list(&emv.icc);

	printf("\nTerminal data:\n");
	print_emv_tlv_list(&emv.terminal);

	r = pcsc_reader_disconnect(reader);
	if (r) {
		printf("PC/SC reader deactivation failed\n");
		goto emv_exit;
	}
	printf("\nCard deactivated\n");

emv_exit:
	emv_app_list_clear(&app_list);
	emv_ctx_clear(&emv);
pcsc_exit:
	pcsc_release(&pcsc);

	if (isocodes_path) {
		free(isocodes_path);
	}
	if (mcc_json) {
		free(mcc_json);
	}
}
