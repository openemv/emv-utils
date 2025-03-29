/**
 * @file emv_oda.c
 * @brief EMV Offline Data Authentication (ODA) helper functions
 *
 * Copyright 2025 Leon Lynch
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

#include "emv_oda.h"
#include "emv_tlv.h"
#include "emv_tags.h"
#include "emv_fields.h"
#include "emv_ttl.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_ODA
#include "emv_debug.h"

#include "crypto_mem.h"

#include <stdlib.h>
#include <string.h>

int emv_oda_init(
	struct emv_oda_ctx_t* ctx,
	const uint8_t* afl,
	size_t afl_len
)
{
	int r;
	struct emv_afl_itr_t afl_itr;
	struct emv_afl_entry_t afl_entry;
	unsigned int oda_record_count = 0;

	if (!ctx || !afl || !afl_len) {
		emv_debug_trace_msg("ctx=%p, afl=%p, afl_len=%zu", ctx, afl, afl_len);
		emv_debug_error("Invalid parameter");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}
	memset(ctx, 0, sizeof(*ctx));

	r = emv_afl_itr_init(afl, afl_len, &afl_itr);
	if (r) {
		emv_debug_trace_msg("emv_afl_itr_init() failed; r=%d", r);
		if (r < 0) {
			emv_debug_error("Internal error");
			return EMV_ODA_ERROR_INTERNAL;
		}
		if (r > 0) {
			emv_debug_error("Invalid AFL");
			return EMV_ODA_ERROR_AFL_INVALID;
		}
	}

	while ((r = emv_afl_itr_next(&afl_itr, &afl_entry)) > 0) {
		oda_record_count += afl_entry.oda_record_count;
	}
	if (r < 0) {
		emv_debug_trace_msg("emv_afl_itr_next() failed; r=%d", r);

		emv_debug_error("AFL parse error");
		return EMV_ODA_ERROR_AFL_INVALID;
	}

	// Allocate enough space for the total number of full length records
	// that are intended for offline data authentication
	ctx->buf = malloc(EMV_RAPDU_DATA_MAX * oda_record_count);
	if (!ctx->buf) {
		emv_debug_error("Failed to allocate ODA buffer");
		return EMV_ODA_ERROR_INTERNAL;
	}

	return 0;
}

int emv_oda_clear(struct emv_oda_ctx_t* ctx)
{
	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}

	if (ctx->buf) {
		crypto_cleanse(ctx->buf, ctx->buf_len);
		free(ctx->buf);
	}
	memset(ctx, 0, sizeof(*ctx));

	return 0;
}

int emv_oda_append_record(
	struct emv_oda_ctx_t* ctx,
	const void* record,
	unsigned int record_len
)
{
	if (!ctx || !record || !record_len) {
		emv_debug_trace_msg("ctx=%p, record=%p, record_len=%u",
			ctx, record, record_len
		);
		emv_debug_error("Invalid parameter");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}
	if (!ctx->buf) {
		emv_debug_trace_msg("ctx->buf=%p", ctx->buf);
		emv_debug_error("Invalid ODA buffer");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}
	if (record_len > EMV_RAPDU_DATA_MAX) {
		emv_debug_trace_msg("record_len=%u", record_len);
		emv_debug_error("Invalid ODA record length");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}

	memcpy(ctx->buf + ctx->buf_len, record, record_len);
	ctx->buf_len += record_len;

	return 0;
}

int emv_oda_apply(
	struct emv_oda_ctx_t* ctx,
	const struct emv_tlv_list_t* config,
	struct emv_tlv_list_t* icc,
	struct emv_tlv_list_t* terminal
)
{
	const struct emv_tlv_t* term_caps;
	const struct emv_tlv_t* aip;

	if (!ctx || !config || !icc || !terminal) {
		emv_debug_trace_msg("ctx=%p, config=%p, icc=%p, terminal=%p",
			ctx, config, icc, terminal
		);
		emv_debug_error("Invalid parameter");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}

	// Lookup fields that are required regardless of selected ODA method
	term_caps = emv_tlv_list_find_const(config, EMV_TAG_9F33_TERMINAL_CAPABILITIES);
	if (!term_caps) {
		emv_debug_error("Terminal Capabilities not found");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}
	aip = emv_tlv_list_find_const(icc, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
	if (!aip) {
		emv_debug_error("AIP not found");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}
	ctx->aid = emv_tlv_list_find_const(terminal, EMV_TAG_9F06_AID);
	if (!ctx->aid) {
		emv_debug_error("AID not found");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}
	ctx->tvr = emv_tlv_list_find_const(terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS);
	if (!ctx->tvr) {
		emv_debug_error("TVR not found");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}
	ctx->tsi = emv_tlv_list_find_const(terminal, EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION);
	if (!ctx->tsi) {
		emv_debug_error("TSI not found");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}

	// Determine whether Extended Data Authentication (XDA) is supported by
	// both the terminal and the card. If so, apply it.
	// See EMV 4.4 Book 3, 10.3 (page 96)
	if (term_caps->value[2] & EMV_TERM_CAPS_SECURITY_XDA &&
		aip->value[0] & EMV_AIP_XDA_SUPPORTED
	) {
		emv_debug_error("XDA selected but not implemented");
		ctx->tvr->value[3] |= EMV_TVR_XDA_FAILED;
		return EMV_ODA_ERROR_INTERNAL;
	}

	// Determine whether Combined DDA/Application Cryptogram Generation (CDA)
	// is supported by both the terminal and the card. If so, apply it.
	// See EMV 4.4 Book 3, 10.3 (page 96)
	if (term_caps->value[2] & EMV_TERM_CAPS_SECURITY_CDA &&
		aip->value[0] & EMV_AIP_CDA_SUPPORTED
	) {
		emv_debug_error("CDA selected but not implemented");
		ctx->tvr->value[0] |= EMV_TVR_CDA_FAILED;
		return EMV_ODA_ERROR_INTERNAL;
	}

	// Determine whether Dynamic Data Authentication (DDA) is supported by both
	// the terminal and the card. If so, apply it.
	// See EMV 4.4 Book 3, 10.3 (page 96)
	if (term_caps->value[2] & EMV_TERM_CAPS_SECURITY_DDA &&
		aip->value[0] & EMV_AIP_DDA_SUPPORTED
	) {
		emv_debug_error("DDA selected but not implemented");
		ctx->tvr->value[0] |= EMV_TVR_DDA_FAILED;
		return EMV_ODA_ERROR_INTERNAL;
	}

	// Determine whether Static Data Authentication (SDA) is supported by both
	// the terminal and the card. If so, apply it.
	// See EMV 4.4 Book 3, 10.3 (page 96)
	if (term_caps->value[2] & EMV_TERM_CAPS_SECURITY_SDA &&
		aip->value[0] & EMV_AIP_SDA_SUPPORTED
	) {
		emv_debug_error("SDA selected but not implemented");
		ctx->tvr->value[0] |= EMV_TVR_SDA_FAILED;
		return EMV_ODA_ERROR_INTERNAL;
	}

	// No supported ODA method
	// See EMV 4.4 Book 3, 10.3 (page 96)
	emv_debug_trace_msg("term_caps=%02X%02X%02X, aip=%02X%02x",
		term_caps->value[0], term_caps->value[1], term_caps->value[2],
		aip->value[0], aip->value[1]
	);
	emv_debug_info("No supported offline data authentication method");
	ctx->tvr->value[0] |= EMV_TVR_OFFLINE_DATA_AUTH_NOT_PERFORMED;
	return EMV_ODA_NO_SUPPORTED_METHOD;
}
