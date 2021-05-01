/**
 * @file iso7816_apdu.h
 * @brief ISO/IEC 7816 Application Protocol Data Unit (APDU)
 *        definitions and helpers
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

#ifndef ISO7816_APDU_H
#define ISO7816_APDU_H

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

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

#define ISO7816_APDU_DATA_MAX (255)             ///< Maximum length of C-APDU data field in bytes

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
	uint8_t data[ISO7816_APDU_DATA_MAX];        ///< Data field of length Lc
} __attribute__((packed));

/// ISO 7816 C-APDU case 4S structure
struct iso7816_apdu_case_4s_t {
	uint8_t CLA;                                ///< Class byte: indicates the type of command (interindustry vs proprietary, command chaining, secure messaging, logical channel)
	uint8_t INS;                                ///< Instruction byte: indicates the command to process
	uint8_t P1;                                 ///< Parameter byte 1
	uint8_t P2;                                 ///< Parameter byte 2
	uint8_t Lc;                                 ///< Length of C-APDU data field in bytes
	uint8_t data[ISO7816_APDU_DATA_MAX + 1];    ///< Data field of length Lc, followed by Le field
} __attribute__((packed));

__END_DECLS

#endif
