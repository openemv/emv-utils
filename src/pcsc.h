/**
 * @file pcsc.h
 * @brief PC/SC abstraction
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

#ifndef PCSC_H
#define PCSC_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

// Forward declarations
typedef void* pcsc_ctx_t; ///< PC/SC context pointer type
typedef void* pcsc_reader_ctx_t; ///< PC/SC reader context pointer type

// NOTE: these are derived from PCSCLite's SCARD_STATE_* defines
#define PCSC_STATE_CHANGED      (0x0002) ///< State has changed
#define PCSC_STATE_UNAVAILABLE  (0x0008) ///< Status unavailable
#define PCSC_STATE_EMPTY        (0x0010) ///< Card removed
#define PCSC_STATE_PRESENT      (0x0020) ///< Card inserted
#define PCSC_STATE_ATRMATCH     (0x0040) ///< ATR matches card
#define PCSC_STATE_EXCLUSIVE    (0x0080) ///< Exclusive Mode
#define PCSC_STATE_INUSE        (0x0100) ///< Shared Mode
#define PCSC_STATE_MUTE         (0x0200) ///< Unresponsive card
#define PCSC_STATE_UNPOWERED    (0x0400) ///< Unpowered card

#define PCSC_TIMEOUT_INFINITE   (0xFFFFFFFF) ///< Infinite timeout
#define PCSC_READER_ANY         (0xFFFFFFFF) ///< Use any reader

#define PCSC_MAX_ATR_SIZE       (33) ///< Maximum size of ATR buffer

/**
 * Initialize PC/SC context
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
 * Retrieve PC/SC reader state
 * @param reader_ctx PC/SC reader context
 * @param state PC/SC reader state output. See PCSC_STATE_* bits.
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
 * Connect to PC/SC reader.
 * This function will attempt to power up the card.
 * @param reader_ctx PC/SC reader context
 * @return Zero for success. Less than zero for error.
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
 * Retrieve ATR for current card in reader
 * @param reader_ctx PC/SC reader context
 * @param atr ATR output of at most @ref PCSC_MAX_ATR_SIZE bytes
 * @param atr_len Length of ATR output in bytes
 * @return Zero for success. Less than zero for error.
 */
int pcsc_reader_get_atr(pcsc_reader_ctx_t reader_ctx, uint8_t* atr, size_t* atr_len);

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
