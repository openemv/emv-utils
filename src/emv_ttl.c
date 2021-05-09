/**
 * @file emv_ttl.c
 * @brief EMV Terminal Transport Layer (TTL)
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

#include "emv_ttl.h"
#include "iso7816_apdu.h"

#include <stdbool.h>
#include <string.h>

int emv_ttl_trx(
	struct emv_ttl_t* ctx,
	const void* c_apdu,
	size_t c_apdu_len,
	void* r_apdu,
	size_t* r_apdu_len,
	uint16_t* sw1sw2
)
{
	enum iso7816_apdu_case_t apdu_case;

	// C-TPDU header: CLA INS P1 P2 P3
	uint8_t c_tpdu_header[5];

	// Next buffer to transmit
	const void* tx_buf;
	size_t tx_buf_len;

	if (!ctx || !c_apdu || !c_apdu_len || !r_apdu || !r_apdu_len || !*r_apdu_len || !sw1sw2) {
		return -1;
	}

	// Determine the APDU case
	// See ISO 7816-3:2006, 12.1.3, table 13
	// See EMV 4.3 Book 1, Annex A

	// APDU cases:
	// Case 1: CLA INS P1 P2
	// Case 2: CLA INS P1 P2 Le
	// Case 3: CLA INS P1 P2 Lc [Data(Lc)]
	// Case 4: CLA INS P1 P2 Lc [Data(Lc)] Le

	if (c_apdu_len == 4) {
		apdu_case = ISO7816_APDU_CASE_1;
	} else if (c_apdu_len == 5) {
		apdu_case = ISO7816_APDU_CASE_2S;
	} else {
		// Extract byte C5 from header; See ISO 7816-3:2006, 12.1.3
		unsigned int C5 = *(uint8_t*)(c_apdu + 4);

		if (C5 != 0 && C5 + 5 == c_apdu_len) { // If C5 is Lc and Le is absent
			apdu_case = ISO7816_APDU_CASE_3S;
		} else if (C5 != 0 && C5 + 6 == c_apdu_len) { // If C5 is Lc and Le is present
			apdu_case = ISO7816_APDU_CASE_4S;
		} else {
			return -2;
		}
	}

	if (ctx->cardreader.mode == EMV_CARDREADER_MODE_APDU) {
		// For APDU mode, transmit C-APDU as-is
		tx_buf = c_apdu;
		tx_buf_len = c_apdu_len;

	} else if (ctx->cardreader.mode == EMV_CARDREADER_MODE_TPDU) {
		// For TPDU mode, transmit C-TPDU header and wait for procedure byte

		if (apdu_case == ISO7816_APDU_CASE_1) {
			// Build case 1 C-TPDU header
			memcpy(c_tpdu_header, c_apdu, 4);
			c_tpdu_header[4] = 0; // P3 = 0
		} else {
			// Build case 2S/3S/4S C-TPDU header
			memcpy(c_tpdu_header, c_apdu, 5);
		}

		tx_buf = c_tpdu_header;
		tx_buf_len = sizeof(c_tpdu_header);

	} else {
		// Unknown cardreader mode
		return -3;
	}

	do {
		int r;
		size_t rx_len = *r_apdu_len;
		uint8_t SW1;
		uint8_t SW2;

		r = ctx->cardreader.trx(ctx->cardreader.ctx, tx_buf, tx_buf_len, r_apdu, &rx_len);
		if (r) {
			return r;
		}
		if (rx_len == 0) {
			// No response
			return 1;
		}

		// Process response containing single procedure byte
		// See ISO 7816-3:2006, 10.3.3
		// See EMV 4.3 Book 1, 9.2.2.3.1, table 25
		if (rx_len == 1) {
			uint8_t procedure_byte = *((uint8_t*)r_apdu);
			uint8_t INS = *((uint8_t*)(c_apdu + 1));

			// Wait for another procudure byte
			if (procedure_byte == 0x60) {
				// Not supported by this TTL; assume card reader is in APDU mode
				*r_apdu_len = rx_len;
				return 1;
			}

			// ACK: Send remaining data bytes
			if (procedure_byte == INS) {
				// Not supported by this TTL; assume card reader is in APDU mode
				*r_apdu_len = rx_len;
				return 1;
			}

			// ACK: Send next byte
			if (procedure_byte == (INS ^ 0xFF)) {
				// Not supported by this TTL; assume card reader is in APDU mode
				*r_apdu_len = rx_len;
				return 1;
			}

			// Unknown procedure byte
			*r_apdu_len = rx_len;
			return 1;
		}

		SW1 = *((uint8_t*)(r_apdu + rx_len - 2));
		SW2 = *((uint8_t*)(r_apdu + rx_len - 1));

		// Process status bytes
		// See ISO 7816-3:2006, 12.2.1, table 14
		// See EMV 4.3 Book 1, 9.2.2.3.1, table 25
		// See EMV 4.3 Book 1, 9.2.2.3.2
		// See EMV 4.3 Book 1, 9.3.1.2
		// See EMV 4.3 Book 1, Annex A for examples
		switch (SW1) {
			case 0x61: // Normal processing: SW2 encodes the number of available bytes

				// Status 61XX is only allowed for APDU cases 2 and 4
				// See ISO 7816-3:2006, 12.2.1, table 14
				if (apdu_case != ISO7816_APDU_CASE_2S &&
					apdu_case != ISO7816_APDU_CASE_4S
				) {
					return -4;
				}

				// Build GET RESPONSE for next transmission
				// See ISO 7816-4:2005, 7.6.1
				c_tpdu_header[0] = 0x00; // CLA
				c_tpdu_header[1] = 0xC0; // INS: GET RESPONSE
				c_tpdu_header[2] = 0x00; // P1
				c_tpdu_header[3] = 0x00; // P2
				c_tpdu_header[4] = SW2;  // Le
				tx_buf = c_tpdu_header;
				tx_buf_len = sizeof(c_tpdu_header);
				break;

			case 0x6C: // Checking error: Wrong Le field; SW2 encodes the exact number of available bytes

				// Status 6CXX is only allowed for APDU cases 2 and 4
				// See ISO 7816-3:2006, 12.2.1, table 14
				if (apdu_case != ISO7816_APDU_CASE_2S &&
					apdu_case != ISO7816_APDU_CASE_4S
				) {
					return -4;
				}

				// Update Le for next transmission
				// See EMV 4.3 Book 1, 9.2.2.3.1, table 25
				memcpy(c_tpdu_header, c_apdu, 4);
				c_tpdu_header[4] = SW2; // P3 = Le
				tx_buf = c_tpdu_header;
				tx_buf_len = sizeof(c_tpdu_header);
				break;

			default:
				// Let Terminal Application Layer (TAL) process the response
				*r_apdu_len = rx_len;

				// Output status bytes SW1-SW2 in host endianness
				*sw1sw2 = ((uint16_t)SW1 << 8) | SW2;

				// TODO: EMV 4.3 Book 1, Annex A7 "Case 4 Command with Warning Condition"

				return 0;
		}

	} while (true);
}

int emv_ttl_select_by_df_name(
	struct emv_ttl_t* ctx,
	const void* df_name,
	size_t df_name_len,
	void* fci,
	size_t* fci_len,
	uint16_t* sw1sw2
)
{
	int r;
	struct iso7816_apdu_case_4s_t c_apdu;
	uint8_t r_apdu[EMV_RAPDU_MAX];
	size_t r_apdu_len = sizeof(r_apdu);

	if (!ctx || !df_name || !fci || !fci_len || !*fci_len || !sw1sw2) {
		return -1;
	}
	if (*fci_len < EMV_RAPDU_DATA_MAX) {
		return -2;
	}

	// For SELECT, ensure that Lc is from 0x05 to 0x10
	// See EMV 4.3 Book 1, 11.3.2, table 40
	if (df_name_len < 0x05 || df_name_len > 0x10) {
		return -3;
	}

	// Build SELECT command
	c_apdu.CLA = 0x00; // See EMV 4.3 Book 3, 6.3.2
	c_apdu.INS = 0xA4; // See EMV 4.3 Book 1, 11.3.2, table 40
	c_apdu.P1  = 0x04; // See EMV 4.3 Book 1, 11.3.2, table 41
	c_apdu.P2  = 0x00; // See EMV 4.3 Book 1, 11.3.2, table 42
	c_apdu.Lc  = df_name_len;
	memcpy(c_apdu.data, df_name, c_apdu.Lc);
	c_apdu.data[c_apdu.Lc] = 0x00; // See EMV 4.3 Book 1, 11.3.2, table 40

	r = emv_ttl_trx(
		ctx,
		&c_apdu,
		iso7816_apdu_case_4s_length(&c_apdu),
		r_apdu,
		&r_apdu_len,
		sw1sw2
	);
	if (r) {
		return r;
	}
	if (r_apdu_len < 2) {
		return -4;
	}
	if (r_apdu_len - 2 > *fci_len) {
		return -5;
	}

	// Copy FCI from R-APDU
	*fci_len = r_apdu_len - 2;
	memcpy(fci, r_apdu, *fci_len);

	return 0;
}

int emv_ttl_select_by_df_name_next(
	struct emv_ttl_t* ctx,
	const void* df_name,
	size_t df_name_len,
	void* fci,
	size_t* fci_len,
	uint16_t* sw1sw2
)
{
	int r;
	struct iso7816_apdu_case_4s_t c_apdu;
	uint8_t r_apdu[EMV_RAPDU_MAX];
	size_t r_apdu_len = sizeof(r_apdu);

	if (!ctx || !df_name || !fci || !fci_len || !*fci_len || !sw1sw2) {
		return -1;
	}
	if (*fci_len < EMV_RAPDU_DATA_MAX) {
		return -2;
	}

	// For SELECT, ensure that Lc is from 0x05 to 0x10
	// See EMV 4.3 Book 1, 11.3.2, table 40
	if (df_name_len < 0x05 || df_name_len > 0x10) {
		return -3;
	}

	// Build SELECT command
	c_apdu.CLA = 0x00; // See EMV 4.3 Book 3, 6.3.2
	c_apdu.INS = 0xA4; // See EMV 4.3 Book 1, 11.3.2, table 40
	c_apdu.P1  = 0x04; // See EMV 4.3 Book 1, 11.3.2, table 41
	c_apdu.P2  = 0x02; // See EMV 4.3 Book 1, 11.3.2, table 42
	c_apdu.Lc  = df_name_len;
	memcpy(c_apdu.data, df_name, c_apdu.Lc);
	c_apdu.data[c_apdu.Lc] = 0x00; // See EMV 4.3 Book 1, 11.3.2, table 40

	r = emv_ttl_trx(
		ctx,
		&c_apdu,
		iso7816_apdu_case_4s_length(&c_apdu),
		r_apdu,
		&r_apdu_len,
		sw1sw2
	);
	if (r) {
		return r;
	}
	if (r_apdu_len < 2) {
		return -4;
	}
	if (r_apdu_len - 2 > *fci_len) {
		return -5;
	}

	// Copy FCI from R-APDU
	*fci_len = r_apdu_len - 2;
	memcpy(fci, r_apdu, *fci_len);

	return 0;
}

int emv_ttl_read_record(
	struct emv_ttl_t* ctx,
	uint8_t sfi,
	uint8_t record_number,
	void* data,
	size_t* data_len,
	uint16_t* sw1sw2
)
{
	int r;
	struct iso7816_apdu_case_2s_t c_apdu;
	uint8_t r_apdu[EMV_RAPDU_MAX];
	size_t r_apdu_len = sizeof(r_apdu);

	if (!ctx || !data || !data_len || !*data_len || !sw1sw2) {
		return -1;
	}

	if (*data_len < EMV_RAPDU_DATA_MAX) {
		return -2;
	}

	// For READ RECORD, ensure that SFI is from 0x01 to 0x1E
	// See ISO 7816-4:2005, 7.3.2, table 47
	if (sfi < 0x01 || sfi > 0x1E) {
		return -3;
	}

	// Build READ RECORD command
	c_apdu.CLA = 0x00; // See EMV 4.3 Book 3, 6.3.2
	c_apdu.INS = 0xB2; // See EMV 4.3 Book 1, 11.2.2, table 38
	c_apdu.P1  = record_number; // See EMV 4.3 Book 1, 11.2.2, table 38
	c_apdu.P2  = (sfi << 3) | 0x04; // See EMV 4.3 Book 1, 11.2.2, table 39
	c_apdu.Le  = 0x00; // See EMV 4.3 Book 1, 11.2.2, table 38

	r = emv_ttl_trx(
		ctx,
		&c_apdu,
		sizeof(c_apdu),
		r_apdu,
		&r_apdu_len,
		sw1sw2
	);
	if (r) {
		return r;
	}
	if (r_apdu_len < 2) {
		return -4;
	}
	if (r_apdu_len - 2 > *data_len) {
		return -5;
	}

	// Copy record data from R-APDU
	*data_len = r_apdu_len - 2;
	memcpy(data, r_apdu, *data_len);

	return 0;
}
