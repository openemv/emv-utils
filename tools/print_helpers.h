/**
 * @file print_helpers.h
 * @brief Helper functions for command line output
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

#ifndef PRINT_HELPERS_H
#define PRINT_HELPERS_H

#include <stddef.h>
#include <stdint.h>

#include "emv_debug.h"

// Forward declarations
struct iso7816_atr_info_t;
struct emv_tlv_t;
struct emv_tlv_list_t;
struct emv_app_t;

/**
 * Print buffer as hex digits
 * @param buf_name Name of buffer in ASCII
 * @param buf Buffer
 * @param length Length of buffer in bytes
 */
void print_buf(const char* buf_name, const void* buf, size_t length);

/**
 * Print string list
 * @param str_list String list
 * @param delim Delimiter
 * @param prefix Recursion prefix to print before every string
 * @param depth Depth of current recursion
 * @param bullet Bullet point to print before every string
 * @param suffix Suffix to print after every string
 */
void print_str_list(
	const char* str_list,
	const char* delim,
	const char* prefix,
	unsigned int depth,
	const char* bullet,
	const char* suffix
);

/**
 * Print ATR details, including historical bytes
 * @param atr_info Parsed ATR info
 */
void print_atr(const struct iso7816_atr_info_t* atr_info);

/**
 * Print ATR historical bytes
 * @param atr_info Parsed ATR info
 */
void print_atr_historical_bytes(const struct iso7816_atr_info_t* atr_info);

/**
 * Print C-APDU
 * @param c_apdu Command Application Protocol Data Unit (C-APDU)
 * @param c_apdu_len Length of Command Application Protocol Data Unit (C-APDU). Must be at least 4 bytes.
 */
void print_capdu(const void* c_apdu, size_t c_apdu_len);

/**
 * Print R-APDU
 * @param r_apdu Response Application Protocol Data Unit (C-APDU)
 * @param r_apdu_len Length of Response Application Protocol Data Unit (C-APDU). Must be at least 4 bytes.
 */
void print_rapdu(const void* r_apdu, size_t r_apdu_len);

/**
 * Print status bytes SW1-SW2
 * @param SW1 Status byte 1
 * @param SW2 Status byte 2
 */
void print_sw1sw2(uint8_t SW1, uint8_t SW2);

/**
 * Print BER data
 * @param ptr BER encoded data
 * @param len Length of BER encoded data in bytes
 * @param prefix Recursion prefix to print before every string
 * @param depth Depth of current recursion
 */
void print_ber_buf(const void* ptr, size_t len, const char* prefix, unsigned int depth);

/**
 * Print EMV TLV field
 * @param tlv EMV TLV field
 * @param prefix Recursion prefix to print before every string
 * @param depth Depth of current recursion
 */
void print_emv_tlv(const struct emv_tlv_t* tlv, const char* prefix, unsigned int depth);

/**
 * Print EMV TLV data
 * @param ptr EMV TLV data
 * @param len Length of EMV TLV data in bytes
 * @param prefix Recursion prefix to print before every string
 * @param depth Depth of current recursion
 */
void print_emv_buf(const void* ptr, size_t len, const char* prefix, unsigned int depth);

/**
 * Print EMV TLV list
 * @param list EMV TLV list object
 */
void print_emv_tlv_list(const struct emv_tlv_list_t* list);

/**
 * Print EMV Data Object List (DOL)
 * @param ptr EMV Data Object List (DOL) data
 * @param len Length of EMV Data Object List (DOL) data in bytes
 * @param prefix Recursion prefix to print before every string
 * @param depth Depth of current recursion
 */
void print_emv_dol(const void* ptr, size_t len, const char* prefix, unsigned int depth);

/**
 * Print EMV Tag List
 * @param ptr EMV Tag List data
 * @param len Length of EMV Tag List data in bytes
 * @param prefix Recursion prefix to print before every string
 * @param depth Depth of current recursion
 */
void print_emv_tag_list(const void* ptr, size_t len, const char* prefix, unsigned int depth);

/**
 * Print EMV application description
 * @param app EMV application object
 */
void print_emv_app(const struct emv_app_t* app);

/**
 * Print simple EMV debug event including source, message and data only.
 * @see emv_debug_func_t
 * @param timestamp 32-bit microsecond timestamp value
 * @param source Debug event source
 * @param level Debug event level
 * @param debug_type Debug event type
 * @param str Debug event string
 * @param buf Debug event data
 * @param buf_len Length of debug event data in bytes
 */
void print_emv_debug(
	unsigned int timestamp,
	enum emv_debug_source_t source,
	enum emv_debug_level_t level,
	enum emv_debug_type_t debug_type,
	const char* str,
	const void* buf,
	size_t buf_len
);

/**
 * Print verbose EMV debug event including timestamp, source, level, message,
 * and data
 * @see emv_debug_func_t
 * @param timestamp 32-bit microsecond timestamp value
 * @param source Debug event source
 * @param level Debug event level
 * @param debug_type Debug event type
 * @param str Debug event string
 * @param buf Debug event data
 * @param buf_len Length of debug event data in bytes
 */
void print_emv_debug_verbose(
	unsigned int timestamp,
	enum emv_debug_source_t source,
	enum emv_debug_level_t level,
	enum emv_debug_type_t debug_type,
	const char* str,
	const void* buf,
	size_t buf_len
);

#endif
