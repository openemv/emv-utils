/**
 * @file emv_ttl.c
 * @brief EMV Terminal Transport Layer (TTL)
 *
 * Copyright (c) 2021, 2024 Leon Lynch
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
#include "emv_tags.h"
#include "iso7816_apdu.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_TTL
#include "emv_debug.h"

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
	// C-TPDU data: found after P3; NULL if absent
	const void* c_tpdu_data = NULL;

	// Next buffer to transmit
	const void* tx_buf;
	size_t tx_buf_len;

	// Current position in R-APDU output
	uint8_t* r_apdu_ptr = r_apdu;

	if (!ctx || !c_apdu || !c_apdu_len || !r_apdu || !r_apdu_len || !*r_apdu_len || !sw1sw2) {
		return -1;
	}

	emv_debug_capdu(c_apdu, c_apdu_len);

	// Determine the APDU case
	// See ISO 7816-3:2006, 12.1.3, table 13
	// See EMV Contact Interface Specification v1.0, 9.3.1.1
	// See EMV Contact Interface Specification v1.0, Annex A

	// APDU cases:
	// Case 1: CLA INS P1 P2
	// Case 2: CLA INS P1 P2 Le
	// Case 3: CLA INS P1 P2 Lc [Data(Lc)]
	// Case 4: CLA INS P1 P2 Lc [Data(Lc)] Le

	apdu_case = iso7816_apdu_case(c_apdu, c_apdu_len);
	switch (apdu_case) {
		case ISO7816_APDU_CASE_1: emv_debug_apdu("APDU case 1"); break;
		case ISO7816_APDU_CASE_2S: emv_debug_apdu("APDU case 2S"); break;
		case ISO7816_APDU_CASE_3S: emv_debug_apdu("APDU case 3S"); break;
		case ISO7816_APDU_CASE_4S: emv_debug_apdu("APDU case 4S"); break;

		default:
			emv_debug_error("Unknown APDU case");
			return -2;
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

		if (apdu_case == ISO7816_APDU_CASE_3S ||
			apdu_case == ISO7816_APDU_CASE_4S
		) {
			// Locate C-TPDU data
			c_tpdu_data = c_apdu + 5;
		}

		tx_buf = c_tpdu_header;
		tx_buf_len = sizeof(c_tpdu_header);

	} else {
		// Unknown cardreader mode
		return -3;
	}

	// Clear SW1-SW2 to allow it to be checked later
	*sw1sw2 = 0;

	do {
		int r;
		uint8_t INS;
		uint8_t rx_buf[EMV_RAPDU_MAX];
		size_t rx_len = sizeof(rx_buf);
		uint8_t SW1;
		uint8_t SW2;
		bool tx_get_response = false;
		bool tx_update_le  = false;

		emv_debug_ctpdu(tx_buf, tx_buf_len);

		r = ctx->cardreader.trx(ctx->cardreader.ctx, tx_buf, tx_buf_len, rx_buf, &rx_len);
		if (r) {
			return r;
		}
		if (rx_len == 0) {
			// No response
			emv_debug_error("No response");
			return 1;
		}

		emv_debug_rtpdu(rx_buf, rx_len);

		// Store INS of most recent tx for later use
		INS = *((uint8_t*)(tx_buf + 1));

		// Extract SW1-SW2 status bytes
		if (rx_len > 1) {
			SW1 = *((uint8_t*)(rx_buf + rx_len - 2));
			SW2 = *((uint8_t*)(rx_buf + rx_len - 1));
		} else {
			SW1 = *((uint8_t*)rx_buf);
			SW2 = 0xFF;
		}

		// For APDU case 1, the R-APDU should only be SW1-SW2; no further action
		// See ISO 7816-3:2006, 12.2.1, table 14
		// See EMV Contact Interface Specification v1.0, 9.3.1.1.1
		if (apdu_case == ISO7816_APDU_CASE_1) {
			if (rx_len != 2) {
				// Unexpected response length for APDU case 1
				*r_apdu_len = 0;
				return 2;
			}

			// Copy data to R-APDU
			memcpy(r_apdu, rx_buf, rx_len);
			*r_apdu_len = rx_len;

			// Output status bytes SW1-SW2 in host endianness
			*sw1sw2 = ((uint16_t)SW1 << 8) | SW2;

			// Let Terminal Application Layer (TAL) process the response
			emv_debug_rapdu(r_apdu, *r_apdu_len);
			return 0;
		}

		// Process response containing single procedure byte
		// See ISO 7816-3:2006, 10.3.3
		// See EMV Contact Interface Specification v1.0, 9.2.2.3.1, table 25
		if (rx_len == 1) {
			uint8_t procedure_byte = *((uint8_t*)rx_buf);

			// Wait for another procedure byte
			if (procedure_byte == 0x60) {
				// Not supported by this TTL; assume that card reader hardware
				// or driver will take care of this procedure byte
				memcpy(r_apdu, rx_buf, rx_len);
				*r_apdu_len = rx_len;
				return 3;
			}

			// ACK: Send remaining data bytes
			if (procedure_byte == INS) {
				if (!c_tpdu_data) {
					// Unexpected procedure byte if no data available
					memcpy(r_apdu, rx_buf, rx_len);
					*r_apdu_len = rx_len;
					return 4;
				}

				tx_buf = c_tpdu_data;
				tx_buf_len = c_tpdu_header[4]; // Next transmit length is Lc

				// No more data available
				c_tpdu_data = NULL;

				continue;
			}

			// ACK: Send next byte
			if (procedure_byte == (INS ^ 0xFF)) {
				if (!c_tpdu_data) {
					// Unexpected procedure byte if no data available
					memcpy(r_apdu, rx_buf, rx_len);
					*r_apdu_len = rx_len;
					return 5;
				}

				tx_buf = c_tpdu_data;
				tx_buf_len = 1;
				++c_tpdu_data;

				// Case 3: CLA INS P1 P2 Lc [Data(Lc)]
				// If c_tpdu_data is at the end of [Data(Lc)],
				// no more data is available
				if (apdu_case == ISO7816_APDU_CASE_3S &&
					c_tpdu_data - c_apdu >= c_apdu_len
				) {
					// No more data available
					c_tpdu_data = NULL;
				}

				// Case 4: CLA INS P1 P2 Lc [Data(Lc)] Le
				// If c_tpdu_data is at the end of [Data(Lc)],
				// no more data is available
				if (apdu_case == ISO7816_APDU_CASE_4S &&
					c_tpdu_data - c_apdu >= c_apdu_len - 1
				) {
					// No more data available
					c_tpdu_data = NULL;
				}

				continue;
			}

			// Unknown procedure byte
			memcpy(r_apdu, rx_buf, rx_len);
			*r_apdu_len = rx_len;
			return 6;
		}

		// Process status bytes
		// See ISO 7816-3:2006, 12.2.1, table 14
		// See ISO 7816-4:2005, 5.1.3, table 6
		// See EMV Contact Interface Specification v1.0, 9.2.2.3.1, table 25
		// See EMV Contact Interface Specification v1.0, 9.2.2.3.2
		// See EMV Contact Interface Specification v1.0, 9.3.1.1
		// See EMV Contact Interface Specification v1.0, 9.3.1.2
		// See EMV Contact Interface Specification v1.0, Annex A for examples
		switch (SW1) {
			case 0x61: // Normal processing: SW2 encodes the number of available bytes

				// Status 61XX is only allowed for APDU cases 2 and 4
				// See ISO 7816-3:2006, 12.2.1, table 14
				if (apdu_case != ISO7816_APDU_CASE_2S &&
					apdu_case != ISO7816_APDU_CASE_4S
				) {
					return 7;
				}

				// Build GET RESPONSE for next transmission
				tx_get_response = true;
				break;

			case 0x6C: // Checking error: Wrong Le field; SW2 encodes the exact number of available bytes

				// Status 6CXX is only allowed for APDU cases 2 and 4
				// See ISO 7816-3:2006, 12.2.1, table 14
				if (apdu_case != ISO7816_APDU_CASE_2S &&
					apdu_case != ISO7816_APDU_CASE_4S
				) {
					return 8;
				}

				// Update Le for next transmission
				tx_update_le = true;
				break;

			default:
				// Terminal Application Layer (TAL) should receive the SW1-SW2
				// value for all remaining cases, including normal completion
				// and completion with a warning. Note that warning processing
				// may occur before the response data is received.
				// See EMV Contact Interface Specification v1.0, 9.3.1.1
				if (!*sw1sw2) {
					// Output status bytes SW1-SW2 in host endianness
					*sw1sw2 = ((uint16_t)SW1 << 8) | SW2;

					// For APDU case 4, if warning processing occurs without
					// response data, it is followed by GET RESPONSE
					// See EMV Contact Interface Specification v1.0, Annex A7 "Case 4 Command with Warning Condition"
					if (apdu_case == ISO7816_APDU_CASE_4S &&
						rx_len == 2 &&
						iso7816_sw1sw2_is_warning(SW1, SW2)
					) {
						tx_get_response = true;
						break;
					}
				}
		}

		// For TPDU mode and a response that appears to contain response data
		if (ctx->cardreader.mode == EMV_CARDREADER_MODE_TPDU &&
			rx_len > 3
		) {
			// Response data is only allowed for APDU cases 2 and 4
			if (apdu_case != ISO7816_APDU_CASE_2S &&
				apdu_case != ISO7816_APDU_CASE_4S
			) {
				return 9;
			}

			// Verify leading INS byte in R-TPDU
			if (memcmp(rx_buf, &INS, 1) != 0) {
				// R-TPDU data response should have leading INS byte
				return 10;
			}

			// Discard leading INS byte
			rx_len -= 1;

			// Ensure that R-APDU buffer has enough capacity for incoming
			// data as well as trailing SW1-SW2
			if (*r_apdu_len < rx_len) {
				return -4;
			}

			// Ignore trailing status bytes
			rx_len -= 2;

			// Copy data to R-APDU
			memcpy(r_apdu_ptr, rx_buf + 1, rx_len);
			r_apdu_ptr += rx_len;
			*r_apdu_len -= rx_len;
		}

		// For APDU mode and a response that appears to contain response data
		if (ctx->cardreader.mode == EMV_CARDREADER_MODE_APDU &&
			rx_len > 2
		) {
			// Response data is only allowed for APDU cases 2 and 4
			if (apdu_case != ISO7816_APDU_CASE_2S &&
				apdu_case != ISO7816_APDU_CASE_4S
			) {
				return 11;
			}

			// Ensure that R-APDU buffer has enough capacity for incoming
			// data as well as trailing SW1-SW2
			if (*r_apdu_len < rx_len) {
				return -5;
			}

			// Ignore trailing status bytes
			rx_len -= 2;

			// Copy data to R-APDU
			memcpy(r_apdu_ptr, rx_buf, rx_len);
			r_apdu_ptr += rx_len;
			*r_apdu_len -= rx_len;
		}

		if (tx_get_response) {
			uint8_t Le;

			// Determine Le field from SW1-SW2
			// See ISO 7816-4:2005, 5.1.3
			if (SW1 == 0x61 ||
				(SW1 == 0x62 && SW2 >= 0x02 && SW2 <= 0x80)
			) {
				Le = SW2;
			} else {
				Le = 0;
			}

			// Build GET RESPONSE for next transmission
			// See ISO 7816-4:2005, 7.6.1
			// See EMV Contact Interface Specification v1.0, 9.3.1.3
			c_tpdu_header[0] = 0x00; // CLA
			c_tpdu_header[1] = 0xC0; // INS: GET RESPONSE
			c_tpdu_header[2] = 0x00; // P1
			c_tpdu_header[3] = 0x00; // P2
			c_tpdu_header[4] = Le;   // Le
			tx_buf = c_tpdu_header;
			tx_buf_len = sizeof(c_tpdu_header);

			// Next transmission
			continue;
		}

		if (tx_update_le) {
			// Update Le for next transmission
			// See EMV Contact Interface Specification v1.0, 9.2.2.3.1, table 25
			memcpy(c_tpdu_header, tx_buf, 4);
			c_tpdu_header[4] = SW2; // P3 = Le
			tx_buf = c_tpdu_header;
			tx_buf_len = sizeof(c_tpdu_header);

			// Next transmission
			continue;
		}

		// Finalise R-APDU using current SW1-SW2
		r_apdu_ptr[0] = *sw1sw2 >> 8;
		r_apdu_ptr[1] = *sw1sw2 & 0xFF;
		r_apdu_ptr += 2;
		*r_apdu_len = (void*)r_apdu_ptr - r_apdu;

		// Let Terminal Application Layer (TAL) process the response
		emv_debug_rapdu(r_apdu, *r_apdu_len);
		return 0;

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
	// See EMV 4.4 Book 1, 11.3.2, table 5
	if (df_name_len < 0x05 || df_name_len > 0x10) {
		return -3;
	}

	// Build SELECT command
	c_apdu.CLA = 0x00; // See EMV 4.4 Book 3, 6.3.2
	c_apdu.INS = 0xA4; // See EMV 4.4 Book 1, 11.3.2, table 5
	c_apdu.P1  = 0x04; // See EMV 4.4 Book 1, 11.3.2, table 6
	c_apdu.P2  = 0x00; // See EMV 4.4 Book 1, 11.3.2, table 7
	c_apdu.Lc  = df_name_len;
	memcpy(c_apdu.data, df_name, c_apdu.Lc);
	c_apdu.data[c_apdu.Lc] = 0x00; // See EMV 4.4 Book 1, 11.3.2, table 5

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
	// See EMV 4.4 Book 1, 11.3.2, table 5
	if (df_name_len < 0x05 || df_name_len > 0x10) {
		return -3;
	}

	// Build SELECT command
	c_apdu.CLA = 0x00; // See EMV 4.4 Book 3, 6.3.2
	c_apdu.INS = 0xA4; // See EMV 4.4 Book 1, 11.3.2, table 5
	c_apdu.P1  = 0x04; // See EMV 4.4 Book 1, 11.3.2, table 6
	c_apdu.P2  = 0x02; // See EMV 4.4 Book 1, 11.3.2, table 7
	c_apdu.Lc  = df_name_len;
	memcpy(c_apdu.data, df_name, c_apdu.Lc);
	c_apdu.data[c_apdu.Lc] = 0x00; // See EMV 4.4 Book 1, 11.3.2, table 5

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
	c_apdu.CLA = 0x00; // See EMV 4.4 Book 3, 6.3.2
	c_apdu.INS = 0xB2; // See EMV 4.4 Book 1, 11.2.2, table 3
	c_apdu.P1  = record_number; // See EMV 4.4 Book 1, 11.2.2, table 3
	c_apdu.P2  = (sfi << 3) | 0x04; // See EMV 4.4 Book 1, 11.2.2, table 4
	c_apdu.Le  = 0x00; // See EMV 4.4 Book 1, 11.2.2, table 3

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

int emv_ttl_get_processing_options(
	struct emv_ttl_t* ctx,
	const void* data,
	size_t data_len,
	void* response,
	size_t* response_len,
	uint16_t* sw1sw2
) {
	int r;
	struct iso7816_apdu_case_4s_t c_apdu;
	uint8_t empty_cmd_data[2];
	uint8_t r_apdu[EMV_RAPDU_MAX];
	size_t r_apdu_len = sizeof(r_apdu);

	if (!ctx || !response || !response_len || !*response_len || !sw1sw2) {
		return -1;
	}

	if (*response_len < EMV_RAPDU_DATA_MAX) {
		return -2;
	}

	if (data) {
		const uint8_t* cmd_data = data;

		// For GET PROCESSING OPTIONS, if data is provided, ensure that it is
		// at least two bytes and doesn't exceed the C-APDU maximum data length
		if (data_len < 2 || data_len > EMV_CAPDU_DATA_MAX) {
			return -3;
		}

		// For GET PROCESSING OPTIONS, ensure that data starts with
		// Command Template (field 83)
		// See EMV 4.4 Book 3, 6.5.8.3
		// See EMV 4.4 Book 3, 10.1
		if (cmd_data[0] != EMV_TAG_83_COMMAND_TEMPLATE) {
			return -4;
		}
	} else {
		// Use empty Command Template (field 83)
		// See EMV 4.4 Book 3, 6.5.8.3
		// See EMV 4.4 Book 3, 10.1
		empty_cmd_data[0] = EMV_TAG_83_COMMAND_TEMPLATE;
		empty_cmd_data[1] = 0x00;

		data = empty_cmd_data;
		data_len = sizeof(empty_cmd_data);
	}

	// Build GET PROCESSING OPTIONS command
	c_apdu.CLA = 0x80; // See EMV 4.4 Book 3, 6.3.2
	c_apdu.INS = 0xA8; // See EMV 4.4 Book 3, 6.5.8.2, table 18
	c_apdu.P1  = 0x00; // See EMV 4.4 Book 3, 6.5.8.2, table 18
	c_apdu.P2  = 0x00; // See EMV 4.4 Book 3, 6.5.8.2, table 18
	c_apdu.Lc  = data_len;
	memcpy(c_apdu.data, data, c_apdu.Lc);
	c_apdu.data[c_apdu.Lc] = 0x00; // See EMV 4.4 Book 3, 6.5.8.2, table 18

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
		return -5;
	}
	if (r_apdu_len - 2 > *response_len) {
		return -6;
	}

	// Copy response from R-APDU
	*response_len = r_apdu_len - 2;
	memcpy(response, r_apdu, *response_len);

	return 0;
}
