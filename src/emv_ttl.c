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
	uint8_t next_c_apdu[5];

	if (!ctx || !c_apdu || !c_apdu_len || !r_apdu || !r_apdu_len || !*r_apdu_len) {
		return -1;
	}

	// Assume card reader is in APDU mode
	if (ctx->cardreader.mode != EMV_CARDREADER_MODE_APDU) {
		return -2;
	}

	do {
		int r;
		size_t rx_len = *r_apdu_len;
		uint8_t SW1;
		uint8_t SW2;

		r = ctx->cardreader.trx(ctx->cardreader.ctx, c_apdu, c_apdu_len, r_apdu, &rx_len);
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
		// See EMV 4.3 Book 1, 9.2.2.3.1, table 25
		// See EMV 4.3 Book 1, 9.2.2.3.2
		// See EMV 4.3 Book 1, 9.3.1.2
		// See EMV 4.3 Book 1, Annex A for examples
		switch (SW1) {
			case 0x61: // Normal processing: SW2 encodes the number of available bytes
				// Build GET RESPONSE command
				next_c_apdu[0] = 0x00; // CLA
				next_c_apdu[1] = 0xC0; // INS: GET RESPONSE
				next_c_apdu[2] = 0x00; // P1
				next_c_apdu[3] = 0x00; // P2
				next_c_apdu[4] = SW2;  // Le
				c_apdu = next_c_apdu;
				c_apdu_len = 5;
				break;

			case 0x6C: // Checking error: Wrong Le field; SW2 encodes the exact number of available bytes

				// TODO: implement
				return -1;
				break;

			default:
				// Let Terminal Application Layer (TAL) process the response
				*r_apdu_len = rx_len;
				return 0;
		}

		// TODO: is it necessary to support ISO 7816-3:2006, 12.2, table 14

	} while (true);
}
