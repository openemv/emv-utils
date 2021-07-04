/**
 * @file emv-tool.c
 * @brief Simple EMV processing tool
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

#include "pcsc.h"
#include "iso7816.h"
#include "print_helpers.h"
#include "emv_tags.h"
#include "emv_fields.h"
#include "emv_tlv.h"
#include "emv_tal.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

// HACK: remove
#include "emv_dol.h"
#include "emv_ttl.h"
#include "emv_app.h"
#include "iso7816_strings.h"
#include <string.h> // for memset() and memcpy() that should be removed later

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
};
static struct emv_txn_t emv_txn;

// Helper functions
static const char* pcsc_get_reader_state_string(unsigned int reader_state);
static void emv_txn_load_config(struct emv_txn_t* emv_txn);
static void emv_txn_destroy(struct emv_txn_t* emv_txn);

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

static void emv_txn_load_params(struct emv_txn_t* emv_txn)
{
	time_t t = time(NULL);
	struct tm* tm = localtime(&t);
	uint8_t emv_date[3];
	uint8_t emv_time[3];
	int date_offset = 0;

	// Transaction sequence counter
	// See EMV 4.3 Book 4, 6.5.5
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_9F41_TRANSACTION_SEQUENCE_COUNTER, 4, (uint8_t[]){ 0x00, 0x00, 0x00, 0x42 }, 0); // Transaction #42

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
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ 0x00 }, 0); // Goods and services
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 }, 0); // 1000
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x03, 0xE8 }, 0); // 1000
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0); // 0
	emv_tlv_list_push(&emv_txn->params, EMV_TAG_9F04_AMOUNT_OTHER_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00 }, 0); // 0
}

static void emv_txn_load_config(struct emv_txn_t* emv_txn)
{
	// Terminal config
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0); // Netherlands
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x03, 0xE8 }, 0); // 1000
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F1C_TERMINAL_IDENTIFICATION, 8, (const uint8_t*)"emv-tool", 0); // Unique location of terminal at merchant
	emv_tlv_list_push(&emv_txn->terminal, EMV_TAG_9F1E_IFD_SERIAL_NUMBER, 8, (const uint8_t*)"12345678", 0); // Serial number

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
}

int main(void)
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
	struct iso7816_atr_info_t atr_info;

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

	r = iso7816_atr_parse(atr, atr_len, &atr_info);
	if (r) {
		printf("Failed to parse ATR\n");
		goto pcsc_exit;
	}

	print_atr(&atr_info);

	// Prepare for EMV transaction
	memset(&emv_txn, 0, sizeof(emv_txn));
	emv_txn.ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
	emv_txn.ttl.cardreader.ctx = reader;
	emv_txn.ttl.cardreader.trx = &pcsc_reader_trx;
	emv_txn_load_params(&emv_txn);
	emv_txn_load_config(&emv_txn);

	printf("\nTransaction parameters:\n");
	print_emv_tlv_list(&emv_txn.params);

	printf("\nTerminal config:\n");
	print_emv_tlv_list(&emv_txn.terminal);

	// Candidate applications for selection
	struct emv_app_list_t app_list = EMV_APP_LIST_INIT;

	printf("\nSELECT Payment System Environment (PSE)\n");
	r = emv_tal_read_pse(&emv_txn.ttl, &app_list);
	if (r < 0) {
		printf("Failed to read PSE; terminate session\n");
		goto emv_exit;
	}
	if (r > 0) {
		printf("Failed to read PSE; continue session\n");
	}
	if (r == 0) {
		printf("Application list provided by PSE:\n");
		for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
			print_emv_app(app);
		}
	}

	// Filter for supported applications
	emv_app_list_filter_supported(&app_list, &emv_txn.supported_aids);

	// If PSE failed or no apps found by PSE
	// See EMV 4.3 Book 1, 12.3.2, step 5
	if (r > 0 || emv_app_list_is_empty(&app_list)) {
		printf("\nFind supported AIDs\n");
		r = emv_tal_find_supported_apps(&emv_txn.ttl, &emv_txn.supported_aids, &app_list);
		if (r) {
			printf("Failed to find supported AIDs; terminate session\n");
			goto emv_exit;
		}
	}

	if (emv_app_list_is_empty(&app_list)) {
		printf("No supported applications\n");
		goto emv_exit;
	}

	printf("Candidate applications:\n");
	for (struct emv_app_t* app = app_list.front; app != NULL; app = app->next) {
		print_emv_app(app);
	}

	// HACK: test application selection
	{
		char str[1024];

		// Use first application
		struct emv_app_t* current_app = emv_app_list_pop(&app_list);
		if (!current_app) {
			printf("No supported applications\n");
			goto emv_exit;
		}
		emv_app_list_clear(&app_list);

		uint8_t current_aid[16];
		size_t current_aid_len = current_app->aid->length;
		memcpy(current_aid, current_app->aid->value, current_app->aid->length);
		emv_app_free(current_app);
		current_app = NULL;

		// Select application
		print_buf("\nSELECT application", current_aid, current_aid_len);
		uint8_t fci[EMV_RAPDU_DATA_MAX];
		size_t fci_len = sizeof(fci);
		uint16_t sw1sw2;
		r = emv_ttl_select_by_df_name(&emv_txn.ttl, current_aid, current_aid_len, fci, &fci_len, &sw1sw2);
		if (r) {
			printf("Failed to select application; r=%d\n", r);
			goto emv_exit;
		}
		print_buf("FCI", fci, fci_len);
		print_emv_buf(fci, fci_len, "  ", 0);
		printf("SW1SW2 = %04hX (%s)\n", sw1sw2, iso7816_sw1sw2_get_string(sw1sw2 >> 8, sw1sw2 & 0xff, str, sizeof(str)));

		if (sw1sw2 != 0x9000) {
			goto emv_exit;
		}

		// Create EMV application object
		struct emv_app_t* app;
		app = emv_app_create_from_fci(fci, fci_len);
		if (r) {
			printf("emv_app_populate_from_fci() failed; r=%d\n", r);
			goto emv_exit;
		}
		printf("\n");
		print_emv_app(app);
		print_emv_tlv_list(&app->tlv_list);

		// Capture ICC data
		emv_txn.icc = app->tlv_list;
		app->tlv_list = EMV_TLV_LIST_INIT;
		emv_app_free(app);

		// Process PDOL
		struct emv_tlv_t* pdol;
		uint8_t gpo_data[EMV_RAPDU_DATA_MAX];
		size_t gpo_data_len = sizeof(gpo_data);
		pdol = emv_tlv_list_find(&emv_txn.icc, EMV_TAG_9F38_PDOL);
		if (pdol) {
			int dol_data_len;
			size_t gpo_data_offset;

			dol_data_len = emv_dol_compute_data_length(pdol->value, pdol->length);
			if (dol_data_len < 0) {
				printf("emv_dol_compute_data_length() failed; r=%d\n", r);
				goto emv_exit;
			}

			gpo_data[0] = EMV_TAG_83_COMMAND_TEMPLATE;
			// TODO: proper BER length encoding
			if (dol_data_len < 0x80) {
				gpo_data[1] = dol_data_len;
				gpo_data_offset = 2;
			} else {
				printf("PDOL data length greater than 127 bytes not implemented\n");
				goto emv_exit;
			}
			gpo_data_len -= gpo_data_offset;

			r = emv_dol_build_data(
				pdol->value,
				pdol->length,
				&emv_txn.params,
				&emv_txn.terminal,
				gpo_data + gpo_data_offset,
				&gpo_data_len
			);
			if (r) {
				printf("emv_dol_build_data() failed; r=%d\n", r);
				goto emv_exit;
			}
			gpo_data_len += gpo_data_offset;

		} else {
			// Use empty Command Template (field 83)
			// See EMV 4.3 Book 3, 6.5.8.3
			// See EMV 4.3 Book 3, 10.1
			gpo_data[0] = EMV_TAG_83_COMMAND_TEMPLATE;
			gpo_data[1] = 0;
			gpo_data_len = 2;
		}

		print_buf("\nGPO data", gpo_data, gpo_data_len);
	}

	r = pcsc_reader_disconnect(reader);
	if (r) {
		printf("PC/SC reader deactivation failed\n");
		goto emv_exit;
	}
	printf("\nCard deactivated\n");

emv_exit:
	emv_txn_destroy(&emv_txn);
pcsc_exit:
	pcsc_release(&pcsc);
}
