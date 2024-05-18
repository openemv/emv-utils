/**
 * @file emv_cardreader_emul.h
 * @brief Basic card reader emulation for unit tests
 *
 * Copyright 2024 Leon Lynch
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

#ifndef EMV_CARDREADER_EMUL_H
#define EMV_CARDREADER_EMUL_H

#include <stddef.h>
#include <stdint.h>

/// Transport/Application Protocol Data Unit (xPDU)
struct xpdu_t {
	size_t c_xpdu_len;
	const uint8_t* c_xpdu;
	size_t r_xpdu_len;
	const uint8_t* r_xpdu;
};

/// Card reader emulator context
struct emv_cardreader_emul_ctx_t {
	const struct xpdu_t* xpdu_list;
	const struct xpdu_t* xpdu_current;
};

/**
 * Emulate card reader transceive
 *
 * @param ctx Card reader emulator context
 * @param tx_buf Transmit buffer
 * @param tx_buf_len Length of transmit buffer in bytes
 * @param rx_buf Receive buffer
 * @param rx_buf_len Length of receive buffer in bytes
 */
int emv_cardreader_emul(
	void* ctx,
	const void* tx_buf,
	size_t tx_buf_len,
	void* rx_buf,
	size_t* rx_buf_len
);

#endif
