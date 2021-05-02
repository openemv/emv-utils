/**
 * @file emv_ttl.c
 * @brief EMV Terminal Transport Layer
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
	size_t* r_apdu_len
)
{
	enum iso7816_apdu_case_t apdu_case;

	// C-TPDU header: CLA INS P1 P2 P3
	uint8_t c_tpdu_header[5];

	// Next buffer to transmit
	const void* tx_buf;
	size_t tx_buf_len;

	if (!ctx || !c_apdu || !c_apdu_len || !r_apdu || !r_apdu_len || !*r_apdu_len) {
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
				return 0;

			// TODO: EMV 4.3 Book 1, Annex A7 "Case 4 Command with Warning Condition"
		}

	} while (true);
}
