/**
 * @file pcsc.h
 * @brief PC/SC abstraction
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

#ifndef PCSC_H
#define PCSC_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdbool.h>

__BEGIN_DECLS

// Forward declarations
typedef void* pcsc_ctx_t; ///< PC/SC context pointer type
typedef void* pcsc_reader_ctx_t; ///< PC/SC reader context pointer type

/**
 * @name PC/SC reader features
 * @remark See PC/SC Part 10 Rev 2.02.09, 2.3
 * @anchor pcsc-reader-features
 */
/// @{
#define PCSC_FEATURE_VERIFY_PIN_DIRECT          (0x06) ///< Direct PIN verification
#define PCSC_FEATURE_MODIFY_PIN_DIRECT          (0x07) ///< Direct PIN modification
#define PCSC_FEATURE_MCT_READER_DIRECT          (0x08) ///< Multifunctional Card Terminal (MCT) direct commands
#define PCSC_FEATURE_MCT_UNIVERSAL              (0x09) ///< Multifunctional Card Terminal (MCT) universal commands
/// @}

/**
 * @name PC/SC reader properties
 * @remark See PC/SC Part 10 Rev 2.02.09, 2.6.14
 * @anchor pcsc-reader-properties
 */
/// @{
#define PCSC_PROPERTY_wLcdLayout                (0x01) ///< LCD Layout (from USB CCID wLcdLayout field)
#define PCSC_PROPERTY_wLcdMaxCharacters         (0x04) ///< Maximum number of characters on a single line of LCD (from USB CCID wLcdLayout field)
#define PCSC_PROPERTY_wLcdMaxLines              (0x05) ///< Maximum number of lines of LCD (from USB CCID wLcdLayout field)
#define PCSC_PROPERTY_bMinPINSize               (0x06) ///< Minimum PIN size accepted by the reader
#define PCSC_PROPERTY_bMaxPINSize               (0x07) ///< Maximum PIN size accepted by the reader
#define PCSC_PROPERTY_wIdVendor                 (0x0B) ///< USB Vendor ID (from USB idVendor field)
#define PCSC_PROPERTY_wIdProduct                (0x0C) ///< USB Product ID (from USB idProduct field)
/// @}

/**
 * @name PC/SC reader states
 * @remark These are derived from PCSCLite's SCARD_STATE_* defines
 * @anchor pcsc-reader-states
 */
/// @{
#define PCSC_STATE_CHANGED      (0x0002) ///< State has changed
#define PCSC_STATE_UNAVAILABLE  (0x0008) ///< Status unavailable
#define PCSC_STATE_EMPTY        (0x0010) ///< Card removed
#define PCSC_STATE_PRESENT      (0x0020) ///< Card inserted
#define PCSC_STATE_ATRMATCH     (0x0040) ///< ATR matches card
#define PCSC_STATE_EXCLUSIVE    (0x0080) ///< Exclusive Mode
#define PCSC_STATE_INUSE        (0x0100) ///< Shared Mode
#define PCSC_STATE_MUTE         (0x0200) ///< Unresponsive card
#define PCSC_STATE_UNPOWERED    (0x0400) ///< Unpowered card
/// @}

#define PCSC_TIMEOUT_INFINITE   (0xFFFFFFFF) ///< Infinite timeout
#define PCSC_READER_ANY         (0xFFFFFFFF) ///< Use any reader

#define PCSC_MAX_ATR_SIZE       (33) ///< Maximum size of ATR buffer

/// Type of card presented
enum pcsc_card_type_t {
	PCSC_CARD_TYPE_UNKNOWN = 0, ///< Unknown card type
	PCSC_CARD_TYPE_CONTACT, ///< ISO 7816 contact card
	PCSC_CARD_TYPE_CONTACTLESS, ///< ISO 14443 contactless card
};

/**
 * Initialise PC/SC context
 * @param ctx PC/SC context pointer
 * @return Zero for success. Less than zero for error.
 */
int pcsc_init(pcsc_ctx_t* ctx);

/**
 * Release PC/SC context
 * @param ctx PC/SC context pointer
 */
void pcsc_release(pcsc_ctx_t* ctx);

/**
 * Retrieve number of available PC/SC readers
 * @param ctx PC/SC context
 * @return Number of available readers
 */
