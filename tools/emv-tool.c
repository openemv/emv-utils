/**
 * @file emv-tool.c
 * @brief Simple EMV processing tool
 *
 * Copyright (c) 2021, 2023-2024 Leon Lynch
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
#include "pcsc.h"
#include "print_helpers.h"
#include "emv_tags.h"
#include "emv_fields.h"
#include "emv_strings.h"
#include "emv_tlv.h"
#include "emv_app.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_APP
#include "emv_debug.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <argp.h>

// HACK: remove
#include "emv_ttl.h"

// Forward declarations
struct emv_txn_t;

// Helper functions
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state);
static const char* pcsc_get_reader_state_string(unsigned int reader_state);
static void emv_txn_load_params(struct emv_txn_t* emv_txn, uint32_t txn_seq_cnt, uint8_t txn_type, uint32_t amount, uint32_t amount_other);
static void emv_txn_load_config(struct emv_txn_t* emv_txn);
static void emv_txn_destroy(struct emv_txn_t* emv_txn);

// argp option keys
enum emv_tool_param_t {
	EMV_TOOL_PARAM_TXN_TYPE = 1,
	EMV_TOOL_PARAM_TXN_AMOUNT,
	EMV_TOOL_PARAM_TXN_AMOUNT_OTHER,
	EMV_TOOL_PARAM_DEBUG_VERBOSE,
	EMV_TOOL_PARAM_DEBUG_SOURCES_MASK,
	EMV_TOOL_PARAM_DEBUG_LEVEL,
	EMV_TOOL_VERSION,
	EMV_TOOL_OVERRIDE_MCC_JSON,
};

// argp option structure
static struct argp_option argp_options[] = {
	{ NULL, 0, NULL, 0, "Transaction parameters", 1 },
	{ "txn-type", EMV_TOOL_PARAM_TXN_TYPE, "VALUE", 0, "Transaction type (two numeric digits, according to ISO 8583:1987 Processing Code)" },
	{ "txn-amount", EMV_TOOL_PARAM_TXN_AMOUNT, "AMOUNT", 0, "Transaction amount (without decimal separator)" },
	{ "txn-amount-other", EMV_TOOL_PARAM_TXN_AMOUNT_OTHER, "AMOUNT", 0, "Secondary transaction amount associated with cashback (without decimal separator)" },

	{ NULL, 0, NULL, 0, "Debug options", 2 },
	{ "debug-verbose", EMV_TOOL_PARAM_DEBUG_VERBOSE, NULL, 0, "Enable verbose debug output. This will include the timestamp, debug source and debug level in the debug output." },
	{ "debug-source", EMV_TOOL_PARAM_DEBUG_SOURCES_MASK, "x,y,z...", 0, "Comma separated list of debug sources. Allowed values are TTL, TAL, EMV, APP, ALL. Default is ALL." },
	{ "debug-level", EMV_TOOL_PARAM_DEBUG_LEVEL, "LEVEL", 0, "Maximum debug level. Allowed values are NONE, ERROR, INFO, CARD, TRACE, ALL. Default is INFO." },

	{ "version", EMV_TOOL_VERSION, NULL, 0, "Display emv-utils version" },

	// Hidden option for testing
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

// Transaction parameters
static uint8_t txn_type = EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES;
static uint32_t txn_amount = 0;
static uint32_t txn_amount_other = 0;

// Debug parameters
static bool debug_verbose = false;
static const char* debug_source_str[] = {
	"TTL",
	"TAL",
	"EMV",
	"APP",
	"ALL",
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
static enum emv_debug_level_t debug_level = EMV_DEBUG_INFO;

// Testing parameters
static char* mcc_json = NULL;

struct emv_txn_t {
	// Terminal Transport Layer
	struct emv_ttl_t ttl;

	// Transaction parameters (type, amount, counter, etc)
	struct emv_tlv_list_t params;

	// Terminal configuration
	struct emv_tlv_list_t terminal;

	// Supported applications
	struct emv_tlv_list_t supported_aids;

	// ICC data
	struct emv_tlv_list_t icc;

	// Cardholder application selection required?
	bool application_selection_required;

	// Selected application
	struct emv_app_t* selected_app;
};
static struct emv_txn_t emv_txn;


static error_t argp_parser_helper(int key, char* arg, struct argp_state* state)
{
	switch (key) {
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
				for (i = 0; i < sizeof(debug_source_str) / sizeof(debug_source_str[0]); ++i) {
					if (strcasecmp(str, debug_source_str[i]) == 0) {
						break;
					}
				}

				if (i >= sizeof(debug_source_str) / sizeof(debug_source_str[0])) {
					// Failed to find debug source string in list
					argp_error(state, "Unknown debug source (--debug-source) argument \"%s\"", str);
				}

				// Found debug source string in list; shift into mask
				debug_sources_mask |= 1 << i;

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

static void emv_txn_load_params(struct emv_txn_t* emv_txn, uint32_t txn_seq_cnt, uint8_t txn_type, uint32_t amount, uint32_t amount_other)
{
	time_t t = time(NULL);
	struct tm* tm = localtime(&t);
	uint8_t emv_date[3];
	uint8_t emv_time[3];
	int date_offset = 0;
	uint8_t buf[6];

	// Transaction sequence counter
	// See EMV 4.3 Book 4, 6.5.5
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_9F41_TRANSACTION_SEQUENCE_COUNTER, 4, emv_uint_to_format_n(txn_seq_cnt, buf, 4), 0);

	// Current date and time
	tm->tm_year += date_offset; // Useful for expired test cards
	emv_date[0] = (((tm->tm_year / 10) % 10) << 4) | (tm->tm_year % 10);
	emv_date[1] = (((tm->tm_mon + 1) / 10) << 4) | ((tm->tm_mon + 1) % 10);
	emv_date[2] = ((tm->tm_mday / 10) << 4) | (tm->tm_mday % 10);
	emv_time[0] = ((tm->tm_hour / 10) << 4) | (tm->tm_hour % 10);
	emv_time[1] = ((tm->tm_min / 10) << 4) | (tm->tm_min % 10);
	emv_time[2] = ((tm->tm_sec / 10) << 4) | (tm->tm_sec % 10);
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_9A_TRANSACTION_DATE, 3, emv_date, 0);
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_9F21_TRANSACTION_TIME, 3, emv_time, 0);

	// Transaction currency
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0); // Euro (978)
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_5F36_TRANSACTION_CURRENCY_EXPONENT, 1, (uint8_t[]){ 0x02 }, 0); // Currency has 2 decimal places

	// Transaction type and amount(s)
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_9C_TRANSACTION_TYPE, 1, &txn_type, 0);
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, emv_uint_to_format_n(amount, buf, 6), 0);
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, emv_uint_to_format_b(amount, buf, 4), 0);
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC, 6, emv_uint_to_format_n(amount_other, buf, 6), 0);
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_9F04_AMOUNT_OTHER_BINARY, 4, emv_uint_to_format_b(amount_other, buf, 4), 0);
}

static void emv_txn_load_config(struct emv_txn_t* emv_txn)
{
	// Terminal config
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F01_ACQUIRER_IDENTIFIER, 6, (uint8_t[]){ 0x00, 0x01, 0x23, 0x45, 0x67, 0x89 }, 0 ); // Unique acquirer identifier
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0); // Netherlands
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x03, 0xE8 }, 0); // 1000
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F16_MERCHANT_IDENTIFIER, 15, (const uint8_t*)"0987654321     ", 0); // Unique merchant identifier
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F1C_TERMINAL_IDENTIFICATION, 8, (const uint8_t*)"TID12345", 0); // Unique location of terminal at merchant
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F1E_IFD_SERIAL_NUMBER, 8, (const uint8_t*)"12345678", 0); // Serial number
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F4E_MERCHANT_NAME_AND_LOCATION, 12, (const uint8_t*)"ACME Peanuts", 0); // Merchant Name and Location

	// Terminal Capabilities:
	// - Card Data Input Capability: IC with Contacts
	// - CVM Capability: Plaintext offline PIN, Enciphered online PIN, Signature, Enciphered offline PIN, No CVM
	// - Security Capability: SDA, DDA, CDA
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){ 0x20, 0xF8, 0xC8}, 0);

	// Merchant attended, offline with online capability
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){ 0x22 }, 0);

	// Additional Terminal Capabilities:
	// - Transaction Type Capability: Goods, Services, Cashback, Cash, Inquiry, Payment
	// - Terminal Data Input Capability: Numeric, Alphabetic and special character keys, Command keys, Function keys
	// - Terminal Data Output Capability: Attended print, Attended display, Code table 1 to 10
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, 5, (uint8_t[]){ 0xFA, 0x00, 0xF0, 0xA3, 0xFF }, 0);

	// Supported applications
	emv_tlv_list_push(&emv_txn->supported_aids, EMV_TAG_9F06_AID, 6, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10 }, EMV_ASI_PARTIAL_MATCH); // Visa
	emv_tlv_list_push(&emv_txn->supported_aids, EMV_TAG_9F06_AID, 7, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x10 }, EMV_ASI_EXACT_MATCH); // Visa Electron
	emv_tlv_list_push(&emv_txn->supported_aids, EMV_TAG_9F06_AID, 7, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x20, 0x20 }, EMV_ASI_EXACT_MATCH); // V Pay
	emv_tlv_list_push(&emv_txn->supported_aids, EMV_TAG_9F06_AID, 6, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10 }, EMV_ASI_PARTIAL_MATCH); // Mastercard
	emv_tlv_list_push(&emv_txn->supported_aids, EMV_TAG_9F06_AID, 6, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x04, 0x30 }, EMV_ASI_PARTIAL_MATCH); // Maestro
}

static void emv_txn_destroy(struct emv_txn_t* emv_txn)
{
	emv_tlv_list_clear(&emv_txn->params);
	emv_tlv_list_clear(&emv_txn->supported_aids);
	emv_tlv_list_clear(&emv_txn->terminal);
	emv_tlv_list_clear(&emv_txn->icc);
	emv_app_free(emv_txn->selected_app);
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
	uint8_t atr[PCSC_MAX_ATR_SIZE];
	size_t atr_len = 0;

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

	r = emv_strings_init(NULL, mcc_json);
	if (r < 0) {
		fprintf(stderr, "Failed to initialise EMV strings\n");
		return 2;
	}
	if (r > 0) {
		fprintf(stderr, "Failed to find iso-codes data; currency, country and language lookups will not be possible\n");
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

	printf("PC/SC readers:\n");
	for (size_t i = 0; i < pcsc_count; ++i) {
		reader = pcsc_get_reader(pcsc, i);
		printf("%zu: %s", i, pcsc_reader_get_name(reader));

		reader_state = 0;
		r = pcsc_reader_get_state(reader, &reader_state);
		if (r == 0) {
			reader_state_str = pcsc_get_reader_state_string(reader_state);
			if (reader_state) {
				printf("; %s", reader_state_str);
			} else {
				printf("; Unknown state");
			}
		}

		printf("\n");
	}

	// Wait for card presentation
	printf("\nPresent card\n");
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
	printf("Card detected\n\n");

	r = pcsc_reader_connect(reader);
	if (r) {
		printf("PC/SC reader activation failed\n");
		goto pcsc_exit;
	}
	printf("Card activated\n");

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
	memset(&emv_txn, 0, sizeof(emv_txn));
	emv_txn.ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
	emv_txn.ttl.cardreader.ctx = reader;
	emv_txn.ttl.cardreader.trx = &pcsc_reader_trx;
	emv_txn_load_params(
		&emv_txn,
		42, // Transaction Sequence Counter
		txn_type, // Transaction Type
		txn_amount, // Transaction Amount
		txn_amount_other // Transaction Amount, Other
	);
	emv_txn_load_config(&emv_txn);

	printf("\nTransaction parameters:\n");
	print_emv_tlv_list(&emv_txn.params);

	printf("\nTerminal config:\n");
	print_emv_tlv_list(&emv_txn.terminal);

	// Candidate applications for selection
	struct emv_app_list_t app_list = EMV_APP_LIST_INIT;

	printf("\nBuild candidate list\n");
	r = emv_build_candidate_list(&emv_txn.ttl, &emv_txn.supported_aids, &app_list);
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

	emv_txn.application_selection_required = emv_app_list_selection_is_required(&app_list);
	if (emv_txn.application_selection_required) {
		printf("Cardholder selection is required\n");
	}

	do {
		unsigned int index;

		if (emv_txn.application_selection_required) {
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
			printf("\nSelect first application:\n");
			index = 0;
		}

		r = emv_select_application(&emv_txn.ttl, &app_list, index, &emv_txn.selected_app);
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
		if (!emv_txn.selected_app) {
			fprintf(stderr, "selected_app unexpectedly NULL\n");
			goto emv_exit;
		}

		printf("\nInitiate application processing:\n");
		r = emv_initiate_application_processing(
			&emv_txn.ttl,
			emv_txn.selected_app,
			&emv_txn.params,
			&emv_txn.terminal,
			&emv_txn.icc
		);
		if (r < 0) {
			printf("ERROR: %s\n", emv_error_get_string(r));
			goto emv_exit;
		}
		if (r > 0) {
			printf("OUTCOME: %s\n", emv_outcome_get_string(r));
			if (r == EMV_OUTCOME_GPO_NOT_ACCEPTED && !emv_app_list_is_empty(&app_list)) {
				// Return to cardholder application selection/confirmation
				// See EMV 4.4 Book 4, 6.3.1
				emv_app_free(emv_txn.selected_app);
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

	// TODO: EMV 4.4 Book 1, 12.4, create 9F06 from 84

	printf("\nRead application data\n");
	r = emv_read_application_data(&emv_txn.ttl, &emv_txn.icc);
	if (r < 0) {
		printf("ERROR: %s\n", emv_error_get_string(r));
		goto emv_exit;
	}
	if (r > 0) {
		printf("OUTCOME: %s\n", emv_outcome_get_string(r));
		goto emv_exit;
	}
	print_emv_tlv_list(&emv_txn.icc);

	r = pcsc_reader_disconnect(reader);
	if (r) {
		printf("PC/SC reader deactivation failed\n");
		goto emv_exit;
	}
	printf("\nCard deactivated\n");

emv_exit:
	emv_app_list_clear(&app_list);
	emv_txn_destroy(&emv_txn);
pcsc_exit:
	pcsc_release(&pcsc);

	if (mcc_json) {
		free(mcc_json);
	}
}
