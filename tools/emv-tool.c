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

#include <stdio.h>

// HACK: remove
#include "emv_ttl.h"
#include "iso7816_strings.h"

// Helper functions
static const char* pcsc_get_reader_state_string(unsigned int reader_state);

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
		goto exit;
	}

	pcsc_count = pcsc_get_reader_count(pcsc);
	if (!pcsc_count) {
		printf("No PC/SC readers detected\n");
		goto exit;
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
		goto exit;
	}
	if (r > 0) {
		printf("No card; exiting\n");
		goto exit;
	}

	reader = pcsc_get_reader(pcsc, reader_idx);
	printf("Card detected\n\n");

	r = pcsc_reader_connect(reader);
	if (r) {
		printf("PC/SC reader activation failed\n");
		goto exit;
	}
	printf("Card activated\n");

	r = pcsc_reader_get_atr(reader, atr, &atr_len);
	if (r) {
		printf("Failed to retrieve ATR\n");
		goto exit;
	}

	r = iso7816_atr_parse(atr, atr_len, &atr_info);
	if (r) {
		printf("Failed to parse ATR\n");
		goto exit;
	}

	print_atr(&atr_info);
	printf("\n");

	// HACK: test SELECT PSE
	{
		char str[1024];
		struct emv_ttl_t emv_ttl;
		emv_ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
		emv_ttl.cardreader.ctx = reader;
		emv_ttl.cardreader.trx = &pcsc_reader_trx;

		printf("SELECT PSE\n");

		const uint8_t PSE[] = "1PAY.SYS.DDF01";
		uint8_t fci[EMV_RAPDU_DATA_MAX];
		size_t fci_len = sizeof(fci);
		uint16_t sw1sw2;
		r = emv_ttl_select_by_df_name(&emv_ttl, PSE, sizeof(PSE) - 1, fci, &fci_len, &sw1sw2);
		if (r) {
			printf("Failed to select PSE; r=%d\n", r);
			goto exit;
		}
		print_buf("FCI", fci, fci_len);
		print_emv_tlv(fci, fci_len, "  ", 0);
		printf("SW1SW2 = %04hX (%s)\n", sw1sw2, iso7816_sw1sw2_get_string(sw1sw2 >> 8, sw1sw2 & 0xff, str, sizeof(str)));

		// HACK: fake SFI for testing; this should be extracted from FCI instead
		uint8_t sfi = 0x01;

		for (uint8_t record_number = 1; ; ++record_number) {
			uint8_t data[EMV_RAPDU_DATA_MAX];
			size_t data_len = sizeof(data);

			printf("READ RECORD %u,%u\n", sfi, record_number);

			r = emv_ttl_read_record(&emv_ttl, sfi, record_number, data, &data_len, &sw1sw2);
			if (r) {
				return r;
			}
			print_buf("RECORD", data, data_len);
			print_emv_tlv(data, data_len, "  ", 0);
			printf("SW1SW2 = %04hX (%s)\n", sw1sw2, iso7816_sw1sw2_get_string(sw1sw2 >> 8, sw1sw2 & 0xff, str, sizeof(str)));

			if (sw1sw2 != 0x9000) {
				break;
			}
		}
	}

	r = pcsc_reader_disconnect(reader);
	if (r) {
		printf("PC/SC reader deactivation failed\n");
		goto exit;
	}
	printf("\nCard deactivated\n");

exit:
	pcsc_release(&pcsc);
}
