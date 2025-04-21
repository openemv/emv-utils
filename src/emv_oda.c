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
#include "emv.h"
#include "emv_tlv.h"
#include "emv_tags.h"
#include "emv_fields.h"
#include "emv_ttl.h"
#include "emv_capk.h"
#include "emv_rsa.h"
#include "emv_dol.h"
#include "emv_tal.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_ODA
#include "emv_debug.h"

#include "crypto_mem.h"

#include <stdbool.h>
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
	struct emv_ctx_t* ctx,
	const uint8_t* term_caps
)
{
	if (!ctx || !term_caps) {
		emv_debug_trace_msg("ctx=%p, term_caps=%p", ctx, term_caps);
		emv_debug_error("Invalid parameter");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}
	if (!ctx->aid || !ctx->tvr || !ctx->tsi || !ctx->aip) {
		emv_debug_trace_msg("aid=%p, tvr=%p, tsi=%p, aip=%p",
			ctx->aid, ctx->tvr, ctx->tsi, ctx->aip
		);
		emv_debug_error("Invalid context variable");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}

	// Determine whether Extended Data Authentication (XDA) is supported by
	// both the terminal and the card. If so, apply it.
	// See EMV 4.4 Book 3, 10.3 (page 96)
	if (term_caps[2] & EMV_TERM_CAPS_SECURITY_XDA &&
		ctx->aip->value[0] & EMV_AIP_XDA_SUPPORTED
	) {
		emv_debug_error("XDA selected but not implemented");
		ctx->tvr->value[3] |= EMV_TVR_XDA_FAILED;
		return EMV_ODA_ERROR_INTERNAL;
	}

	// Determine whether Combined DDA/Application Cryptogram Generation (CDA)
	// is supported by both the terminal and the card. If so, apply it.
	// See EMV 4.4 Book 3, 10.3 (page 96)
	if (term_caps[2] & EMV_TERM_CAPS_SECURITY_CDA &&
		ctx->aip->value[0] & EMV_AIP_CDA_SUPPORTED
	) {
		return emv_oda_apply_cda(ctx);
	}

	// Determine whether Dynamic Data Authentication (DDA) is supported by both
	// the terminal and the card. If so, apply it.
	// See EMV 4.4 Book 3, 10.3 (page 96)
	if (term_caps[2] & EMV_TERM_CAPS_SECURITY_DDA &&
		ctx->aip->value[0] & EMV_AIP_DDA_SUPPORTED
	) {
		return emv_oda_apply_dda(ctx);
	}

	// Determine whether Static Data Authentication (SDA) is supported by both
	// the terminal and the card. If so, apply it.
	// See EMV 4.4 Book 3, 10.3 (page 96)
	if (term_caps[2] & EMV_TERM_CAPS_SECURITY_SDA &&
		ctx->aip->value[0] & EMV_AIP_SDA_SUPPORTED
	) {
		return emv_oda_apply_sda(ctx);
	}

	// No supported ODA method
	// See EMV 4.4 Book 3, 10.3 (page 96)
	emv_debug_trace_msg("term_caps=%02X%02X%02X, aip=%02X%02x",
		term_caps[0], term_caps[1], term_caps[2],
		ctx->aip->value[0], ctx->aip->value[1]
	);
	emv_debug_info("No supported offline data authentication method");
	ctx->tvr->value[0] |= EMV_TVR_OFFLINE_DATA_AUTH_NOT_PERFORMED;
	return EMV_ODA_NO_SUPPORTED_METHOD;
}

