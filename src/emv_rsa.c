/**
 * @file emv_rsa.c
 * @brief EMV RSA helper functions
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

#include "emv_rsa.h"
#include "emv_oda.h"
#include "emv_fields.h"
#include "emv_capk.h"
#include "emv_tlv.h"
#include "emv_tags.h"
#include "emv_date.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_ODA
#include "emv_debug.h"

#include "crypto_rsa.h"
#include "crypto_sha.h"
#include "crypto_mem.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// See EMV 4.4 Book 2, 5.3, table 6
struct emv_rsa_issuer_cert_t {
	uint8_t header;
	uint8_t format;
	uint8_t issuer_id[4];
	uint8_t cert_exp[2];
	uint8_t cert_sn[3];
	uint8_t hash_id;
	uint8_t alg_id;
	uint8_t modulus_len;
	uint8_t exponent_len;
	// Body of issuer public key consisting of leftmost digits of public key,
	// hash result, and trailer
	uint8_t body[(1984 / 8) - 36 + 20 + 1];
} __attribute__((packed));

// See EMV 4.4 Book 2, 5.4, table 7
struct emv_rsa_decrypted_ssad_t {
	uint8_t header;
	uint8_t format;
	uint8_t hash_id;
	uint8_t data_auth_code[2];
	uint8_t body[(1984 / 8) - 26 + 20 + 1];
} __attribute__((packed));

// See EMV 4.4 Book 2, 6.4, table 14
struct emv_rsa_icc_cert_t {
	uint8_t header;
	uint8_t format;
	uint8_t pan[10];
	uint8_t cert_exp[2];
	uint8_t cert_sn[3];
	uint8_t hash_id;
	uint8_t alg_id;
	uint8_t modulus_len;
	uint8_t exponent_len;
	// Body of ICC public key consisting of leftmost digits of public key,
	// hash result, and trailer
	uint8_t body[(1984 / 8) - 42 + 20 + 1];
} __attribute__((packed));

// See EMV 4.4 Book 2, 6.5.2, table 17
struct emv_rsa_decrypted_sdad_t {
	uint8_t header;
	uint8_t format;
	uint8_t hash_id;
	uint8_t icc_dynamic_data_len;
	uint8_t body[(1984 / 8) - 25 + 20 + 1];
} __attribute__((packed));

int emv_rsa_retrieve_issuer_pkey(
	const uint8_t* issuer_cert,
	size_t issuer_cert_len,
	const struct emv_capk_t* capk,
	const struct emv_tlv_list_t* icc,
	const struct emv_tlv_list_t* params,
	struct emv_rsa_issuer_pkey_t* pkey
)
{
	int r;
	struct emv_rsa_issuer_cert_t cert;
	const size_t cert_meta_len = offsetof(struct emv_rsa_issuer_cert_t, body);
	size_t cert_modulus_len;
	const size_t cert_hash_len = 20;
	crypto_sha1_ctx_t sha1_ctx = NULL;
	uint8_t hash[SHA1_SIZE];
	const struct emv_tlv_t* remainder_tlv;
	const struct emv_tlv_t* exponent_tlv;
	const struct emv_tlv_t* pan_tlv;
	const struct emv_tlv_t* txn_date_tlv;

	if (!issuer_cert || !issuer_cert_len || !capk || !pkey) {
		return -1;
	}
	memset(pkey, 0, sizeof(*pkey));

	// Ensure that key sizes match
	// See EMV 4.4 Book 2, 5.3, step 1
	if (capk->modulus_len != issuer_cert_len ||
		capk->modulus_len > sizeof(cert)
	) {
		// Unsuitable CAPK modulus length
		return -2;
	}

	// Ensure that issuer public key is at least 512-bit
	if (issuer_cert_len < cert_meta_len + (512 / 8) + cert_hash_len + 1) {
		// Unsuitable issuer public key modulus length
		return -3;
	}

	// Decrypt Issuer Public Key Certificate (field 90)
	// See EMV 4.4 Book 2, 5.3, step 2
	r = crypto_rsa_mod_exp(
		capk->modulus,
		capk->modulus_len,
		capk->exponent,
		capk->exponent_len,
		issuer_cert,
		&cert
	);
	if (r) {
		r = -4;
		goto exit;
	}
	cert_modulus_len = issuer_cert_len - cert_meta_len - cert_hash_len - 1;

	// Validate various certificate fields
	// See EMV 4.4 Book 2, 5.3, step 2 - 4
	// See EMV 4.4 Book 2, 5.3, table 6
	if (
		// Step 2
		cert.body[cert_modulus_len + cert_hash_len] != 0xBC ||
		// Step 3
		cert.header != 0x6A ||
		// Step 4
		cert.format != EMV_RSA_FORMAT_ISSUER_CERT ||
		// Sanity check
		cert.modulus_len > sizeof(pkey->modulus) ||
		cert.exponent_len > sizeof(pkey->exponent)
	) {
		// Incorrect CAPK
		r = -5;
		goto exit;
	}
	// See EMV 4.4 Book 2, 5.3, step 6
	if (cert.hash_id != EMV_PKEY_HASH_SHA1) {
		// Unsupported hash algorithm indicator
		r = -6;
		goto exit;
	}
	// See EMV 4.4 Book 2, 5.3, step 11
	if (cert.alg_id != EMV_PKEY_SIG_RSA_SHA1) {
		// Unsupported public key algorithm indicator
		r = -7;
		goto exit;
	}

	// Populate available issuer public key fields because decryption has
	// succeeded.
	pkey->format = cert.format;
	memcpy(pkey->issuer_id, cert.issuer_id, sizeof(pkey->issuer_id));
	memcpy(pkey->cert_exp, cert.cert_exp, sizeof(pkey->cert_exp));
	memcpy(pkey->cert_sn, cert.cert_sn, sizeof(pkey->cert_sn));
	pkey->hash_id = cert.hash_id;
	pkey->alg_id = cert.alg_id;
	pkey->modulus_len = cert.modulus_len;
	pkey->exponent_len = cert.exponent_len;

	if (!icc) {
		// Optional fields not available. Full certificate retrieval and hash
		// validation not possible.
		r = 1;
		goto exit;
	}

	// Populate issuer public key modulus
	// NOTE: Remainder is optional if length is zero
	// See EMV 4.4 Book 2, 5.3, step 12
	// See EMV 4.4 Book 3, 7.2, footnote 5
	remainder_tlv = emv_tlv_list_find_const(icc, EMV_TAG_92_ISSUER_PUBLIC_KEY_REMAINDER);
	if (cert.modulus_len > cert_modulus_len) {
		if (!remainder_tlv) {
			// Remainder not available. Modulus retrieval not posssible.
			r = 2;
			goto exit;
		}
		if (cert.modulus_len != cert_modulus_len + remainder_tlv->length) {
			// Invalid remainder length. Modulus retrieval not posssible.
			r = 3;
			goto exit;
		}
		memcpy(pkey->modulus, cert.body, cert_modulus_len);
		memcpy(pkey->modulus + cert_modulus_len, remainder_tlv->value, remainder_tlv->length);
	} else {
		memcpy(pkey->modulus, cert.body, cert.modulus_len);
	}

	// Populate issuer public key exponent
	// NOTE: Exponent is mandatory for validation
	// See EMV 4.4 Book 3, 7.2, table 29
	exponent_tlv = emv_tlv_list_find_const(icc, EMV_TAG_9F32_ISSUER_PUBLIC_KEY_EXPONENT);
	if (!exponent_tlv || !exponent_tlv->length) {
		// Exponent not available. Certificate hash validation not possible.
		r = 4;
		goto exit;
	}
	if (exponent_tlv->length != pkey->exponent_len) {
		// Invalid exponent length. Certificate hash validation not possible.
		r = 5;
		goto exit;
	}
	memcpy(pkey->exponent, exponent_tlv->value, exponent_tlv->length);

	// Compute hash
	// See EMV 4.4 Book 2, 5.3, step 5 - 6
	r = crypto_sha1_init(&sha1_ctx);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_init() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -8;
		goto exit;
	}
	r = crypto_sha1_update(
		&sha1_ctx,
		&cert.format,
		cert_meta_len + cert_modulus_len - offsetof(struct emv_rsa_issuer_cert_t, format)
	);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_update() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -9;
		goto exit;
	}
	if (remainder_tlv) {
		r = crypto_sha1_update(
			&sha1_ctx,
			remainder_tlv->value,
			remainder_tlv->length
		);
		if (r) {
			emv_debug_trace_msg("crypto_sha1_update() failed; r=%d", r);
			emv_debug_error("Internal error");
			r = -11;
			goto exit;
		}
	}
	r = crypto_sha1_update(
		&sha1_ctx,
		exponent_tlv->value,
		exponent_tlv->length
	);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_update() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -12;
		goto exit;
	}
	r = crypto_sha1_finish(&sha1_ctx, hash);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_finish() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -13;
		goto exit;
	}

	// Validate hash
	// See EMV 4.4 Book 2, 5.3, step 7
	if (crypto_memcmp_s(hash, cert.body + cert_modulus_len, cert_hash_len) != 0) {
		// Certificate hash validation failed
		r = 6;
		goto exit;
	}

	// See EMV 4.4 Book 2, 5.3, step 8
	pan_tlv = emv_tlv_list_find_const(icc, EMV_TAG_5A_APPLICATION_PAN);
	if (!pan_tlv || pan_tlv->length < sizeof(pkey->issuer_id)) {
		// PAN not available or not valid. Issuer identifier validation not
		// possible.
		r = 7;
		goto exit;
	}
	for (size_t i = 0; i < sizeof(pkey->issuer_id); ++i) {
		if (pkey->issuer_id[i] == 0xFF) {
			// Skip padding
			continue;
		}
		if ((pkey->issuer_id[i] & 0x0F) == 0x0F) {
			// Only compare first nibble of byte
			if ((pkey->issuer_id[i] & 0xF0) != (pan_tlv->value[i] & 0xF0)) {
				// Issuer identifier is invalid
				r = 8;
				goto exit;
			}
		}
		if (pkey->issuer_id[i] != pan_tlv->value[i]) {
			// Issuer identifier is invalid
			r = 9;
			goto exit;
		}
	}

	// See EMV 4.4 Book 2, 5.3, step 9
	txn_date_tlv = emv_tlv_list_find_const(params, EMV_TAG_9A_TRANSACTION_DATE);
	if (emv_date_mmyy_is_expired(txn_date_tlv, pkey->cert_exp)) {
		// Transaction date not available, transaction date not valid, or
		// certificate is expired
		emv_debug_trace_data("Certificate expiration date", pkey->cert_exp, sizeof(pkey->cert_exp));
		emv_debug_error("Certificate is expired");
		r = 10;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	crypto_sha1_free(&sha1_ctx);
	// Cleanse decrypted certificate because it contains up to 8 PAN digits
	crypto_cleanse(&cert, sizeof(cert));
	return r;
}

int emv_rsa_retrieve_ssad(
	const uint8_t* ssad,
	size_t ssad_len,
	const struct emv_rsa_issuer_pkey_t* issuer_pkey,
	const struct emv_oda_ctx_t* oda,
	struct emv_rsa_ssad_t* data
)
{
	int r;
	struct emv_rsa_decrypted_ssad_t decrypted_ssad;
	const size_t ssad_meta_len = offsetof(struct emv_rsa_decrypted_ssad_t, body);
	size_t ssad_pad_len;
	const size_t ssad_hash_len = 20;
	crypto_sha1_ctx_t sha1_ctx = NULL;
	uint8_t hash[SHA1_SIZE];

	if (!ssad || !ssad_len || !issuer_pkey || !data) {
		return -1;
	}
	memset(data, 0, sizeof(*data));

	// Ensure that key and signature sizes match
	// See EMV 4.4 Book 2, 5.4, step 1
	if (issuer_pkey->modulus_len != ssad_len ||
		issuer_pkey->modulus_len > sizeof(decrypted_ssad)
	) {
		// Unsuitable issuer public key modulus length
		return -2;
	}

	// Decrypt Signed Static Application Data (field 93)
	// See EMV 4.4 Book 2, 5.4, step 2
	r = crypto_rsa_mod_exp(
		issuer_pkey->modulus,
		issuer_pkey->modulus_len,
		issuer_pkey->exponent,
		issuer_pkey->exponent_len,
		ssad,
		&decrypted_ssad
	);
	if (r) {
		return -3;
	}
	ssad_pad_len = ssad_len - ssad_meta_len - ssad_hash_len - 1;

	// Validate various data fields
	// See EMV 4.4 Book 2, 5.4, step 2 - 4
	// See EMV 4.4 Book 2, 5.4, table 7
	if (
		// Step 2
		decrypted_ssad.body[ssad_pad_len + ssad_hash_len] != 0xBC ||
		// Step 3
		decrypted_ssad.header != 0x6A ||
		// Step 4
		decrypted_ssad.format != EMV_RSA_FORMAT_SSAD
	) {
		// Incorrect issuer public key
		return -4;
	}
	// See EMV 4.4 Book 2, 5.4, step 6
	if (decrypted_ssad.hash_id != EMV_PKEY_HASH_SHA1) {
		// Unsupported hash algorithm indicator
		return -5;
	}

	// Populate output
	data->format = decrypted_ssad.format;
	data->hash_id = decrypted_ssad.hash_id;
	memcpy(data->data_auth_code, decrypted_ssad.data_auth_code, sizeof(data->data_auth_code));
	memcpy(data->hash, decrypted_ssad.body + ssad_pad_len, sizeof(data->hash));

	if (!oda) {
		// Optional Offline Data Authentication (ODA) context not available.
		// Hash validation not possible.
		r = 1;
		goto exit;
	}

	// Compute hash
	// See EMV 4.4 Book 2, 5.4, step 5 - 6
	r = crypto_sha1_init(&sha1_ctx);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_init() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -6;
		goto exit;
	}
	r = crypto_sha1_update(
		&sha1_ctx,
		&decrypted_ssad.format,
		ssad_meta_len + ssad_pad_len - offsetof(struct emv_rsa_decrypted_ssad_t, format)
	);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_update() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -7;
		goto exit;
	}
	if (oda->record_buf && oda->record_buf_len) {
		r = crypto_sha1_update(
			&sha1_ctx,
			oda->record_buf,
			oda->record_buf_len
		);
		if (r) {
			emv_debug_trace_msg("crypto_sha1_update() failed; r=%d", r);
			emv_debug_error("Internal error");
			r = -8;
			goto exit;
		}
	}
	r = crypto_sha1_finish(&sha1_ctx, hash);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_finish() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -9;
		goto exit;
	}

	// Verify hash
	// See EMV 4.4 Book 2, 5.4, step 7
	emv_debug_trace_data("Computed hash", hash, sizeof(hash));
	emv_debug_trace_data("SSAD obj hash", data->hash, sizeof(data->hash));
	if (crypto_memcmp_s(hash, data->hash, sizeof(data->hash)) != 0) {
		emv_debug_error("Invalid hash");
		r = 2;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	crypto_sha1_free(&sha1_ctx);
	return r;
}

int emv_rsa_retrieve_icc_pkey(
	const uint8_t* icc_cert,
	size_t icc_cert_len,
	const struct emv_rsa_issuer_pkey_t* issuer_pkey,
	const struct emv_tlv_list_t* icc,
	const struct emv_tlv_list_t* params,
	const struct emv_oda_ctx_t* oda,
	struct emv_rsa_icc_pkey_t* pkey
)
{
	int r;
	struct emv_rsa_icc_cert_t cert;
	const size_t cert_meta_len = offsetof(struct emv_rsa_icc_cert_t, body);
	size_t cert_modulus_len;
	const size_t cert_hash_len = 20;
	const struct emv_tlv_t* remainder_tlv;
	const struct emv_tlv_t* exponent_tlv;
	const struct emv_tlv_t* pan_tlv;
	const struct emv_tlv_t* txn_date_tlv;
	crypto_sha1_ctx_t sha1_ctx = NULL;
	uint8_t hash[SHA1_SIZE];

	if (!icc_cert || !icc_cert_len || !issuer_pkey || !pkey) {
		return -1;
	}
	memset(pkey, 0, sizeof(*pkey));

	// Ensure that key sizes match
	// See EMV 4.4 Book 2, 6.4, step 1
	if (issuer_pkey->modulus_len != icc_cert_len ||
		issuer_pkey->modulus_len > sizeof(cert)
	) {
		// Unsuitable issuer public key modulus length
		return -2;
	}

	// Ensure that ICC public key is at least 512-bit
	if (icc_cert_len < cert_meta_len + (512 / 8) + cert_hash_len + 1) {
		// Unsuitable ICC public key modulus length
		return -3;
	}

	// Decrypt ICC Public Key Certificate (field 9F46)
	// See EMV 4.4 Book 2, 6.4, step 2
	r = crypto_rsa_mod_exp(
		issuer_pkey->modulus,
		issuer_pkey->modulus_len,
		issuer_pkey->exponent,
		issuer_pkey->exponent_len,
		icc_cert,
		&cert
	);
	if (r) {
		r = -4;
		goto exit;
	}
	cert_modulus_len = icc_cert_len - cert_meta_len - cert_hash_len - 1;

	// Validate various certificate fields
	// See EMV 4.4 Book 2, 6.4, step 2 - 4
	// See EMV 4.4 Book 2, 6.4, table 14
	if (
		// Step 2
		cert.body[cert_modulus_len + cert_hash_len] != 0xBC ||
		// Step 3
		cert.header != 0x6A ||
		// Step 4
		cert.format != EMV_RSA_FORMAT_ICC_CERT ||
		// Sanity check
		cert.modulus_len > sizeof(pkey->modulus) ||
		cert.exponent_len > sizeof(pkey->exponent)
	) {
		// Incorrect issuer public key
		r = -5;
		goto exit;
	}
	// See EMV 4.4 Book 2, 6.4, step 6
	if (cert.hash_id != EMV_PKEY_HASH_SHA1) {
		// Unsupported hash algorithm indicator
		r = -6;
		goto exit;
	}
	// See EMV 4.4 Book 2, 6.4, step 10
	if (cert.alg_id != EMV_PKEY_SIG_RSA_SHA1) {
		// Unsupported public key algorithm indicator
		r = -7;
		goto exit;
	}

	// Populate available ICC public key fields because decryption has
	// succeeded.
	pkey->format = cert.format;
	memcpy(pkey->pan, cert.pan, sizeof(pkey->pan));
	memcpy(pkey->cert_exp, cert.cert_exp, sizeof(pkey->cert_exp));
	memcpy(pkey->cert_sn, cert.cert_sn, sizeof(pkey->cert_sn));
	pkey->hash_id = cert.hash_id;
	pkey->alg_id = cert.alg_id;
	pkey->modulus_len = cert.modulus_len;
	pkey->exponent_len = cert.exponent_len;
	memcpy(pkey->hash, cert.body + cert_modulus_len, sizeof(pkey->hash));

	if (!icc) {
		// Optional fields not available. Full certificate retrieval not
		// possible.
		r = 1;
		goto exit;
	}

	// Populate ICC public key modulus
	// NOTE: Remainder is optional if length is zero
	// See EMV 4.4 Book 2, 6.4, step 11
	// See EMV 4.4 Book 3, 7.2, footnote 5
	remainder_tlv = emv_tlv_list_find_const(icc, EMV_TAG_9F48_ICC_PUBLIC_KEY_REMAINDER);
	if (cert.modulus_len > cert_modulus_len) {
		if (!remainder_tlv) {
			// Remainder not available. Modulus retrieval not posssible.
			r = 2;
			goto exit;
		}
		if (cert.modulus_len != cert_modulus_len + remainder_tlv->length) {
			// Invalid remainder length. Modulus retrieval not posssible.
			r = 3;
			goto exit;
		}
		memcpy(pkey->modulus, cert.body, cert_modulus_len);
		memcpy(pkey->modulus + cert_modulus_len, remainder_tlv->value, remainder_tlv->length);
	} else {
		memcpy(pkey->modulus, cert.body, cert.modulus_len);
	}

	// Populate ICC public key exponent
	// See EMV 4.4 Book 3, 7.2, table 30
	exponent_tlv = emv_tlv_list_find_const(icc, EMV_TAG_9F47_ICC_PUBLIC_KEY_EXPONENT);
	if (!exponent_tlv || !exponent_tlv->length) {
		// Exponent not available. Full certificate retrieval not possible.
		r = 4;
		goto exit;
	}
	if (exponent_tlv->length != pkey->exponent_len) {
		// Invalid exponent length. Full certificate retrieval not possible.
		r = 5;
		goto exit;
	}
	memcpy(pkey->exponent, exponent_tlv->value, exponent_tlv->length);

	// Validate PAN
	// See EMV 4.4 Book 2, 6.4, step 8
	pan_tlv = emv_tlv_list_find_const(icc, EMV_TAG_5A_APPLICATION_PAN);
	if (!pan_tlv || pan_tlv->length < 6) {
		// PAN not available or not valid. PAN validation not possible.
		r = 6;
		goto exit;
	}
	for (size_t i = 0; i < sizeof(pkey->pan); ++i) {
		if (pkey->pan[i] == 0xFF) {
			// Skip padding
			continue;
		}
		if ((pkey->pan[i] & 0x0F) == 0x0F) {
			// Only compare first nibble of byte
			if ((pkey->pan[i] & 0xF0) != (pan_tlv->value[i] & 0xF0)) {
				// PAN is invalid
				r = 7;
				goto exit;
			}
		}
		if (pkey->pan[i] != pan_tlv->value[i]) {
			// PAN is invalid
			r = 8;
			goto exit;
		}
	}

	// See EMV 4.4 Book 2, 6.4, step 9
	txn_date_tlv = emv_tlv_list_find_const(params, EMV_TAG_9A_TRANSACTION_DATE);
	if (emv_date_mmyy_is_expired(txn_date_tlv, pkey->cert_exp)) {
		// Transaction date not available, transaction date not valid, or
		// certificate is expired
		emv_debug_trace_data("Certificate expiration date", pkey->cert_exp, sizeof(pkey->cert_exp));
		emv_debug_error("Certificate is expired");
		r = 9;
		goto exit;
	}

	if (!oda) {
		// Optional Offline Data Authentication (ODA) context not available.
		// Hash validation not possible.
		r = 10;
		goto exit;
	}

	// Compute hash
	// See EMV 4.4 Book 2, 6.4, step 5 - 6
	r = crypto_sha1_init(&sha1_ctx);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_init() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -8;
		goto exit;
	}
	r = crypto_sha1_update(
		&sha1_ctx,
		&cert.format,
		cert_meta_len + cert_modulus_len - offsetof(struct emv_rsa_icc_cert_t, format)
	);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_update() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -9;
		goto exit;
	}
	if (remainder_tlv) {
		r = crypto_sha1_update(
			&sha1_ctx,
			remainder_tlv->value,
			remainder_tlv->length
		);
		if (r) {
			emv_debug_trace_msg("crypto_sha1_update() failed; r=%d", r);
			emv_debug_error("Internal error");
			r = -10;
			goto exit;
		}
	}
	r = crypto_sha1_update(
		&sha1_ctx,
		exponent_tlv->value,
		exponent_tlv->length
	);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_update() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -11;
		goto exit;
	}
	if (oda->record_buf && oda->record_buf_len) {
		r = crypto_sha1_update(
			&sha1_ctx,
			oda->record_buf,
			oda->record_buf_len
		);
		if (r) {
			emv_debug_trace_msg("crypto_sha1_update() failed; r=%d", r);
			emv_debug_error("Internal error");
			r = -12;
			goto exit;
		}
	}
	r = crypto_sha1_finish(&sha1_ctx, hash);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_finish() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -13;
		goto exit;
	}

	// Verify hash
	// See EMV 4.4 Book 2, 6.4, step 7
	emv_debug_trace_data("Computed hash", hash, sizeof(hash));
	emv_debug_trace_data("ICC cert hash", pkey->hash, sizeof(pkey->hash));
	if (crypto_memcmp_s(hash, pkey->hash, sizeof(pkey->hash)) != 0) {
		emv_debug_error("Invalid hash");
		r = 11;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	crypto_sha1_free(&sha1_ctx);
	// Cleanse decrypted certificate because it contains the PAN
	crypto_cleanse(&cert, sizeof(cert));
	return r;
}

int emv_rsa_retrieve_sdad(
	const uint8_t* sdad,
	size_t sdad_len,
	const struct emv_rsa_icc_pkey_t* icc_pkey,
	const void* ddol_data,
	size_t ddol_data_len,
	struct emv_rsa_sdad_t* data
)
{
	int r;
	struct emv_rsa_decrypted_sdad_t decrypted_sdad;
	const size_t sdad_meta_len = offsetof(struct emv_rsa_decrypted_sdad_t, body);
	size_t sdad_dynamic_len;
	const size_t sdad_hash_len = 20;
	crypto_sha1_ctx_t sha1_ctx = NULL;
	uint8_t hash[SHA1_SIZE];

	if (!sdad || !sdad_len || !icc_pkey || !data) {
		return -1;
	}
	memset(data, 0, sizeof(*data));

	// Ensure that key and signature sizes match
	// See EMV 4.4 Book 2, 6.5.2, step 1
	// See EMV 4.4 Book 2, 6.6.2, step 1
	if (icc_pkey->modulus_len != sdad_len ||
		icc_pkey->modulus_len > sizeof(decrypted_sdad)
	) {
		// Unsuitable ICC public key modulus length
		return -2;
	}

	// Decrypt Signed Dynamic Application Data (field 9F4B)
	// See EMV 4.4 Book 2, 6.5.2, step 2
	// See EMV 4.4 Book 2, 6.6.2, step 2
	r = crypto_rsa_mod_exp(
		icc_pkey->modulus,
		icc_pkey->modulus_len,
		icc_pkey->exponent,
		icc_pkey->exponent_len,
		sdad,
		&decrypted_sdad
	);
	if (r) {
		return -3;
	}
	sdad_dynamic_len = sdad_len - sdad_meta_len - sdad_hash_len - 1;

	// Validate various data fields
	// See EMV 4.4 Book 2, 6.5.2, step 2 - 4
	// See EMV 4.4 Book 2, 6.5.2, table 17
	// See EMV 4.4 Book 2, 6.6.2, step 2 - 4
	// See EMV 4.4 Book 2, 6.6.2, table 22
	// See EMV 4.4 Book 2, 6.6.1, table 19
	if (
		// Step 2
		decrypted_sdad.body[sdad_dynamic_len + sdad_hash_len] != 0xBC ||
		// Step 3
		decrypted_sdad.header != 0x6A ||
		// Step 4
		decrypted_sdad.format != EMV_RSA_FORMAT_SDAD ||
		// Sanity check
		decrypted_sdad.icc_dynamic_data_len > 1 + 8 + 1 + 8 + 20 ||
		(decrypted_sdad.icc_dynamic_data_len > 0 && decrypted_sdad.body[0] > 8)
	) {
		// Incorrect ICC public key
		return -4;
	}
	// See EMV 4.4 Book 2, 6.5.2, step 6
	// See EMV 4.4 Book 2, 6.6.2, step 8
	if (decrypted_sdad.hash_id != EMV_PKEY_HASH_SHA1) {
		// Unsupported hash algorithm indicator
		return -5;
	}

	// Populate output
	data->format = decrypted_sdad.format;
	data->hash_id = decrypted_sdad.hash_id;
	if (decrypted_sdad.icc_dynamic_data_len > 0) {
		// Extract ICC Dynamic Number (field 9F4C)
		data->icc_dynamic_number_len = decrypted_sdad.body[0];
		if (data->icc_dynamic_number_len > 0 &&
			data->icc_dynamic_number_len <= sizeof(data->icc_dynamic_number)
		) {
			memcpy(data->icc_dynamic_number, decrypted_sdad.body + 1, data->icc_dynamic_number_len);
		}

		// Extract other fields from ICC Dynamic Data
		// See EMV 4.4 Book 2, 6.6.2, step 5
		// See EMV 4.4 Book 2, 6.6.1, table 19
		if (decrypted_sdad.icc_dynamic_data_len ==
			1 + data->icc_dynamic_number_len + 1 + 8 + 20) {

			const uint8_t* ptr = decrypted_sdad.body + 1 + data->icc_dynamic_number_len;

			// Extract Cryptogram Information Data (field 9F27)
			data->cid = *ptr;
			++ptr;

			// Extract Application Cryptogram (field 9F26)
			memcpy(data->cryptogram, ptr, sizeof(data->cryptogram));
			ptr += sizeof(data->cryptogram);

			// Extract Transaction Data Hash Code
			memcpy(data->txn_data_hash_code, ptr, sizeof(data->txn_data_hash_code));
			ptr += sizeof(data->txn_data_hash_code);
		}
	}
	memcpy(data->hash, decrypted_sdad.body + sdad_dynamic_len, sizeof(data->hash));

	if (!ddol_data) {
		// Concatenated DDOL data not available. Hash validation not possible.
		r = 1;
		goto exit;
	}

	// Compute hash
	// See EMV 4.4 Book 2, 6.5.2, step 5 - 6
	// See EMV 4.4 Book 2, 6.6.2, step 7 - 8
	r = crypto_sha1_init(&sha1_ctx);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_init() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -6;
		goto exit;
	}
	r = crypto_sha1_update(
		&sha1_ctx,
		&decrypted_sdad.format,
		sdad_meta_len + sdad_dynamic_len - offsetof(struct emv_rsa_decrypted_sdad_t, format)
	);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_update() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -7;
		goto exit;
	}
	if (ddol_data && ddol_data_len) {
		r = crypto_sha1_update(
			&sha1_ctx,
			ddol_data,
			ddol_data_len
		);
		if (r) {
			emv_debug_trace_msg("crypto_sha1_update() failed; r=%d", r);
			emv_debug_error("Internal error");
			r = -8;
			goto exit;
		}
	}
	r = crypto_sha1_finish(&sha1_ctx, hash);
	if (r) {
		emv_debug_trace_msg("crypto_sha1_finish() failed; r=%d", r);
		emv_debug_error("Internal error");
		r = -9;
		goto exit;
	}

	// Verify hash
	// See EMV 4.4 Book 2, 6.5.2, step 7
	// See EMV 4.4 Book 2, 6.6.2, step 9
	emv_debug_trace_data("Computed hash", hash, sizeof(hash));
	emv_debug_trace_data("SDAD obj hash", data->hash, sizeof(data->hash));
	if (crypto_memcmp_s(hash, data->hash, sizeof(data->hash)) != 0) {
		emv_debug_error("Invalid hash");
		r = 2;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	crypto_sha1_free(&sha1_ctx);
	return r;
}
