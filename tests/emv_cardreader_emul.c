/**
 * @file emv_cardreader_emul.c
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

#include "emv_cardreader_emul.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int emv_cardreader_emul(
	void* ctx,
	const void* tx_buf,
	size_t tx_buf_len,
	void* rx_buf,
	size_t* rx_buf_len
) {
	struct emv_cardreader_emul_ctx_t* emul_ctx = ctx;
	const struct xpdu_t* xpdu;

	// NOTE: This function calls exit() on error to ensure that they are
	// detected because the TTL interprets the failure of this function as a
	// hardware or card error.

	if (!emul_ctx->xpdu_current) {
		emul_ctx->xpdu_current = emul_ctx->xpdu_list;
	}
	if (!emul_ctx->xpdu_current->c_xpdu_len) {
		fprintf(stderr, "Invalid transmission\n");
		exit(1);
		return -101;
	}
	xpdu = emul_ctx->xpdu_current;

	if (tx_buf_len != xpdu->c_xpdu_len) {
		fprintf(stderr, "Incorrect transmit length\n");
		exit(1);
		return -102;
	}
	if (memcmp(tx_buf, xpdu->c_xpdu, xpdu->c_xpdu_len) != 0) {
		fprintf(stderr, "Incorrect transmit data\n");
		exit(1);
		return -103;
	}

	memcpy(rx_buf, xpdu->r_xpdu, xpdu->r_xpdu_len);
	*rx_buf_len = xpdu->r_xpdu_len;

	emul_ctx->xpdu_current++;

	return 0;
}