int emv_oda_apply_sda(struct emv_ctx_t* ctx)
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

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}
	if (!ctx->aid || !ctx->tvr || !ctx->tsi || !ctx->aip) {
		emv_debug_trace_msg("aid=%p, tvr=%p, tsi=%p, aip=%p",
			ctx->aid, ctx->tvr, ctx->tsi, ctx->aip
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
	capk_index = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_8F_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX);
	ipk_cert = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_90_ISSUER_PUBLIC_KEY_CERTIFICATE);
	ipk_exp = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_9F32_ISSUER_PUBLIC_KEY_EXPONENT);
	enc_ssad = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_93_SIGNED_STATIC_APPLICATION_DATA);
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
	sdatl = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_9F4A_SDA_TAG_LIST);
	if (sdatl) {
		if (sdatl->length != 1 || sdatl->value[0] != EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE) {
			emv_debug_trace_data("9F4A", sdatl->value, sdatl->length);
			emv_debug_error("Invalid SDA tag list");
			// EMV_TVR_SDA_FAILED already set in TVR
			return EMV_ODA_SDA_FAILED;
		}

		// Append AIP to ODA record data
		// See EMV 4.4 Book 2, 5.4, step 5
		// See EMV 4.4 Book 3, 10.3 (page 98)
		r = emv_oda_append_record(&ctx->oda, ctx->aip->value, ctx->aip->length);
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
		&ctx->icc,
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
		&ctx->oda,
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
		&ctx->terminal,
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
		&ctx->icc,
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

static int emv_oda_apply_sad_auth(struct emv_ctx_t* ctx, struct emv_rsa_icc_pkey_t* icc_pkey)
{
	int r;
	const struct emv_tlv_t* capk_index;
	const struct emv_tlv_t* ipk_cert;
	const struct emv_tlv_t* ipk_exp;
	const struct emv_tlv_t* icc_cert;
	const struct emv_tlv_t* icc_exp;
	const struct emv_tlv_t* sdatl;
	const struct emv_capk_t* capk;
	struct emv_rsa_issuer_pkey_t ipk;

	// Mandatory data objects for DDA/CDA
	// See EMV 4.4 Book 2, 6.1.1, table 12
	// See EMV 4.4 Book 3, 7.2, table 30
	// Although Issuer Public Key Remainder (field 92) and ICC Public Key
	// Remainder (field 9F48) are mandatory, they will not always be used. As
	// such, this implementation does not enforce its presence.
	capk_index = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_8F_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX);
	ipk_cert = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_90_ISSUER_PUBLIC_KEY_CERTIFICATE);
	ipk_exp = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_9F32_ISSUER_PUBLIC_KEY_EXPONENT);
	icc_cert = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_9F46_ICC_PUBLIC_KEY_CERTIFICATE);
	icc_exp = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_9F47_ICC_PUBLIC_KEY_EXPONENT);
	if (!capk_index || capk_index->length != 1 ||
		!ipk_cert ||
		!ipk_exp ||
		!icc_cert ||
		!icc_exp
	) {
		emv_debug_trace_msg(
			"capk_index=%p, ipk_cert=%p, ipk_exp=%p, icc_cert=%p, icc_exp=%p",
			capk_index, ipk_cert, ipk_exp, icc_cert, icc_exp
		);
		emv_debug_error("Mandatory data object missing for DDA/CDA");
		return EMV_ODA_ICC_DATA_MISSING;
	}

	// Validate Static Data Authentication Tag List (field 9F4A), if present
	// See EMV 4.4 Book 2, 6.1.1
	// See EMV 4.4 Book 3, 10.3 (page 98)
	sdatl = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_9F4A_SDA_TAG_LIST);
	if (sdatl) {
		if (sdatl->length != 1 || sdatl->value[0] != EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE) {
			emv_debug_trace_data("9F4A", sdatl->value, sdatl->length);
			emv_debug_error("Invalid SDA tag list");
			return EMV_ODA_SAD_AUTH_FAILED;
		}

		// Append AIP to ODA record data
		// See EMV 4.4 Book 2, 6.4, step 5
		// See EMV 4.4 Book 3, 10.3 (page 98)
		r = emv_oda_append_record(&ctx->oda, ctx->aip->value, ctx->aip->length);
		if (r) {
			emv_debug_trace_msg("emv_oda_append_record() failed; r=%d", r);
			emv_debug_error("Internal error");
			return EMV_ODA_ERROR_INTERNAL;
		}
	}

	// Retrieve Certificate Authority Public Key (CAPK)
	// See EMV 4.4 Book 2, 6.2
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
		return EMV_ODA_SAD_AUTH_FAILED;
	}

	// Retrieve issuer public key
	// See EMV 4.4 Book 2, 6.3
	r = emv_rsa_retrieve_issuer_pkey(
		ipk_cert->value,
		ipk_cert->length,
		capk,
		&ctx->icc,
		&ipk
	);
	if (r) {
		emv_debug_trace_msg("emv_rsa_retrieve_issuer_pkey() failed; r=%d", r);
		if (r < 0) {
			emv_debug_error("Failed to retrieve issuer public key");
		} else {
			emv_debug_error("Failed to validate issuer certificate hash");
		}
		r = EMV_ODA_SAD_AUTH_FAILED;
		goto exit;
	}

	// Retrieve ICC public key
	// See EMV 4.4 Book 2, 6.4
	r = emv_rsa_retrieve_icc_pkey(
		icc_cert->value,
		icc_cert->length,
		&ipk,
		&ctx->icc,
		&ctx->oda,
		icc_pkey
	);
	if (r) {
		emv_debug_trace_msg("emv_rsa_retrieve_icc_pkey() failed; r=%d", r);
		if (r < 0) {
			emv_debug_error("Failed to retrieve ICC public key");
		} else {
			emv_debug_error("Failed to validate ICC certificate hash");
		}
		r = EMV_ODA_SAD_AUTH_FAILED;
		goto exit;
	}
	emv_debug_info("Valid ICC certificate hash");

	// Create Certification Authority Public Key (CAPK) Index - terminal (field 9F22)
	r = emv_tlv_list_push(
		&ctx->terminal,
		EMV_TAG_9F22_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX,
		1,
		capk_index->value,
		0
	);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = EMV_ODA_ERROR_INTERNAL;
		goto exit;
	}

