/**
 * @file emv_rsa.h
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

#ifndef EMV_RSA_H
#define EMV_RSA_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

// Forward declarations
struct emv_tlv_list_t;
struct emv_capk_t;

/**
 * @name EMV RSA formats
 * @remark See EMV Book 4.4 Book 2
 * @anchor emv-rsa-format-values
 */
/// @{
#define EMV_RSA_FORMAT_ISSUER_CERT              (0x02) ///< Issuer Public Key Certificate format
/// @}

/**
 * Issuer public key
 * @remark See EMV 4.4 Book 2, 5.3, Table 6
 *
 * This structure is intended to represent the complete and validated Issuer
 * Public Key created from the combination of these fields:
 * - @ref EMV_TAG_90_ISSUER_PUBLIC_KEY_CERTIFICATE
 * - @ref EMV_TAG_92_ISSUER_PUBLIC_KEY_REMAINDER
 * - @ref EMV_TAG_9F32_ISSUER_PUBLIC_KEY_EXPONENT
 */
struct emv_rsa_issuer_pkey_t {
	uint8_t format; ///< Certificate Format. Must be @ref EMV_RSA_FORMAT_ISSUER_CERT.
	uint8_t issuer_id[4]; ///< Issuer Identifier. Leftmost 3-8 digits from the PAN (padded to the right with hex 'F's).
	uint8_t cert_exp[2]; ///< Certificate Expiration Date (MMYY)
	uint8_t cert_sn[3]; ///< Binary number unique to this certificate
	uint8_t hash_id; ///< Hash algorithm indicator. Must be @ref EMV_PKEY_HASH_SHA1.
	uint8_t alg_id; ///< Public key algorithm indicator. Must be @ref EMV_PKEY_SIG_RSA_SHA1.
	uint8_t modulus_len; ///< Public key modulus length in bytes
	uint8_t exponent_len; ///< Public key exponent length in bytes
	uint8_t modulus[1984 / 8]; ///< Public key modulus
	uint8_t exponent[3]; ///< Public key exponent
};

/**
 * Retrieve issuer public key and optionally validate certificate hash
 * @remark See EMV 4.4 Book 2, 5.3
 *
 * The following optional fields can be provided in @p icc and contribute to
 * issuer public key retrieval and validation:
 * - @ref EMV_TAG_92_ISSUER_PUBLIC_KEY_REMAINDER will be used to obtain the
 *   complete modulus of the issuer public key.
 * - @ref EMV_TAG_9F32_ISSUER_PUBLIC_KEY_EXPONENT will be used to obtain the
 *   exponent of the issuer public key.
 * - @ref EMV_TAG_5A_APPLICATION_PAN will be used to validate the issuer
 *   identifier of the issuer public key.
 *
 * If sufficient fields are present to retrieve the full issuer public key,
 * then the hash field of the issuer public key certificate will be validated.
 * However, the certificate header, trailer, format, hash algorithm indicator
 * and public key algorithm indicator will always be validated and if incorrect
 * will result in an error.
 *
 * @param issuer_cert Issuer Public Key Certificate (field 90)
 * @param issuer_cert_len Length of Issuer Public Key Certificate in bytes
 * @param capk Certificate Authority Public Key (CAPK) to use
 * @param icc ICC data used during issuer public key retrieval and validation.
 *            NULL to ignore.
 * @param pkey Issuer public key output
 *
 * @return Zero if retrieved and validated.
 * @return Less than zero for error.
 * @return Greater than zero if decryption succeeded but full issuer public key
 *         retrieval or validation failed.
 */
int emv_rsa_retrieve_issuer_pkey(
	const uint8_t* issuer_cert,
	size_t issuer_cert_len,
	const struct emv_capk_t* capk,
	const struct emv_tlv_list_t* icc,
	struct emv_rsa_issuer_pkey_t* pkey
);

__END_DECLS

#endif