size_t pcsc_get_reader_count(pcsc_ctx_t ctx);

/**
 * Retrieve PC/SC reader context
 * @param ctx PC/SC context
 * @param idx PC/SC reader index
 * @return PC/SC reader context. NULL for error. Do not @ref free()
 */
pcsc_reader_ctx_t pcsc_get_reader(pcsc_ctx_t ctx, size_t idx);

/**
 * Retrieve PC/SC reader name
 * @param reader_ctx PC/SC reader context
 * @return PC/SC reader name. NULL for error. Do not @ref free()
 */
const char* pcsc_reader_get_name(pcsc_reader_ctx_t reader_ctx);

/**
 * Indicate whether PC/SC reader feature is supported
 * @param reader_ctx PC/SC reader context
 * @param feature PC/SC reader feature. See @ref pcsc-reader-features "PC/SC reader features"
 * @return Boolean indicating whether specified PC/SC reader feature is supported.
 */
bool pcsc_reader_has_feature(pcsc_reader_ctx_t reader_ctx, unsigned int feature);

/**
 * Retrieve PC/SC reader property value
 * @param reader_ctx PC/SC reader context
 * @param property PC/SC property to retrieve. See @ref pcsc-reader-properties "PC/SC reader properties"
 * @param value Value buffer output
 * @param value_len Length of value buffer in bytes
 * @return Zero for success. Less than zero for error. Greater than zero if not found.
 */
int pcsc_reader_get_property(
	pcsc_reader_ctx_t reader_ctx,
	unsigned int property,
	void* value,
	size_t* value_len
);

/**
 * Retrieve PC/SC reader state
 * @param reader_ctx PC/SC reader context
 * @param state PC/SC reader state output. See @ref pcsc-reader-states "PC/SC reader states"
 * @return Zero for success. Less than zero for error.
 */
int pcsc_reader_get_state(pcsc_reader_ctx_t reader_ctx, unsigned int* state);

/**
 * Wait for card from specific reader or any reader
 * @param ctx PC/SC context
 * @param timeout_ms Timeout in milliseconds
 * @param[in,out] idx PC/SC reader index input indicates which card reader to
 *                    use. Use @ref PCSC_READER_ANY for any reader. Output
 *                    indicates reader when card detected.
 * @return Zero when card detected. Less than zero for error. Greater than zero for timeout.
 */
int pcsc_wait_for_card(pcsc_ctx_t ctx, unsigned long timeout_ms, size_t* idx);

/**
 * Connect to PC/SC reader, attempt to power up the card, and attempt to
 * identify the type of card.
 * @param reader_ctx PC/SC reader context
 * @return Less than zero for error. Otherwise @ref pcsc_card_type_t
 */
int pcsc_reader_connect(pcsc_reader_ctx_t reader_ctx);

/**
 * Disconnect from PC/SC reader.
 * This function will attempt to unpower the card.
 * @param reader_ctx PC/SC reader context
 * @return Zero for success. Less than zero for error.
 */
int pcsc_reader_disconnect(pcsc_reader_ctx_t reader_ctx);

/**
 * Retrieve ISO 7816 Answer-To-Reset (ATR) for current card in reader
 * @note Although PC/SC provides an artificial ATR for contactless cards, this
 *       function will only retrieve the ATR for contact cards.
 * @param reader_ctx PC/SC reader context
 * @param atr ATR output of at most @ref PCSC_MAX_ATR_SIZE bytes
 * @param atr_len Length of ATR output in bytes
 * @return Zero for success. Less than zero for error. Greater than zero if not available.
 */
int pcsc_reader_get_atr(pcsc_reader_ctx_t reader_ctx, void* atr, size_t* atr_len);

/**
 * Transmit and receive data for current card in reader
 * @param reader_ctx PC/SC reader context
 * @param tx_buf Transmit buffer
 * @param tx_buf_len Length of transmit buffer in bytes
 * @param rx_buf Receive buffer
 * @param rx_buf_len Length of receive buffer in bytes
 * @return Zero for success. Less than zero for error.
 */
int pcsc_reader_trx(
	pcsc_reader_ctx_t reader_ctx,
	const void* tx_buf,
	size_t tx_buf_len,
	void* rx_buf,
	size_t* rx_buf_len
);

__END_DECLS

#endif