exit:
	// Cleanse issuer public key because it contains up to 8 PAN digits
	crypto_cleanse(&ipk, sizeof(ipk));
	return r;
}

int emv_oda_apply_dda(struct emv_ctx_t* ctx)
{
	int r;
	const struct emv_tlv_t* un;
	struct emv_rsa_icc_pkey_t icc_pkey;
	const struct emv_tlv_t* ddol;
	const struct emv_tlv_list_t* sources[3];
	size_t sources_count = sizeof(sources) / sizeof(sources[0]);
	uint8_t ddol_data_buf[EMV_CAPDU_DATA_MAX];
	size_t ddol_data_len = sizeof(ddol_data_buf);
	struct emv_tlv_list_t list = EMV_TLV_LIST_INIT;
	const struct emv_tlv_t* enc_sdad;
	struct emv_rsa_sdad_t sdad;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}
	if (!ctx->aid || !ctx->tvr || !ctx->tsi || !ctx->aip) {
		emv_debug_trace_msg("aid=%p, tvr=%p, tsi=%p, aip=%p",
			ctx->aid, ctx->tvr, ctx->tsi, ctx->aip
		);
		emv_debug_error("Invalid context variable");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}

	un = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9F37_UNPREDICTABLE_NUMBER);
	if (!un) {
		// Unpredictable Number should have been created by
		// emv_initiate_application_processing()
		emv_debug_error("Unpredictable Number not found");
		return EMV_ODA_ERROR_INTERNAL;
	}

	// Assume that DDA has failed until the related steps have succeeded
	emv_debug_info("Select Dynamic Data Authentication (DDA)");
	ctx->tvr->value[0] |=
		EMV_TVR_DDA_FAILED |
		EMV_TVR_ICC_DATA_MISSING;
	ctx->tsi->value[0] |= EMV_TSI_OFFLINE_DATA_AUTH_PERFORMED;

	// Authenticate static application data and retrieve ICC public key
	// See EMV 4.4 Book 2, 6.1 - 6.4
	r = emv_oda_apply_sad_auth(ctx, &icc_pkey);
	if (r) {
		emv_debug_trace_msg("emv_oda_apply_sad_auth() failed; r=%d", r);

		if (r < 0 || r == EMV_ODA_ICC_DATA_MISSING) {
			// EMV_TVR_DDA_FAILED and EMV_TVR_ICC_DATA_MISSING already set in TVR
			// Return error as-is
			goto exit;
		}

		ctx->tvr->value[0] &= ~EMV_TVR_ICC_DATA_MISSING;
		if (r == EMV_ODA_SAD_AUTH_FAILED) {
			// EMV_TVR_DDA_FAILED already set in TVR
			r = EMV_ODA_DDA_FAILED;
			goto exit;
		}

		emv_debug_error("Internal error");
		// EMV_TVR_DDA_FAILED already set in TVR
		r = EMV_ODA_ERROR_INTERNAL;
		goto exit;
	}
	ctx->tvr->value[0] &= ~EMV_TVR_ICC_DATA_MISSING;

	// Prepare DDOL
	// See EMV 4.4 Book 2, 6.5.1
	ddol = emv_tlv_list_find_const(&ctx->icc, EMV_TAG_9F49_DDOL);
	if (!ddol) {
		emv_debug_info("Use default Dynamic Data Authentication Data Object List (DDOL)");
		ddol = emv_tlv_list_find_const(&ctx->config, EMV_TAG_9F49_DDOL);
		if (!ddol) {
			// Presence of Default DDOL should have been confirmed by
			// emv_offline_data_authentication(), but if it is missing then
			// EMV 4.4 Book 2, 6.5.1 consider DDA to have failed and it is not
			// an internal error like other missing fields.
			emv_debug_error("Default Dynamic Data Authentication Data Object List (DDOL) not found");
			// EMV_TVR_DDA_FAILED already set in TVR
			r = EMV_ODA_DDA_FAILED;
			goto exit;
		}
	}

	// Validate DDOL
	// See EMV 4.4 Book 2, 6.5.1
	struct emv_dol_itr_t itr;
	struct emv_dol_entry_t entry;
	bool found_9F37 = false;
	r = emv_dol_itr_init(ddol->value, ddol->length, &itr);
	if (r) {
		emv_debug_trace_msg("emv_dol_itr_init() failed; r=%d", r);
		// EMV_TVR_DDA_FAILED already set in TVR
		r = EMV_ODA_DDA_FAILED;
		goto exit;
	}
	while ((r = emv_dol_itr_next(&itr, &entry)) > 0) {
		if (entry.tag == EMV_TAG_9F37_UNPREDICTABLE_NUMBER) {
			found_9F37 = true;
		}
	}
	if (r != 0) {
		emv_debug_trace_msg("emv_dol_itr_next() failed; r=%d", r);
		emv_debug_error("Invalid Dynamic Data Authentication Data Object List (DDOL)");
		// EMV_TVR_DDA_FAILED already set in TVR
		r = EMV_ODA_DDA_FAILED;
		goto exit;
	}
	if (!found_9F37) {
		emv_debug_error("Dynamic Data Authentication Data Object List (DDOL) does not contain Unpredictable Number (9F37)");
		// EMV_TVR_DDA_FAILED already set in TVR
		r = EMV_ODA_DDA_FAILED;
		goto exit;
	}

	// Build DDOL data
	// Favour terminal data for current transaction and do not allow config
	// to override terminal data nor ICC data
	// See EMV 4.4 Book 3, 5.4
	sources[0] = &ctx->terminal;
	sources[1] = &ctx->icc;
	sources[2] = &ctx->config;
	r = emv_dol_build_data(
		ddol->value,
		ddol->length,
		sources,
		sources_count,
		ddol_data_buf,
		&ddol_data_len
	);
	if (r) {
		emv_debug_trace_msg("emv_dol_build_data() failed; r=%d", r);
		emv_debug_error("Failed to build DDOL data");
		// EMV_TVR_DDA_FAILED already set in TVR
		r = EMV_ODA_DDA_FAILED;
		goto exit;
	}

	// Authenticate ICC
	// See EMV 4.4 Book 2, 6.5.1
	r = emv_tal_internal_authenticate(
		ctx->ttl,
		ddol_data_buf,
		ddol_data_len,
		&list
	);
	if (r) {
		emv_debug_trace_msg("emv_tal_internal_authenticate() failed; r=%d", r);
		emv_debug_error("Error during dynamic data authentication");
		if (r == EMV_TAL_ERROR_INTERNAL || r == EMV_TAL_ERROR_INVALID_PARAMETER) {
			r = EMV_ODA_ERROR_INTERNAL;
		} else {
			r = EMV_ODA_ERROR_INT_AUTH_FAILED;
		}
		// Internal errors, parse errors or missing mandatory fields in
		// INTERNAL AUTHENTICATE response all require the terminal
		// to terminate the session
		// See EMV 4.4 Book 3, 6.5.9.4
		goto exit;
	}
	enc_sdad = emv_tlv_list_find_const(&list, EMV_TAG_9F4B_SIGNED_DYNAMIC_APPLICATION_DATA);
	if (!enc_sdad) {
		// Presence of SDAD should have been confirmed by
		// emv_tal_internal_authenticate()
		emv_debug_error("SDAD not found in INTERNAL AUTHENTICATE response");
		r = EMV_ODA_ERROR_INTERNAL;
		goto exit;
	}

	// Retrieve Signed Dynamic Application Data (SDAD)
	// See EMV 4.4 Book 2, 6.5.2
	r = emv_rsa_retrieve_sdad(
		enc_sdad->value,
		enc_sdad->length,
		&icc_pkey,
		ddol_data_buf,
		ddol_data_len,
		&sdad
	);
	if (r) {
		emv_debug_trace_msg("emv_rsa_retrieve_sdad() failed; r=%d", r);
		if (r < 0) {
			emv_debug_error("Failed to retrieve Signed Dynamic Application Data");
		} else {
			emv_debug_error("Failed to validate Signed Dynamic Application Data hash");
		}
		// EMV_TVR_DDA_FAILED already set in TVR
		r = EMV_ODA_DDA_FAILED;
		goto exit;
	}
	emv_debug_info("Valid Signed Dynamic Application Data hash");

	// Append INTERNAL AUTHENTICATE output to ICC data list
	r = emv_tlv_list_append(&ctx->icc, &list);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_append() failed; r=%d", r);

		// Internal error; terminate session
		emv_debug_error("Internal error");
		r = EMV_ODA_ERROR_INTERNAL;
		goto exit;
	}

	// Create ICC Dynamic Number (field 9F4C)
	// See EMV 4.4 Book 2, 6.5.2 (page 64)
	r = emv_tlv_list_push(
		&ctx->icc,
		EMV_TAG_9F4C_ICC_DYNAMIC_NUMBER,
		sdad.icc_dynamic_number_len,
		sdad.icc_dynamic_number,
		0
	);
	if (r) {
		emv_debug_trace_msg("emv_tlv_list_push() failed; r=%d", r);
		emv_debug_error("Internal error");
		// EMV_TVR_DDA_FAILED already set in TVR
		r = EMV_ODA_ERROR_INTERNAL;
		goto exit;
	}

	// Successful DDA processing
	emv_debug_info("Dynamic Data Authentication (DDA) succeeded");
	ctx->tvr->value[0] &= ~EMV_TVR_DDA_FAILED;
	r = 0;
	goto exit;

