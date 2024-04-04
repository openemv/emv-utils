/**
 * @file iso7816_apdu.h
 * @brief ISO/IEC 7816 Application Protocol Data Unit (APDU)
 *        definitions and helpers
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

#ifndef ISO7816_APDU_H
#define ISO7816_APDU_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

__BEGIN_DECLS

/// Maximum length of C-APDU data field in bytes
#define ISO7816_CAPDU_DATA_MAX (255)

/// Maximum length of C-APDU buffer in bytes
#define ISO7816_CAPDU_MAX (6 + ISO7816_CAPDU_DATA_MAX)

/// Maximum length of R-APDU data field in bytes
#define ISO7816_RAPDU_DATA_MAX (256)

/// Maximum length of R-APDU buffer in bytes
#define ISO7816_RAPDU_MAX (ISO7816_RAPDU_DATA_MAX + 2)

// ISO 7816 Class byte
// See ISO 7816-4:2005, 5.1.1
#define ISO7816_CLA_INTERINDUSTRY                               (0x00) ///< ISO 7816 C-APDU interindustry class
#define ISO7816_CLA_PROPRIETARY                                 (0x80) ///< ISO 7816 C-APDU proprietary class

// ISO 7816 SELECT (A4) command, P2 byte
// See ISO 7816-4:2005, 7.1.1, table 40
#define ISO7816_SELECT_P2_FILE_OCCURRENCE_MASK                  (0x03) ///< ISO 7816 SELECT P2 mask for file occurrence
#define ISO7816_SELECT_P2_FILE_OCCURRENCE_FIRST                 (0x00) ///< ISO 7816 SELECT: First occurrence
#define ISO7816_SELECT_P2_FILE_OCCURRENCE_LAST                  (0x01) ///< ISO 7816 SELECT: Last occurrence
#define ISO7816_SELECT_P2_FILE_OCCURRENCE_NEXT                  (0x02) ///< ISO 7816 SELECT: Next occurrence
#define ISO7816_SELECT_P2_FILE_OCCURRENCE_PREVIOUS              (0x03) ///< ISO 7816 SELECT: Previous occurrence

// ISO 7816 READ RECORD (B2 / B3) command, P2 byte
// See ISO 7816-4:2005, 7.3.3, table 49
#define ISO7816_READ_RECORD_P2_SHORT_EF_ID_MASK                 (0xF8) ///< ISO 7816 READ RECORD P2 mask for Short EF Identifier
#define ISO7816_READ_RECORD_P2_SHORT_EF_ID_SHIFT                (3) ///< ISO 7816 READ RECORD P2 shift for Short EF Identifier
#define ISO7816_READ_RECORD_P2_RECORD_NUMBER                    (0x04) ///< ISO 7816 READ RECORD P2 bit for record number in P1, otherwise record identifer in P1
#define ISO7816_READ_RECORD_P2_RECORD_ID_OCCURRENCE_MASK        (0x03) ///< ISO 7816 READ RECORD P2 mask for file occurrence
#define ISO7816_READ_RECORD_P2_RECORD_ID_OCCURRENCE_FIRST       (0x00) ///< ISO 7816 READ RECORD: First occurrence
#define ISO7816_READ_RECORD_P2_RECORD_ID_OCCURRENCE_LAST        (0x01) ///< ISO 7816 READ RECORD: Last occurrence
#define ISO7816_READ_RECORD_P2_RECORD_ID_OCCURRENCE_NEXT        (0x02) ///< ISO 7816 READ RECORD: Next occurrence
#define ISO7816_READ_RECORD_P2_RECORD_ID_OCCURRENCE_PREVIOUS    (0x03) ///< ISO 7816 READ RECORD: Previous occurrence
#define ISO7816_READ_RECORD_P2_RECORD_SEQUENCE_MASK             (0x03) ///< ISO 7816 READ RECORD P2 mask for record sequence
#define ISO7816_READ_RECORD_P2_RECORD_SEQUENCE_ONE              (0x00) ///< ISO 7816 READ RECORD: Read record P1
#define ISO7816_READ_RECORD_P2_RECORD_SEQUENCE_P1_TO_LAST       (0x01) ///< ISO 7816 READ RECORD: Read all records from P1 up to the last
#define ISO7816_READ_RECORD_P2_RECORD_SEQUENCE_LAST_TO_P1       (0x02) ///< ISO 7816 READ RECORD: Read all records from the last up to P1

/// ISO 7816 C-APDU cases. See ISO 7816-3:2006, 12.1.3
enum iso7816_apdu_case_t {
	ISO7816_APDU_CASE_1,                        ///< ISO 7816 C-APDU case 1: CLA, INS, P1, P2
	ISO7816_APDU_CASE_2S,                       ///< ISO 7816 C-APDU case 2 for short Le field: CLA, INS, P1, P2, Le
	ISO7816_APDU_CASE_2E,                       ///< ISO 7816 C-APDU case 2 for long Le field: CLA, INS, P1, P2, Le(3)
	ISO7816_APDU_CASE_3S,                       ///< ISO 7816 C-APDU case 3 for short Lc field: CLA, INS, P1, P2, Lc, Data(Lc)
	ISO7816_APDU_CASE_3E,                       ///< ISO 7816 C-APDU case 3 for long Lc field: CLA, INS, P1, P2, Lc(3), Data(Lc)
	ISO7816_APDU_CASE_4S,                       ///< ISO 7816 C-APDU case 4 for short Lc/Le fields: CLA, INS, P1, P2, Lc, Data(Lc), Le
	ISO7816_APDU_CASE_4E,                       ///< ISO 7816 C-APDU case 4 for long Lc/Le fields: CLA, INS, P1, P2, Lc(3), Data(Lc), Le(3)
};

/**
 * Determine ISO 7816 Command Application Protocol Data Unit (C-APDU) case
 * @remark See ISO 7816-3:2006, 12.1.3
 * @param c_apdu Command Application Protocol Data Unit (C-APDU) buffer
 * @param c_apdu_len Length of Application Protocol Data Unit (C-APDU) buffer in bytes
 * @return Less than zero for error. Otherwise @ref iso7816_apdu_case_t
 */
