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
#include "emv_capk.h"
#include "emv_rsa.h"

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
	// that are intended for offline data authentication, as well as the
	// encoded AIP, AID (terminal) and PDOL. Assume that the encoded fields
	// cannot exceed two R-APDU responses in total.
	// See EMV 4.4 Book 3, 10.3 (page 98)
	ctx->buf = malloc(EMV_RAPDU_DATA_MAX * (oda_record_count + 2));
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
		return emv_oda_apply_sda(ctx, icc, terminal);
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

int emv_oda_apply_sda(
	struct emv_oda_ctx_t* ctx,
	struct emv_tlv_list_t* icc,
	struct emv_tlv_list_t* terminal
)
{
	int r;
	const struct emv_tlv_t* capk_index;
	const struct emv_tlv_t* ipk_cert;
	const struct emv_tlv_t* ipk_exp;
	const struct emv_tlv_t* enc_ssad;
	const struct emv_tlv_t* sdatl;
	const struct emv_capk_t* capk;
	struct emv_rsa_issuer_pkey_t ipk;
	struct emv_rsa_ssad_t ssad;

	if (!ctx || !icc || !terminal) {
		emv_debug_trace_msg("ctx=%p, icc=%p, terminal=%p",
			ctx, icc, terminal
		);
		emv_debug_error("Invalid parameter");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}
	if (!ctx->aid || !ctx->tvr || !ctx->tsi) {
		emv_debug_trace_msg("aid=%p, tvr=%p, tsi=%p",
			ctx->aid, ctx->tvr, ctx->tsi
		);
		emv_debug_error("Invalid context variable");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}

	// Indicate that SDA is selected and performed, but assume that it has
	// failed until the related steps have succeeded
	emv_debug_info("Select Static Data Authentication (SDA)");
	ctx->tvr->value[0] |=
		EMV_TVR_SDA_SELECTED |
		EMV_TVR_SDA_FAILED |
		EMV_TVR_ICC_DATA_MISSING;
	ctx->tsi->value[0] |= EMV_TSI_OFFLINE_DATA_AUTH_PERFORMED;

	// Mandatory data objects for SDA
	// See EMV 4.4 Book 2, 5.1.1, table 4
	// See EMV 4.4 Book 3, 7.2, table 29
	// Although Issuer Public Key Remainder (field 92) is mandatory, it
	// will not always be used. As such, this implementation does not
	// enforce its presence.
	capk_index = emv_tlv_list_find_const(icc, EMV_TAG_8F_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX);
	ipk_cert = emv_tlv_list_find_const(icc, EMV_TAG_90_ISSUER_PUBLIC_KEY_CERTIFICATE);
	ipk_exp = emv_tlv_list_find_const(icc, EMV_TAG_9F32_ISSUER_PUBLIC_KEY_EXPONENT);
	enc_ssad = emv_tlv_list_find_const(icc, EMV_TAG_93_SIGNED_STATIC_APPLICATION_DATA);
	if (!capk_index || capk_index->length != 1 ||
		!ipk_cert ||
		!ipk_exp ||
		!enc_ssad
	) {
		emv_debug_trace_msg("capk_index=%p, ipk_cert=%p, ipk_exp=%p, enc_ssad=%p",
			capk_index, ipk_cert, ipk_exp, enc_ssad
		);
		emv_debug_error("Mandatory data object missing for SDA");
		// EMV_TVR_SDA_FAILED and EMV_TVR_ICC_DATA_MISSING already set in TVR
		return EMV_ODA_ICC_DATA_MISSING;
	}
	ctx->tvr->value[0] &= ~EMV_TVR_ICC_DATA_MISSING;

	// Validate Static Data Authentication Tag List (field 9F4A), if present
	// See EMV 4.4 Book 2, 5.1.1
	// See EMV 4.4 Book 3, 10.3 (page 98)
	sdatl = emv_tlv_list_find_const(icc, EMV_TAG_9F4A_SDA_TAG_LIST);
	if (sdatl) {
		const struct emv_tlv_t* aip;

		if (sdatl->length != 1 || sdatl->value[0] != EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE) {
			emv_debug_trace_data("9F4A", sdatl->value, sdatl->length);
			emv_debug_error("Invalid SDA tag list");
			// EMV_TVR_SDA_FAILED already set in TVR
			return EMV_ODA_SDA_FAILED;
		}

		aip = emv_tlv_list_find_const(icc, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);
		if (!aip) {
			emv_debug_error("AIP not found");
			// EMV_TVR_SDA_FAILED already set in TVR
			return EMV_ODA_SDA_FAILED;
		}

		// Append AIP to ODA record data
		// See EMV 4.4 Book 2, 5.4, step 5
		// See EMV 4.4 Book 3, 10.3 (page 98)
		r = emv_oda_append_record(ctx, aip->value, aip->length);
		if (r) {
			emv_debug_trace_msg("emv_oda_append_record() failed; r=%d", r);
			emv_debug_error("Internal error");
			// EMV_TVR_SDA_FAILED already set in TVR
			return EMV_ODA_ERROR_INTERNAL;
		}
	}

	// Retrieve Certificate Authority Public Key (CAPK)
	// See EMV 4.4 Book 2, 5.2
	capk = emv_capk_lookup(ctx->aid->value, capk_index->value[0]);
	if (!capk) {
		emv_debug_error(
			"CAPK %02X%02X%02X%02X%02X #%02X not found",
			ctx->aid->value[0],
			ctx->aid->value[1],
			ctx->aid->value[2],
			ctx->aid->value[3],
			ctx->aid->value[4],
			capk_index->value[0]
		);
		// EMV_TVR_SDA_FAILED already set in TVR
		return EMV_ODA_SDA_FAILED;
	}

	// Retrieve issuer public key
	// See EMV 4.4 Book 2, 5.3
	r = emv_rsa_retrieve_issuer_pkey(
		ipk_cert->value,
		ipk_cert->length,
		capk,
		icc,
		&ipk
	);
	if (r) {
		emv_debug_trace_msg("emv_rsa_retrieve_issuer_pkey() failed; r=%d", r);
		emv_debug_error("Failed to retrieve issuer public key");
		// EMV_TVR_SDA_FAILED already set in TVR
		r = EMV_ODA_SDA_FAILED;
		goto exit;
	}

	// Retrieve Signed Static Application Data (SSAD)
	// See EMV 4.4 Book 2, 5.4
	r = emv_rsa_retrieve_ssad(
		enc_ssad->value,
		enc_ssad->length,
		&ipk,
		ctx,
		&ssad
	);
	if (r) {
		emv_debug_trace_msg("emv_rsa_retrieve_ssad() failed; r=%d", r);
		if (r < 0) {
			emv_debug_error("Failed to retrieve Signed Static Application Data");
		} else {
			emv_debug_error("Failed to validate Signed Static Application Data hash");
		}
		// EMV_TVR_SDA_FAILED already set in TVR
		r = EMV_ODA_SDA_FAILED;
		goto exit;
	}
	emv_debug_info("Valid Signed Static Application Data hash");

	// Create Certification Authority Public Key (CAPK) Index - terminal (field 9F22)
	r = emv_tlv_list_push(
		terminal,
		EMV_TAG_9F22_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX,
		1,
		capk_index->value,
		0
	);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);
		emv_debug_error("Internal error");
		// EMV_TVR_SDA_FAILED already set in TVR
		r = EMV_ODA_ERROR_INTERNAL;
		goto exit;
	}

	// Create Data Authentication Code (field 9F45)
	// See EMV 4.4 Book 2, 5.4 (page 48)
	r = emv_tlv_list_push(
		icc,
		EMV_TAG_9F45_DATA_AUTHENTICATION_CODE,
		sizeof(ssad.data_auth_code),
		ssad.data_auth_code,
		0
	);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);
		emv_debug_error("Internal error");
		// EMV_TVR_SDA_FAILED already set in TVR
		r = EMV_ODA_ERROR_INTERNAL;
		goto exit;
	}

	// Successful SDA processing
	emv_debug_info("Static Data Authentication (SDA) succeeded");
	ctx->tvr->value[0] &= ~EMV_TVR_SDA_FAILED;
	r = 0;
	goto exit;

exit:
	// Cleanse issuer public key because it contains up to 8 PAN digits
	crypto_cleanse(&ipk, sizeof(ipk));
	return r;
}