exit:
	// Cleanse ICC public key because it contains the PAN
	crypto_cleanse(&icc_pkey, sizeof(icc_pkey));
	return r;
}

int emv_oda_apply_cda(struct emv_ctx_t* ctx)
{
	int r;
	const struct emv_tlv_t* un;
	struct emv_rsa_icc_pkey_t icc_pkey;

	if (!ctx) {
		emv_debug_trace_msg("ctx=%p", ctx);
		emv_debug_error("Invalid parameter");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}
	if (!ctx->aid || !ctx->tvr || !ctx->tsi || !ctx->aip) {
		emv_debug_trace_msg("aid=%p, tvr=%p, tsi=%p, aip=%p",
			ctx->aid, ctx->tvr, ctx->tsi, ctx->aip
		);
		emv_debug_error("Invalid context variable");
		return EMV_ODA_ERROR_INVALID_PARAMETER;
	}

	un = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9F37_UNPREDICTABLE_NUMBER);
	if (!un) {
		// Unpredictable Number should have been created by
		// emv_initiate_application_processing()
		emv_debug_error("Unpredictable Number not found");
		return EMV_ODA_ERROR_INTERNAL;
	}

	// Assume that CDA has failed until the related steps have succeeded. Note
	// that TSI is set here as if an error occurred and must be unset again at
	// the end of the function in preparation for GENAC1.
	// See EMV 4.4 Book 3, 10.3 (page 98)
	emv_debug_info("Select Combined DDA/Application Cryptogram Generation (CDA)");
	ctx->tvr->value[0] |=
		EMV_TVR_CDA_FAILED |
		EMV_TVR_ICC_DATA_MISSING;
	ctx->tsi->value[0] |= EMV_TSI_OFFLINE_DATA_AUTH_PERFORMED;

	// Authenticate static application data and retrieve ICC public key
	// See EMV 4.4 Book 2, 6.1 - 6.4
	r = emv_oda_apply_sad_auth(ctx, &icc_pkey);
	if (r) {
		emv_debug_trace_msg("emv_oda_apply_sad_auth() failed; r=%d", r);

		if (r < 0 || r == EMV_ODA_ICC_DATA_MISSING) {
			// EMV_TVR_CDA_FAILED and EMV_TVR_ICC_DATA_MISSING already set in TVR
			// Return error as-is
			goto exit;
		}

		ctx->tvr->value[0] &= ~EMV_TVR_ICC_DATA_MISSING;
		if (r == EMV_ODA_SAD_AUTH_FAILED) {
			// EMV_TVR_CDA_FAILED already set in TVR
			r = EMV_ODA_CDA_FAILED;
			goto exit;
		}

		emv_debug_error("Internal error");
		// EMV_TVR_CDA_FAILED already set in TVR
		r = EMV_ODA_ERROR_INTERNAL;
		goto exit;
	}
	ctx->tvr->value[0] &= ~EMV_TVR_ICC_DATA_MISSING;

	// Successful CDA processing
	emv_debug_info("Combined DDA/Application Cryptogram Generation (CDA) applied");
	ctx->tvr->value[0] &= ~EMV_TVR_CDA_FAILED;
	ctx->tsi->value[0] &= ~EMV_TSI_OFFLINE_DATA_AUTH_PERFORMED;
	r = 0;
	goto exit;

exit:
	// Cleanse ICC public key because it contains the PAN
	crypto_cleanse(&icc_pkey, sizeof(icc_pkey));
	return r;
}
