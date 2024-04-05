/**
 * @file iso7816_strings.c
 * @brief ISO/IEC 7816 string helper functions
 *
 * Copyright (c) 2021-2022, 2024 Leon Lynch
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

#include "iso7816_strings.h"
#include "iso7816_apdu.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

struct str_itr_t {
	char* ptr;
	size_t len;
};

static void iso7816_str_list_init(struct str_itr_t* itr, char* buf, size_t len)
{
	itr->ptr = buf;
	itr->len = len;
}

static void iso7816_str_list_add(struct str_itr_t* itr, const char* str)
{
	size_t str_len = strlen(str);

	// Ensure that the string, delimiter and NULL termination do not exceed
	// the remaining buffer length
	if (str_len + 2 > itr->len) {
		return;
	}

	// This is safe because we know str_len < itr->len
	memcpy(itr->ptr, str, str_len);

	// Delimiter
	itr->ptr[str_len] = '\n';
	++str_len;

	// Advance iterator
	itr->ptr += str_len;
	itr->len -= str_len;

	// NULL terminate string list
	*itr->ptr = 0;
}

static const char* iso7816_capdu_select_get_string(
	const uint8_t* c_apdu,
	size_t c_apdu_len,
	char* str,
	size_t str_len
)
{
	enum iso7816_apdu_case_t apdu_case;
	const struct iso7816_apdu_case_2s_t* apdu_case_2s = NULL;
	const struct iso7816_apdu_case_4s_t* apdu_case_4s = NULL;
	bool file_id_printable;
	char file_id_str[ISO7816_CAPDU_DATA_MAX * 2 + 1];
	const char* occurence_str;

	if (!c_apdu || !c_apdu_len || !str || !str_len) {
		// Invalid parameters
		return NULL;
	}

	if ((c_apdu[0] & ISO7816_CLA_PROPRIETARY) != 0 ||
		c_apdu[1] != 0xA4
	) {
		// Not SELECT
		return NULL;
	}

	// SELECT must be APDU case 2S or case 4S
	apdu_case = iso7816_apdu_case(c_apdu, c_apdu_len);
	switch (apdu_case) {
		case ISO7816_APDU_CASE_2S:
			apdu_case_2s = (void*)c_apdu;
			break;

		case ISO7816_APDU_CASE_4S:
			apdu_case_4s = (void*)c_apdu;
			break;

		default:
			// Invalid APDU case for SELECT
			return NULL;
	}

	// Determine whether master file is being selected
	// See ISO 7816-4:2005, 7.1.1
	if (c_apdu[2] == 0x00 && c_apdu[3] == 0x00) { // If P1 and P2 are zero
		// If no command data field or command data field is 3F00
		if (apdu_case_2s ||
			(
				apdu_case_4s &&
				apdu_case_4s->Lc == 2 &&
				apdu_case_4s->data[0] == 0x3F &&
				apdu_case_4s->data[1] == 0x00
			)
		) {
			snprintf(str, str_len, "SELECT Master File");
			return str;
		}
	} else if (!apdu_case_4s) {
		// Must be APDU case 4S if P1 or P2 are non-zero
		return NULL;
	}

	// Determine whether file identifier is printable
	file_id_printable = true;
	for (size_t i = 0; i < apdu_case_4s->Lc; ++i) {
		if (apdu_case_4s->data[i] < 0x20 || apdu_case_4s->data[i] > 0x7E) {
			file_id_printable = false;
			break;
		}
	}
	if (file_id_printable) {
		snprintf(file_id_str, sizeof(file_id_str), "%.*s", apdu_case_4s->Lc, apdu_case_4s->data);
	} else {
		for (size_t i = 0; i < apdu_case_4s->Lc; ++i) {
			snprintf(
				file_id_str + (i * 2),
				sizeof(file_id_str) - 1 - (i * 2),
				"%02X",
				apdu_case_4s->data[i]
			);
		}
	}

	// Decode P2 for file occurence
	// See ISO 7816-4:2005, 7.1.1, table 40
	switch (apdu_case_4s->P2 & ISO7816_SELECT_P2_FILE_OCCURRENCE_MASK) {
		case ISO7816_SELECT_P2_FILE_OCCURRENCE_FIRST:
			occurence_str = "first";
			break;

		case ISO7816_SELECT_P2_FILE_OCCURRENCE_LAST:
			occurence_str = "last";
			break;

		case ISO7816_SELECT_P2_FILE_OCCURRENCE_NEXT:
			occurence_str = "next";
			break;

		case ISO7816_SELECT_P2_FILE_OCCURRENCE_PREVIOUS:
			occurence_str = "previous";
			break;

		default:
			// Invalid P2
			return NULL;
	}

	// Stringify
	snprintf(str, str_len, "SELECT %s %s",
		occurence_str,
		file_id_str
	);

	return str;
}

static const char* iso7816_capdu_read_record_get_string(
	const uint8_t* c_apdu,
	size_t c_apdu_len,
	char* str,
	size_t str_len
)
{
	enum iso7816_apdu_case_t apdu_case;
	uint8_t P1;
	uint8_t P2;
	char record_str[32]; // "current record" / "record number XX" / "record identifer XX"
	unsigned int short_ef_id;

	if ((c_apdu[0] & ISO7816_CLA_PROPRIETARY) != 0 ||
		(c_apdu[1] != 0xB2 && c_apdu[1] != 0xB3)
	) {
		// Not READ RECORD
		return NULL;
	}

	// READ RECORD must be APDU case 2S/2E or case 4S/4E
	apdu_case = iso7816_apdu_case(c_apdu, c_apdu_len);
	switch (apdu_case) {
		case ISO7816_APDU_CASE_2S:
		case ISO7816_APDU_CASE_2E:
			if (c_apdu[1] != 0xB2) {
				// INS must be B2 for READ RECORD case 2S/2E
				return NULL;
			}
			break;

		case ISO7816_APDU_CASE_4S:
		case ISO7816_APDU_CASE_4E:
			if (c_apdu[1] != 0xB3) {
				// INS must be B3 for READ RECORD case 4S/4E
				return NULL;
			}
			break;

		default:
			// Invalid APDU case for READ RECORD
			return NULL;
	}

	// Extract P1 and P2 for convenience
	P1 = c_apdu[2];
	P2 = c_apdu[3];

	// P1 is record identifier/number (zero is current record)
	if (P1 != 0 && P1 != 0xFF) {
		if (P2 & ISO7816_READ_RECORD_P2_RECORD_NUMBER) {
			snprintf(record_str, sizeof(record_str), "record number %u", P1);
		} else {
			snprintf(record_str, sizeof(record_str), "record identifier %u", P1);
		}
	} else if (P1 == 0) {
		snprintf(record_str, sizeof(record_str), "current record");
	} else {
		// Invalid P1
		return NULL;
	}

	// Decode P2
	// See ISO 7816-4:2005, 7.3.3, table 49
	short_ef_id = (P2 & ISO7816_READ_RECORD_P2_SHORT_EF_ID_MASK)
		>> ISO7816_READ_RECORD_P2_SHORT_EF_ID_SHIFT;
	if (P2 & ISO7816_READ_RECORD_P2_RECORD_NUMBER) {
		// P1 is record number and P2 describes record sequence and also
		// output string format
		switch (P2 & ISO7816_READ_RECORD_P2_RECORD_SEQUENCE_MASK) {
			case ISO7816_READ_RECORD_P2_RECORD_SEQUENCE_ONE:
				snprintf(str, str_len, "READ RECORD from short EF %u, %s",
					short_ef_id,
					record_str
				);
				return str;

			case ISO7816_READ_RECORD_P2_RECORD_SEQUENCE_P1_TO_LAST:
				snprintf(str, str_len, "READ RECORD from short EF %u, "
					"all records from %s up to last",
					short_ef_id,
					record_str
				);
				return str;

			case ISO7816_READ_RECORD_P2_RECORD_SEQUENCE_LAST_TO_P1:
				snprintf(str, str_len, "READ RECORD from short EF %u, "
					"all records from the last up to %s",
					short_ef_id,
					record_str
				);
				return str;

			default:
				// Invalid P2
				return NULL;
		}
	} else {
		// P1 is record identifier and P2 describes record occurence
		const char* occurence_str;
		switch (P2 & ISO7816_READ_RECORD_P2_RECORD_ID_OCCURRENCE_MASK) {
			case ISO7816_READ_RECORD_P2_RECORD_ID_OCCURRENCE_FIRST:
				occurence_str = "first";
				break;

			case ISO7816_READ_RECORD_P2_RECORD_ID_OCCURRENCE_LAST:
				occurence_str = "last";
				break;

			case ISO7816_READ_RECORD_P2_RECORD_ID_OCCURRENCE_NEXT:
				occurence_str = "next";
				break;

			case ISO7816_READ_RECORD_P2_RECORD_ID_OCCURRENCE_PREVIOUS:
				occurence_str = "previous";
				break;

			default:
				// Invalid P2
				return NULL;
		}

		snprintf(str, str_len, "READ RECORD from short EF %u, "
			"%s occurence of %s",
			short_ef_id,
			occurence_str,
			record_str
		);
		return str;
	}
}

static const char* iso7816_capdu_put_data_get_string(
	const uint8_t* c_apdu,
	size_t c_apdu_len,
	char* str,
	size_t str_len
)
{
	enum iso7816_apdu_case_t apdu_case;
	struct iso7816_apdu_case_3s_t* apdu_case_3s;
	uint8_t P1;
	uint8_t P2;

	if ((c_apdu[0] & ISO7816_CLA_PROPRIETARY) != 0 ||
		(c_apdu[1] != 0xDA && c_apdu[1] != 0xDB)
	) {
		// Not PUT DATA
		return NULL;
	}

	// PUT DATA must be APDU case 3S/3E but only support 3S for now
	// See ISO 7816-4:2005, 7.4.3, table 64
	apdu_case = iso7816_apdu_case(c_apdu, c_apdu_len);
	switch (apdu_case) {
		case ISO7816_APDU_CASE_3S:
			apdu_case_3s = (void*)c_apdu;
			break;

		case ISO7816_APDU_CASE_3E:
			// Unsupported APDU case for PUT DATA
			snprintf(str, str_len, "PUT DATA");
			return str;

		default:
			// Invalid APDU case for PUT DATA
			return NULL;
	}

	// Extract P1 and P2 for convenience
	P1 = apdu_case_3s->P1;
	P2 = apdu_case_3s->P2;

	// Decode P1 and P2
	// See ISO 7816-4:2005, 7.4.1, table 62
	if ((c_apdu[1] & 0x01) == 0) {
		// Even INS code
		if (P1 == 0 && P2 >= 0x40 && P2 <= 0xFE) {
			// P2 is 1-byte BER-TLV tag
			snprintf(str, str_len, "PUT DATA for BER-TLV field %02X", P2);
			return str;
		} else if (P1 == 1) {
			// P2 is proprietary
			snprintf(str, str_len, "PUT DATA for proprietary identifier %02X", P2);
			return str;
		} else if (P1 == 2 && P2 >= 0x01 && P2 <= 0xFE) {
			// P2 is 1-byte SIMPLE-TLV tag
			snprintf(str, str_len, "PUT DATA for SIMPLE-TLV field %02X", P2);
			return str;
		} else if (P1 >= 0x40) {
			// P1-P2 is 2-byte BER-TLV tag
			snprintf(str, str_len, "PUT DATA for BER-TLV field %02X%02X", P1, P2);
			return str;
		}
	} else {
		// Odd INS code
		if (P1 == 0x00 && (P2 & 0xE0) == 0x00 && // Highest 11 bits of P1-P2 are unset
			(P2 & 0x1F) != 0x00 && (P2 & 0x1F) != 0x1F // Lowest 5 bits are not equal
		) {
			// P2 is short EF identifier
			snprintf(str, str_len, "PUT DATA for short EF %u", P2);
			return str;
		} else if (P1 == 0x3F && P2 == 0xFF) { // P1-P2 is 3FFF
			// P1-P2 is current DF
			snprintf(str, str_len, "PUT DATA for current DF");
			return str;
		} else if (P1 == 0x00 && P2 == 0x00 && // P1-P2 is 0000
			apdu_case_3s->Lc == 0 // No command data
		) {
			// P1-P2 is current EF
			snprintf(str, str_len, "PUT DATA for current EF");
			return str;
		} else if (P1 != 0x00 || P2 != 0x00) { // P1-P2 is not 0000
			// P1-P2 is file identifier
			snprintf(str, str_len, "PUT DATA for file %02X%02X", P1, P2);
			return str;
		}
	}

	// Unknown P1 and P2
	snprintf(str, str_len, "PUT DATA");
	return str;
}

const char* iso7816_capdu_get_string(
	const void* c_apdu,
	size_t c_apdu_len,
	char* str,
	size_t str_len
)
{
	const uint8_t* c_apdu_hdr = c_apdu;
	const char* ins_str;

	if (!c_apdu || !c_apdu_len || !str || !str_len) {
		// Invalid parameters
		return NULL;
	}
	str[0] = 0; // NULL terminate

	if (str_len < 4) {
		// C-APDU must be least 4 bytes
		// See ISO 7816-4:2005, 5.1
		return NULL;
	}

	if (c_apdu_hdr[0] == 0xFF) {
		// Class byte 'FF' is invalid
		// See ISO 7816-4:2005, 5.1.1
		return NULL;
	}

	// Determine whether it is interindustry or proprietary
	// See ISO 7816-4:2005, 5.1.1
	if ((c_apdu_hdr[0] & ISO7816_CLA_PROPRIETARY) == 0) {
		// Interindustry class
		// Decode INS byte according to ISO 7816-4:2005, 5.1.2, table 4.2
		switch (c_apdu_hdr[1]) {
			case 0x04: ins_str = "DEACTIVATE FILE"; break;
			case 0x0C: ins_str = "ERASE RECORD"; break;
			case 0x0E:
			case 0x0F: ins_str = "ERASE BINARY"; break;
			case 0x10: ins_str = "PERFORM SCQL OPERATION"; break;
			case 0x12: ins_str = "PERFORM TRANSACTION OPERATION"; break;
			case 0x14: ins_str = "PERFORM USER OPERATION"; break;
			case 0x20:
			case 0x21: ins_str = "VERIFY"; break;
			case 0x22: ins_str = "MANAGE SECURITY ENVIRONMENT"; break;
			case 0x24: ins_str = "CHANGE REFERENCE DATA"; break;
			case 0x26: ins_str = "DISABLE VERIFICATION REQUIREMENT"; break;
			case 0x28: ins_str = "ENABLE VERIFICATION REQUIREMENT"; break;
			case 0x2A: ins_str = "PERFORM SECURITY OPERATION"; break;
			case 0x2C: ins_str = "RESET RETRY COUNTER"; break;
			case 0x44: ins_str = "ACTIVATE FILE"; break;
			case 0x46: ins_str = "GENERATE ASYMMETRIC KEY PAIR"; break;
			case 0x70: ins_str = "MANAGE CHANNEL"; break;
			case 0x82: ins_str = "EXTERNAL AUTHENTICATE"; break;
			case 0x84: ins_str = "GET CHALLENGE"; break;
			case 0x86:
			case 0x87: ins_str = "GENERAL AUTHENTICATE"; break;
			case 0x88: ins_str = "INTERNAL AUTHENTICATE"; break;
			case 0xA0:
			case 0xA1: ins_str = "SEARCH BINARY"; break;
			case 0xA2: ins_str = "SEARCH RECORD"; break;
			case 0xA4: return iso7816_capdu_select_get_string(c_apdu, c_apdu_len, str, str_len);
			case 0xB0:
			case 0xB1: ins_str = "READ BINARY"; break;
			case 0xB2:
			case 0xB3: return iso7816_capdu_read_record_get_string(c_apdu, c_apdu_len, str, str_len);
			case 0xC0: ins_str = "GET RESPONSE"; break;
			case 0xC2:
			case 0xC3: ins_str = "ENVELOPE"; break;
			case 0xCA:
			case 0xCB: ins_str = "GET DATA"; break;
			case 0xD0:
			case 0xD1: ins_str = "WRITE BINARY"; break;
			case 0xD2: ins_str = "WRITE RECORD"; break;
			case 0xD6:
			case 0xD7: ins_str = "UPDATE BINARY"; break;
			case 0xDA:
			case 0xDB: return iso7816_capdu_put_data_get_string(c_apdu, c_apdu_len, str, str_len);
			case 0xDC:
			case 0xDD: ins_str = "UPDATE RECORD"; break;
			case 0xE0: ins_str = "CREATE FILE"; break;
			case 0xE2: ins_str = "APPEND RECORD"; break;
			case 0xE4: ins_str = "DELETE FILE"; break;
			case 0xE6: ins_str = "TERMINATE DF"; break;
			case 0xE8: ins_str = "TERMINATE EF"; break;
			case 0xFE: ins_str = "TERMINATE CARD USAGE"; break;

			default: return NULL; // Unknown command
		}
	} else {
		return NULL; // Unknown proprietary command
	}

	strncpy(str, ins_str, str_len);
	str[str_len - 1] = 0;

	return str;
}

const char* iso7816_sw1sw2_get_string(uint8_t SW1, uint8_t SW2, char* str, size_t str_len)
{
	int r;
	char* str_ptr = str;

	if (!str || !str_len) {
		// Invalid parameters
		return NULL;
	}

	// Normal processing (see ISO 7816-4:2005, 5.1.3)
	if (SW1 == 0x90 && SW2 == 0x00) {
		snprintf(str, str_len, "Normal");
		return str;
	}
	if (SW1 == 0x61) {
		snprintf(str, str_len, "Normal: %u data bytes remaining", SW2);
		return str;
	}

	// According to ISO 7816-4:2005, 5.1.3:
	// Any value different from 6XXX and 9XXX is invalid
	// and any value 60XX is also invalid
	// Also see ISO 7816-3:2006, 10.3.3
	if ((SW1 & 0xF0) != 0x60 && (SW1 & 0xF0) != 0x90) {
		snprintf(str, str_len, "Invalid");
		return str;
	}
	if (SW1 == 0x60) {
		snprintf(str, str_len, "Invalid");
		return str;
	}

	// According to ISO 7816-4:2005, 5.1.3:
	// 67XX, 6BXX, 6DXX, 6EXX, 6FXX and 9XXX are proprietary, except for
	// 6700, 6B00, 6D00, 6E00, 6F00 and 9000 that are interindustry
	if (((
			SW1 == 0x67 ||
			SW1 == 0x6B ||
			SW1 == 0x6D ||
			SW1 == 0x6E ||
			SW1 == 0x6F
		) && SW2 != 0x00) ||
		(SW1 & 0xF0) == 0x90 // 9000 is checked for earlier
	) {
		snprintf(str, str_len, "Proprietary");
		return str;
	}

	// Add prefix for high level meaning of SW1
	// See ISO 7816-4:2005, 5.1.3, table 5
	switch (SW1) {
		case 0x62:
		case 0x63:
			r = snprintf(str_ptr, str_len, "Warning: ");
			break;

		case 0x64:
		case 0x65:
		case 0x66:
			r = snprintf(str_ptr, str_len, "Execution error: ");
			break;

		case 0x67:
		case 0x68:
		case 0x69:
		case 0x6A:
		case 0x6B:
		case 0x6C:
		case 0x6D:
		case 0x6E:
		case 0x6F:
			r = snprintf(str_ptr, str_len, "Checking error: ");
			break;

		default:
			// This should never happen
			return NULL;
	}
	if (r >= str_len) {
		// Not enough space in string buffer; return truncated content
		return str;
	}
	str_ptr += r;
	str_len -= r;

	// Warning processing (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x62) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "State of non-volatile memory is unchanged"); break;
			case 0x81: snprintf(str_ptr, str_len, "Part of returned data may be corrupted"); break;
			case 0x82: snprintf(str_ptr, str_len, "End of file or record reached before reading Ne bytes"); break;
			case 0x83: snprintf(str_ptr, str_len, "Selected file deactivated"); break;
			case 0x84: snprintf(str_ptr, str_len, "File control information not formatted according to ISO 7816-4:2005, 5.3.3"); break;
			case 0x85: snprintf(str_ptr, str_len, "Selected file in termination state"); break;
			case 0x86: snprintf(str_ptr, str_len, "No input data available from a sensor on the card"); break;
			default:
				// Card-originated queries (see ISO 7816-4:2005, 8.6.1)
				if (SW2 >= 0x02 && SW2 <= 0x80) {
					snprintf(str_ptr, str_len, "Card-originated query of %u bytes", SW2);
				} else {
					snprintf(str_ptr, str_len, "Unknown");
				}
		}

		return str;
	}

	// Warning processing (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x63) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "State of non-volatile memory has changed"); break;
			case 0x81: snprintf(str_ptr, str_len, "File filled up by the last write"); break;
			default:
				if ((SW2 & 0xF0) == 0xC0) {
					snprintf(str_ptr, str_len, "Counter is %u", (SW2 & 0x0F));
				} else {
					snprintf(str_ptr, str_len, "Unknown");
				}
		}

		return str;
	}

	// Execution error (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x64) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "State of non-volatile memory is unchanged"); break;
			case 0x01: snprintf(str_ptr, str_len, "Immediate response required by card"); break;
			default:
				// Card-originated queries (see ISO 7816-4:2005, 8.6.1)
				if (SW2 >= 0x02 && SW2 <= 0x80) {
					snprintf(str_ptr, str_len, "Card-originated query of %u bytes", SW2);
				} else {
					snprintf(str_ptr, str_len, "Unknown");
				}
		}

		return str;
	}

	// Execution error (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x65) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "State of non-volatile memory has changed"); break;
			case 0x81: snprintf(str_ptr, str_len, "Memory failure"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Execution error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x66) {
		snprintf(str_ptr, str_len, "Security error 0x%02X", SW2);
		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x67) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Wrong length"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x68) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Functions in CLA not supported"); break;
			case 0x81: snprintf(str_ptr, str_len, "Logical channel not supported"); break;
			case 0x82: snprintf(str_ptr, str_len, "Secure messaging not supported"); break;
			case 0x83: snprintf(str_ptr, str_len, "Last command of the chain expected"); break;
			case 0x84: snprintf(str_ptr, str_len, "Command chaining not supported"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x69) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Command not allowed"); break;
			case 0x81: snprintf(str_ptr, str_len, "Command incompatible with file structure"); break;
			case 0x82: snprintf(str_ptr, str_len, "Security status not satisfied"); break;
			case 0x83: snprintf(str_ptr, str_len, "Authentication method blocked"); break;
			case 0x84: snprintf(str_ptr, str_len, "Reference data not usable"); break;
			case 0x85: snprintf(str_ptr, str_len, "Conditions of use not satisfied"); break;
			case 0x86: snprintf(str_ptr, str_len, "Command not allowed (no current EF)"); break;
			case 0x87: snprintf(str_ptr, str_len, "Expected secure messaging data objects missing"); break;
			case 0x88: snprintf(str_ptr, str_len, "Incorrect secure messaging data objects"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 6)
	if (SW1 == 0x6A) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Wrong parameters P1-P2"); break;
			case 0x80: snprintf(str_ptr, str_len, "Incorrect parameters in the command data field"); break;
			case 0x81: snprintf(str_ptr, str_len, "Function not supported"); break;
			case 0x82: snprintf(str_ptr, str_len, "File or application not found"); break;
			case 0x83: snprintf(str_ptr, str_len, "Record not found"); break;
			case 0x84: snprintf(str_ptr, str_len, "Not enough memory space in the file"); break;
			case 0x85: snprintf(str_ptr, str_len, "Nc inconsistent with TLV structure"); break;
			case 0x86: snprintf(str_ptr, str_len, "Incorrect parameters P1-P2"); break;
			case 0x87: snprintf(str_ptr, str_len, "Nc inconsistent with parameters P1-P2"); break;
			case 0x88: snprintf(str_ptr, str_len, "Referenced data or reference data not found"); break;
			case 0x89: snprintf(str_ptr, str_len, "File already exists"); break;
			case 0x8A: snprintf(str_ptr, str_len, "DF name already exists"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x6B) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Wrong parameters P1-P2"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x6C) {
		snprintf(str_ptr, str_len, "Wrong Le field (%u data bytes available)", SW2);
		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x6D) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Instruction code not supported or invalid"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x6E) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "Class not supported"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// Checking error (see ISO 7816-4:2005, 5.1.3, table 5)
	if (SW1 == 0x6F) {
		switch (SW2) {
			case 0x00: snprintf(str_ptr, str_len, "No precise diagnosis"); break;
			default: snprintf(str_ptr, str_len, "Unknown"); break;
		}

		return str;
	}

	// According to ISO 7816-4:2005, 5.1.3, all other values are reserved
	snprintf(str, str_len, "Unknown error");
	return str;
}

const char* iso7816_lcs_get_string(uint8_t lcs)
{
	// See ISO 7816-4:2005, 5.3.3.2, table 13

	if (lcs == ISO7816_LCS_NONE) {
		return "No information given";
	}

	if (lcs == ISO7816_LCS_CREATION) {
		return "Creation state";
	}

	if (lcs == ISO7816_LCS_INITIALISATION) {
		return "Initialisation state";
	}

	if ((lcs & ISO7816_LCS_OPERATIONAL_MASK) == ISO7816_LCS_ACTIVATED) {
		return "Operational state (activated)";
	}

	if ((lcs & ISO7816_LCS_OPERATIONAL_MASK) == ISO7816_LCS_DEACTIVATED) {
		return "Operational state (deactivated)";
	}

	if ((lcs & ISO7816_LCS_TERMINATION_MASK) == ISO7816_LCS_TERMINATION) {
		return "Termination state";
	}

	return "Proprietary";
}

int iso7816_card_service_data_get_string_list(uint8_t card_service_data, char* str, size_t str_len)
{
	struct str_itr_t itr;

	iso7816_str_list_init(&itr, str, str_len);

	// Application selection (see ISO 7816-4:2005, 8.1.1.2.3, table 85)
	if ((card_service_data & ISO7816_CARD_SERVICE_APP_SEL_FULL_DF)) {
		iso7816_str_list_add(&itr, "Application selection: by full DF name");
	}
	if ((card_service_data & ISO7816_CARD_SERVICE_APP_SEL_PARTIAL_DF)) {
		iso7816_str_list_add(&itr, "Application selection: by partial DF name");
	}

	// BER-TLV data objects availability (see ISO 7816-4:2005, 8.1.1.2.3, table 85)
	if ((card_service_data & ISO7816_CARD_SERVICE_BER_TLV_EF_DIR)) {
		iso7816_str_list_add(&itr, "BER-TLV data objects available in EF.DIR");
	}
	if ((card_service_data & ISO7816_CARD_SERVICE_BER_TLV_EF_ATR)) {
		iso7816_str_list_add(&itr, "BER-TLV data objects available in EF.ATR");
	}

	// EF.DIR and EF.ATR access services (see ISO 7816-4:2005, 8.1.1.2.3, table 85)
	switch ((card_service_data & ISO7816_CARD_SERVICE_ACCESS_MASK)) {
		case ISO7816_CARD_SERVICE_ACCESS_READ_BINARY:
			iso7816_str_list_add(&itr, "EF.DIR and EF.ATR access services: by READ BINARY command (transparent structure)");
			break;

		case ISO7816_CARD_SERVICE_ACCESS_READ_RECORD:
			iso7816_str_list_add(&itr, "EF.DIR and EF.ATR access services: by READ RECORD(S) command (record structure)");
			break;

		case ISO7816_CARD_SERVICE_ACCESS_GET_DATA:
			iso7816_str_list_add(&itr, "EF.DIR and EF.ATR access services: by GET DATA command (TLV structure)");
			break;

		default:
			iso7816_str_list_add(&itr, "EF.DIR and EF.ATR access services: Unknown value");
			break;
	}

	// Master file presence (see ISO 7816-4:2005, 8.1.1.2.3, table 85)
	switch ((card_service_data & ISO7816_CARD_SERVICE_MF_MASK)) {
		case ISO7816_CARD_SERVICE_WITHOUT_MF:
			iso7816_str_list_add(&itr, "Card without MF");
			break;

		case ISO7816_CARD_SERVICE_WITH_MF:
			iso7816_str_list_add(&itr, "Card with MF");
			break;

		default:
			return -1;
	}

	return 0;
}

int iso7816_card_capabilities_get_string_list(
	const uint8_t* card_capabilities,
	size_t card_capabilities_len,
	char* str,
	size_t str_len
)
{
	struct str_itr_t itr;

	if (!card_capabilities || !card_capabilities_len) {
		return -1;
	}

	iso7816_str_list_init(&itr, str, str_len);

	// DF selection (see ISO 7816-4:2005, 8.1.1.2.7, table 86)
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_DF_SEL_FULL_DF)) {
		iso7816_str_list_add(&itr, "DF selection: by full DF NAME");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_DF_SEL_PARTIAL_DF)) {
		iso7816_str_list_add(&itr, "DF selection: by partial DF NAME");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_DF_SEL_PATH)) {
		iso7816_str_list_add(&itr, "DF selection: by path");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_DF_SEL_FILE_ID)) {
		iso7816_str_list_add(&itr, "DF selection: by file identifier");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_DF_SEL_IMPLICIT)) {
		iso7816_str_list_add(&itr, "DF selection: implicit");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_SHORT_EF_ID)) {
		iso7816_str_list_add(&itr, "Short EF identifier supported");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_RECORD_NUMBER)) {
		iso7816_str_list_add(&itr, "Record number supported");
	}
	if ((card_capabilities[0] & ISO7816_CARD_CAPS_RECORD_ID)) {
		iso7816_str_list_add(&itr, "Record identifier supported");
	}

	// Data coding byte (see ISO 7816-4:2005, 8.1.1.2.7, table 86)
	if (card_capabilities_len < 2) {
		return 0;
	}
	if ((card_capabilities[1] & ISO7816_CARD_CAPS_EF_TLV)) {
		iso7816_str_list_add(&itr, "EFs of TLV structure supported");
	}

	switch ((card_capabilities[1] & ISO7816_CARD_CAPS_WRITE_FUNC_MASK)) {
		case ISO7816_CARD_CAPS_WRITE_FUNC_ONE_TIME:
			iso7816_str_list_add(&itr, "Behaviour of write functions: one-time write");
			break;

		case ISO7816_CARD_CAPS_WRITE_FUNC_PROPRIETARY:
			iso7816_str_list_add(&itr, "Behaviour of write functions: proprietary");
			break;

		case ISO7816_CARD_CAPS_WRITE_FUNC_OR:
			iso7816_str_list_add(&itr, "Behaviour of write functions: write OR");
			break;

		case ISO7816_CARD_CAPS_WRITE_FUNC_AND:
			iso7816_str_list_add(&itr, "Behaviour of write functions: write AND");
			break;

		default:
			return -1;
	}

	switch ((card_capabilities[1] & ISO7816_CARD_CAPS_BER_TLV_FF_MASK)) {
		case ISO7816_CARD_CAPS_BER_TLV_FF_VALID:
			iso7816_str_list_add(&itr, "FF as first byte of BER-TLV tag is valid");
			break;

		case ISO7816_CARD_CAPS_BER_TLV_FF_INVALID:
			iso7816_str_list_add(&itr, "FF as first byte of BER-TLV tag is invalid / padding");
			break;

		default:
			return -1;
	}

	unsigned int data_unit_size_field;
	data_unit_size_field = card_capabilities[1] & ISO7816_CARD_CAPS_DATA_UNIT_SIZE_MASK;
	if (data_unit_size_field) {
		unsigned int data_unit_size;

		// See ISO 7816-4:2005, 8.1.1.2.7, table 86
		// data_unit_size = 2 ^ data_unit_size_field quartets
		//                = (2 ^ data_unit_size_field) / 2 octets
		//                = 1 << (data_unit_size_field - 1) bytes
		data_unit_size = 1 << (data_unit_size_field - 1);

		// Stringify
		char tmp[32];
		snprintf(tmp, sizeof(tmp), "Data unit size: %u bytes", data_unit_size);

		iso7816_str_list_add(&itr, tmp);
	}

	// Command chaining, length fields, logical channels (see ISO 7816-4:2005, 8.1.1.2.7, table 87)
	if (card_capabilities_len < 3) {
		return 0;
	}
	if ((card_capabilities[2] & ISO7816_CARD_CAPS_COMMAND_CHAINING)) {
		iso7816_str_list_add(&itr, "Command chaining");
	}
	if ((card_capabilities[2] & ISO7816_CARD_CAPS_EXTENDED_LC_LE)) {
		iso7816_str_list_add(&itr, "Extended Lc and Le fields");
	}

	switch ((card_capabilities[2] & ISO7816_CARD_CAPS_CHAN_NUM_ASSIGN_MASK)) {
		case ISO7816_CARD_CAPS_CHAN_NUM_ASSIGN_CARD:
			iso7816_str_list_add(&itr, "Logical channel number assignment: by the card");
			break;

		case ISO7816_CARD_CAPS_CHAN_NUM_ASSIGN_IFD:
			iso7816_str_list_add(&itr, "Logical channel number assignment: by the interface device");
			break;

		case ISO7816_CARD_CAPS_CHAN_NUM_ASSIGN_NONE:
			iso7816_str_list_add(&itr, "No logical channel");
			break;

		default:
			return -1;
	}

	unsigned int max_logical_channels;
	max_logical_channels = card_capabilities[2] & ISO7816_CARD_CAPS_MAX_CHAN_MASK;
	if (max_logical_channels == 0x7) {
		iso7816_str_list_add(&itr, "Maximum number of logical channels: 8 or more");
	} else {
		// Stringify
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "Maximum number of logical channels: %u", max_logical_channels + 1);

		iso7816_str_list_add(&itr, tmp);
	}

	return 0;
}
