/**
 * @file iso7816_strings.h
 * @brief ISO/IEC 7816 string helper functions
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

#ifndef ISO7816_STRINGS_H
#define ISO7816_STRINGS_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

// Life cycle status bits
#define ISO7816_LCS_NONE                        (0x00) ///< No life cycle information
#define ISO7816_LCS_CREATION                    (0x01) ///< Creation state
#define ISO7816_LCS_INITIALISATION              (0x03) ///< Initialisation state
#define ISO7816_LCS_OPERATIONAL_MASK            (0xFD) ///< Operational state bitmask
#define ISO7816_LCS_ACTIVATED                   (0x05) ///< Operational state: activated
#define ISO7816_LCS_DEACTIVATED                 (0x04) ///< Operational state: deactivated
#define ISO7816_LCS_TERMINATION_MASK            (0xFC) ///< Termination state bitmask
#define ISO7816_LCS_TERMINATION                 (0x0C) ///< Termination state

// Card service data
#define ISO7816_CARD_SERVICE_APP_SEL_MASK       (0xC0) ///< Card service data mask for application selection (see ISO 7816-4:2005, 8.1.1.2.3, table 85)
#define ISO7816_CARD_SERVICE_APP_SEL_FULL_DF    (0x80) ///< Card service data: application selection by full DF name
#define ISO7816_CARD_SERVICE_APP_SEL_PARTIAL_DF (0x40) ///< Card service data: application selection by partial DF name
#define ISO7816_CARD_SERVICE_BER_TLV_MASK       (0x30) ///< Card service data mask for available BER-TLV data objects (see ISO 7816-4:2005, 8.1.1.2.3, table 85)
#define ISO7816_CARD_SERVICE_BER_TLV_EF_DIR     (0x20) ///< Card service data: BER-TLV data objects in EF.DIR
#define ISO7816_CARD_SERVICE_BER_TLV_EF_ATR     (0x10) ///< Card service data: BER-TLV data objects in EF.ATR
#define ISO7816_CARD_SERVICE_ACCESS_MASK        (0x0E) ///< Card service data mask for EF.DIR and EF.ATR access services
#define ISO7816_CARD_SERVICE_ACCESS_READ_BINARY (0x08) ///< Card service data: EF.DIR and EF.ATR access by the READ BINARY command (transparent structure)
#define ISO7816_CARD_SERVICE_ACCESS_READ_RECORD (0x00) ///< Card service data: EF.DIR and EF.ATR access by the READ RECORD(s) command (record structure)
#define ISO7816_CARD_SERVICE_ACCESS_GET_DATA    (0x04) ///< Card service data: EF.DIR and EF.ATR access by the GET DATA command (TLV structure)
#define ISO7816_CARD_SERVICE_MF_MASK            (0x01) ///< Card service data mask for master file (MF) presence
#define ISO7816_CARD_SERVICE_WITHOUT_MF         (0x01) ///< Card service data: card without MF
#define ISO7816_CARD_SERVICE_WITH_MF            (0x00) ///< Card service data: card with MF

/**
 * Stringify ISO/IEC 7816 life cycle status byte (LCS)
 * @param lcs Life cycle status byte (LCS)
 * @return String. NULL for error.
 */
const char* iso7816_lcs_get_string(uint8_t lcs);

/**
 * Stringify ISO/IEC 7816 card service data byte
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param card_service_data Card service data byte
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int iso7816_card_service_data_get_string_list(uint8_t card_service_data, char* str, size_t str_len);

__END_DECLS

#endif
