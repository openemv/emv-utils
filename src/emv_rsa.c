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
#include "emv_fields.h"
#include "emv_capk.h"
#include "emv_tlv.h"
#include "emv_tags.h"

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

int emv_rsa_retrieve_issuer_pkey(
	const uint8_t* issuer_cert,
	size_t issuer_cert_len,
	const struct emv_capk_t* capk,
	const struct emv_tlv_list_t* icc,
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

	// See EMV 4.4 Book 2, 5.3, step 5 - 6
	r = crypto_sha1_init(&sha1_ctx);
	if (r) {
		// Internal error
		r = -8;
		goto exit;
	}
	r = crypto_sha1_update(
		&sha1_ctx,
		&cert.format,
		cert_meta_len - offsetof(struct emv_rsa_issuer_cert_t, format)
	);
	if (r) {
		// Internal error
		r = -9;
		goto exit;
	}
	r = crypto_sha1_update(
		&sha1_ctx,
		cert.body,
		cert_modulus_len // Use data as-is, including padding
	);
	if (r) {
		// Internal error
		r = -10;
		goto exit;
	}
	if (remainder_tlv) {
		r = crypto_sha1_update(
			&sha1_ctx,
			remainder_tlv->value,
			remainder_tlv->length
		);
		if (r) {
			// Internal error
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
		// Internal error
		r = -12;
		goto exit;
	}
	r = crypto_sha1_finish(&sha1_ctx, hash);
	if (r) {
		// Internal error
		r = -13;
		goto exit;
	}

	// See EMV 4.4 Book 2, 5.3, step 7
	if (memcmp(hash, cert.body + cert_modulus_len, cert_hash_len) != 0) {
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
	struct emv_rsa_ssad_t* data
)
{
	int r;
	struct emv_rsa_decrypted_ssad_t decrypted_ssad;
	const size_t ssad_meta_len = offsetof(struct emv_rsa_decrypted_ssad_t, body);
	size_t ssad_pad_len;
	const size_t ssad_hash_len = 20;

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

	return 0;
}

int emv_rsa_retrieve_icc_pkey(
	const uint8_t* icc_cert,
	size_t icc_cert_len,
	const struct emv_rsa_issuer_pkey_t* issuer_pkey,
	const struct emv_tlv_list_t* icc,
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

	// Success
	r = 0;
	goto exit;

exit:
	// Cleanse decrypted certificate because it contains the PAN
	crypto_cleanse(&cert, sizeof(cert));
	return r;
}