static inline int iso7816_apdu_case(const void* c_apdu, size_t c_apdu_len)
{
	if (!c_apdu || c_apdu_len < 4) {
		return -1;
	}

	if (c_apdu_len == 4) {
		return ISO7816_APDU_CASE_1;
	} else if (c_apdu_len == 5) {
		return ISO7816_APDU_CASE_2S;
	} else {
		// Extract byte C5 from header
		// See ISO 7816-3:2006, 12.1.3, table 13
		unsigned int C5 = *(uint8_t*)(c_apdu + 4);

		if (C5 != 0) { // If C5 is Lc
			if (5 + C5 == c_apdu_len) { // Le is absent
				return ISO7816_APDU_CASE_3S;
			}
			if (6 + C5 == c_apdu_len) { // Le is present
				return ISO7816_APDU_CASE_4S;
			}
		} else { // If C5 is zero
			if (c_apdu_len == 7) {
				return ISO7816_APDU_CASE_2E;
			}

			if (c_apdu_len > 7) {
				// Extract bytes C6C7 from header
				// See ISO 7816-3:2006, 12.1.3, table 13
				unsigned int C6 = *(uint8_t*)(c_apdu + 6);
				unsigned int C7 = *(uint8_t*)(c_apdu + 7);
				unsigned int C6C7 = (C6 << 8) + C7;

				if (C6C7) {
					if (7 + C6C7 == c_apdu_len) { // Le is absent
						return ISO7816_APDU_CASE_3E;
					}
					if (9 + C6C7 == c_apdu_len) { // Le is present
						return ISO7816_APDU_CASE_4E;
					}
				}
			}
		}
	}

	// Unknown C-APDU case
	return -2;
}

/// ISO 7816 C-APDU case 1 structure
struct iso7816_apdu_case_1_t {
	uint8_t CLA;                                ///< Class byte: indicates the type of command (interindustry vs proprietary, command chaining, secure messaging, logical channel)
	uint8_t INS;                                ///< Instruction byte: indicates the command to process
	uint8_t P1;                                 ///< Parameter byte 1
	uint8_t P2;                                 ///< Parameter byte 2
} __attribute__((packed));

