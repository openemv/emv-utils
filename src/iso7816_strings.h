/**
 * @file iso7816_strings.h
 * @brief ISO/IEC 7816 string helper functions
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

// Card capabilities byte 1 (selection methods, see ISO 7816-4:2005, 8.1.1.2.7, table 86)
#define ISO7816_CARD_CAPS_DF_SEL_MASK           (0xF8) ///< Card capabilities mask for DF selection
#define ISO7816_CARD_CAPS_DF_SEL_FULL_DF        (0x80) ///< Card capabilities: DF selection by full DF NAME
#define ISO7816_CARD_CAPS_DF_SEL_PARTIAL_DF     (0x40) ///< Card capabilities: DF selection by partial DF NAME
#define ISO7816_CARD_CAPS_DF_SEL_PATH           (0x20) ///< Card capabilities: DF selection by path
#define ISO7816_CARD_CAPS_DF_SEL_FILE_ID        (0x10) ///< Card capabilities: DF selection by file identifier
#define ISO7816_CARD_CAPS_DF_SEL_IMPLICIT       (0x08) ///< Card capabilities: implicit DF selection
#define ISO7816_CARD_CAPS_SHORT_EF_ID           (0x04) ///< Card capabilities: short EF identifier supported
#define ISO7816_CARD_CAPS_RECORD_NUMBER         (0x02) ///< Card capabilities: record number supported
#define ISO7816_CARD_CAPS_RECORD_ID             (0x01) ///< Card capabilities: record identifier supported

// Card capabilities byte 2 (data coding byte, see ISO 7816-4:2005, 8.1.1.2.7, table 87)
#define ISO7816_CARD_CAPS_EF_TLV                (0x80) ///< Card capabilities: EFs of TLV structure supported
#define ISO7816_CARD_CAPS_WRITE_FUNC_MASK       (0x60) ///< Card capabilities mask for behaviour of write functions
#define ISO7816_CARD_CAPS_WRITE_FUNC_ONE_TIME   (0x00) ///< Card capabilities: one-time write
#define ISO7816_CARD_CAPS_WRITE_FUNC_PROPRIETARY (0x20) ///< Card capabilities: proprietary
#define ISO7816_CARD_CAPS_WRITE_FUNC_OR         (0x40) ///< Card capabilities: write OR
#define ISO7816_CARD_CAPS_WRITE_FUNC_AND        (0x60) ///< Card capabilities: write AND
#define ISO7816_CARD_CAPS_BER_TLV_FF_MASK       (0x10) ///< Card capabilities mask for validity of FF as first byte of BER-TLV tags
#define ISO7816_CARD_CAPS_BER_TLV_FF_VALID      (0x10) ///< Card capabilities: FF as first byte of BER-TLV tag is valid (used for long form, private class, constructed encoding)
#define ISO7816_CARD_CAPS_BER_TLV_FF_INVALID    (0x00) ///< Card capabilities: FF as first byte of BER-TLV tag is invalid (used for padding)
#define ISO7816_CARD_CAPS_DATA_UNIT_SIZE_MASK   (0x0F) ///< Card capabilities: data unit size in quartets as a power of 2

// Card capabilities byte 3 (command chaining, length fields, logical channels, see ISO 7816-4:2005, 8.1.1.2.7, table 87)
#define ISO7816_CARD_CAPS_COMMAND_CHAINING      (0x80) ///< Card capabilities: command chaining
#define ISO7816_CARD_CAPS_EXTENDED_LC_LE        (0x40) ///< Card capabilities: extended Lc and Le fields
#define ISO7816_CARD_CAPS_CHAN_NUM_ASSIGN_MASK  (0x18) ///< Card capabilities mask for logical channel number assignment
#define ISO7816_CARD_CAPS_CHAN_NUM_ASSIGN_CARD  (0x10) ///< Card capabilities: logical channel number assignment by the card
#define ISO7816_CARD_CAPS_CHAN_NUM_ASSIGN_IFD   (0x08) ///< Card capabilities: logical channel number assignment by the interface device
#define ISO7816_CARD_CAPS_CHAN_NUM_ASSIGN_NONE  (0x00) ///< Card capabilities: no logical channel
#define ISO7816_CARD_CAPS_MAX_CHAN_MASK         (0x07) ///< Card capabilities mask for maximum number of logical channels

/**
 * Stringify ISO/IEC 7816 Command Application Protocol Data Unit (C-APDU)
 * @param c_apdu Command Application Protocol Data Unit (C-APDU). Must be at least 4 bytes.
 * @param c_apdu_len Length of Command Application Protocol Data Unit (C-APDU). Must be at least 4 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return String. NULL for error.
 */
const char* iso7816_capdu_get_string(
	const void* c_apdu,
	size_t c_apdu_len,
	char* str,
	size_t str_len
);

/**
 * Stringify ISO/IEC 7816 status bytes (SW1-SW2)
 * @param SW1 Status byte 1
 * @param SW2 Status byte 2
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return String. NULL for error.
 */
const char* iso7816_sw1sw2_get_string(uint8_t SW1, uint8_t SW2, char* str, size_t str_len);

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

/**
 * Stringify ISO/IEC 7816 card capabilities
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param card_capabilities Card capabilities bytes
 * @param card_capabilities_len Length of card capabilities bytes
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int iso7816_card_capabilities_get_string_list(
	const uint8_t* card_capabilities,
	size_t card_capabilities_len,
	char* str,
	size_t str_len
);

__END_DECLS

#endif