/// ISO 7816 C-APDU case 2S structure
struct iso7816_apdu_case_2s_t {
	uint8_t CLA;                                ///< Class byte: indicates the type of command (interindustry vs proprietary, command chaining, secure messaging, logical channel)
	uint8_t INS;                                ///< Instruction byte: indicates the command to process
	uint8_t P1;                                 ///< Parameter byte 1
	uint8_t P2;                                 ///< Parameter byte 2
	uint8_t Le;                                 ///< Maximum length of R-APDU data (excluding SW1-SW2) in bytes
} __attribute__((packed));

/// ISO 7816 C-APDU case 3S structure
struct iso7816_apdu_case_3s_t {
	uint8_t CLA;                                ///< Class byte: indicates the type of command (interindustry vs proprietary, command chaining, secure messaging, logical channel)
	uint8_t INS;                                ///< Instruction byte: indicates the command to process
	uint8_t P1;                                 ///< Parameter byte 1
	uint8_t P2;                                 ///< Parameter byte 2
	uint8_t Lc;                                 ///< Length of C-APDU data field in bytes
	uint8_t data[ISO7816_CAPDU_DATA_MAX];       ///< Data field of length Lc
} __attribute__((packed));

/// Compute ISO 7816 C-APDU case 3S length in bytes
static inline size_t iso7816_apdu_case_3s_length(const struct iso7816_apdu_case_3s_t* c_apdu)
{
	return 5 + c_apdu->Lc;
}

/// ISO 7816 C-APDU case 4S structure
struct iso7816_apdu_case_4s_t {
	uint8_t CLA;                                ///< Class byte: indicates the type of command (interindustry vs proprietary, command chaining, secure messaging, logical channel)
	uint8_t INS;                                ///< Instruction byte: indicates the command to process
	uint8_t P1;                                 ///< Parameter byte 1
	uint8_t P2;                                 ///< Parameter byte 2
	uint8_t Lc;                                 ///< Length of C-APDU data field in bytes
	uint8_t data[ISO7816_CAPDU_DATA_MAX + 1];   ///< Data field of length Lc, followed by Le field
} __attribute__((packed));

/// Compute ISO 7816 C-APDU case 4S length in bytes
static inline size_t iso7816_apdu_case_4s_length(const struct iso7816_apdu_case_4s_t* c_apdu)
{
	return 6 + c_apdu->Lc;
}

/**
 * Determine whether SW1-SW2 indicates success (9000)
 * @remark See ISO 7816-4:2005, 5.1.3
 * @param SW1 Status byte 1
 * @param SW2 Status byte 2
 * @return Boolean indicating whether SW1-SW2 indicates success
 */
static inline bool iso7816_sw1sw2_is_success(uint8_t SW1, uint8_t SW2) { return SW1 == 0x90 && SW2 == 0x00; }

/**
 * Determine whether SW1-SW2 indicates normal processing (9000 or 61XX)
 * @remark See ISO 7816-4:2005, 5.1.3
 * @param SW1 Status byte 1
 * @param SW2 Status byte 2
 * @return Boolean indicating whether SW1-SW2 indicates normal processing
 */
static inline bool iso7816_sw1sw2_is_normal(uint8_t SW1, uint8_t SW2) { return iso7816_sw1sw2_is_success(SW1, SW2) || SW1 == 0x61; }

/**
 * Determine whether SW1-SW2 indicates warning processing (62XX or 62XX)
 * @remark See ISO 7816-4:2005, 5.1.3
 * @param SW1 Status byte 1
 * @param SW2 Status byte 2
 * @return Boolean indicating whether SW1-SW2 indicates warning processing
 */
static inline bool iso7816_sw1sw2_is_warning(uint8_t SW1, uint8_t SW2) { return SW1 == 0x62 || SW1 == 0x63; }

/**
 * Determine whether SW1-SW2 indicates error
 * @remark See ISO 7816-4:2005, 5.1.3
 * @param SW1 Status byte 1
 * @param SW2 Status byte 2
 * @return Boolean indicating whether SW1-SW2 indicates error
 */
static inline bool iso7816_sw1sw2_is_error(uint8_t SW1, uint8_t SW2) { return !iso7816_sw1sw2_is_normal(SW1, SW2) && !iso7816_sw1sw2_is_warning(SW1, SW2); }

__END_DECLS

#endif
