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

#include "emv_oda_types.h"

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

// Forward declarations
struct emv_capk_t;
struct emv_tlv_list_t;

/**
 * @name EMV RSA formats
 * @remark See EMV Book 4.4 Book 2
 * @anchor emv-rsa-format-values
 */
/// @{
#define EMV_RSA_FORMAT_ISSUER_CERT              (0x02) ///< Issuer Public Key Certificate format
#define EMV_RSA_FORMAT_SSAD                     (0x03) ///< Signed Static Application Data format
#define EMV_RSA_FORMAT_ICC_CERT                 (0x04) ///< ICC Public Key Certificate format
#define EMV_RSA_FORMAT_SDAD                     (0x05) ///< Signed Dynamic Application Data format
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
 * Retrieved Signed Static Application Data
 * @remark See EMV 4.4 Book 2, 5.4, Table 7
 */
struct emv_rsa_ssad_t {
	uint8_t format; ///< Signed Data Format. Must be @ref EMV_RSA_FORMAT_SSAD.
	uint8_t hash_id; ///< Hash algorithm indicator. Must be @ref EMV_PKEY_HASH_SHA1.
	uint8_t data_auth_code[2]; ///< Data authentication code
	uint8_t hash[20]; ///< Hash used for Static Data Authentication (SDA)
};

/**
 * Retrieved Signed Dynamic Application Data
 * @remark See EMV 4.4 Book 2, 6.5.2, Table 17
 * @remark See EMV 4.4 Book 2, 6.6.1, Table 19
 * @remark See EMV 4.4 Book 2, 6.6.2, Table 22
 */
struct emv_rsa_sdad_t {
	uint8_t format; ///< Signed Data Format. Must be @ref EMV_RSA_FORMAT_SDAD.
	uint8_t hash_id; ///< Hash algorithm indicator. Must be @ref EMV_PKEY_HASH_SHA1.
	uint8_t icc_dynamic_number_len; ///< Length of ICC Dynamic Number (field 9F4C) in bytes
	uint8_t icc_dynamic_number[8]; ///< Value of ICC Dynamic Number (field 9F4C)
	uint8_t cid; ///< Value of Cryptogram Information Data (field 9F27)
	uint8_t cryptogram[8]; ///< Value of Application Cryptogram (field 9F26)
	uint8_t txn_data_hash_code[20]; ///< Transaction Data Hash Code
	uint8_t hash[20]; ///< Hash used for Signed Dynamic Application Data validation
};

/**
 * Retrieve issuer public key and optionally validate certificate hash.
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
 * The following optional fields can be provided in @p params and contribute to
 * issuer public key validation:
 * - @ref EMV_TAG_9A_TRANSACTION_DATE will be used to validate the certificate
 *   expiration date of the issuer public key.
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
 * @param params Transaction parameters used during issuer public key
 *        validation. NULL to ignore.
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
	const struct emv_tlv_list_t* params,
	struct emv_rsa_issuer_pkey_t* pkey
);

/**
 * Retrieve Signed Static Application Data (SSAD) and optionally validate the
 * hash.
 * @remark See EMV 4.4 Book 2, 5.4
 *
 * This function will perform decryption and validate the data header, trailer,
 * format and hash algorithm indicator to confirm that decryption succeeded
 * with the appropriate issuer public key. If the initialised and populated
 * Offline Data Authentication (ODA) context is provided, this function will
 * also validate the hash.
 *
 * @param ssad Signed Static Application Data (field 93)
 * @param ssad_len Length of Signed Static Application Data in bytes
 * @param issuer_pkey Issuer public key
 * @param oda Offline Data Authentication (ODA) context used during hash
 *            validation. NULL to ignore.
 * @param data Retrieved Signed Static Application Data output
 *
 * @return Zero if retrieved and validated.
 * @return Less than zero for error.
 * @return Greater than zero if decryption succeeded but hash validation was
 *         not possible or failed.
 */
int emv_rsa_retrieve_ssad(
	const uint8_t* ssad,
	size_t ssad_len,
	const struct emv_rsa_issuer_pkey_t* issuer_pkey,
	const struct emv_oda_ctx_t* oda,
	struct emv_rsa_ssad_t* data
);

/**
 * Retrieve ICC public key and optionally validate certificate hash.
 * @remark See EMV 4.4 Book 2, 6.4
 *
 * The following optional fields can be provided in @p icc and contribute to
 * ICC public key retrieval:
 * - @ref EMV_TAG_9F48_ICC_PUBLIC_KEY_REMAINDER will be used to obtain the
 *   complete modulus of the ICC public key.
 * - @ref EMV_TAG_9F47_ICC_PUBLIC_KEY_EXPONENT will be used to obtain the
 *   exponent of the ICC public key.
 * - @ref EMV_TAG_5A_APPLICATION_PAN will be used to validate the application
 *   PAN of the ICC public key.
 *
 * The following optional fields can be provided in @p params and contribute to
 * ICC public key validation:
 * - @ref EMV_TAG_9A_TRANSACTION_DATE will be used to validate the certificate
 *   expiration date of the ICC public key.
 *
 * This function will perform decryption and validate the data header, trailer,
 * format, hash algorithm indicator and public key algorithm indicator to
 * confirm that decryption succeeded with the appropriate issuer public key.
 * If sufficient ICC fields are present then the full ICC public key will be
 * retrieved. If the full ICC public has been retrieved, and the initialised
 * and populated Offline Data Authentication (ODA) context is provided, this
 * function will also validate the hash.
 *
 * @param icc_cert ICC Public Key Certificate (field 9F46)
 * @param icc_cert_len Length ICC Public Key Certificate in bytes
 * @param issuer_pkey Issuer public key
 * @param icc ICC data used during ICC public key retrieval and validation.
 *            NULL to ignore.
 * @param params Transaction parameters used during ICC public key validation.
 *               NULL to ignore.
 * @param oda Offline Data Authentication (ODA) context used during hash
 *            validation. NULL to ignore.
 * @param pkey ICC public key output
 *
 * @return Zero if retrieved and validated.
 * @return Less than zero for error.
 * @return Greater than zero if decryption succeeded but full ICC public key
 *         retrieval or hash validation not possible or failed.
 */
int emv_rsa_retrieve_icc_pkey(
	const uint8_t* icc_cert,
	size_t icc_cert_len,
	const struct emv_rsa_issuer_pkey_t* issuer_pkey,
	const struct emv_tlv_list_t* icc,
	const struct emv_tlv_list_t* params,
	const struct emv_oda_ctx_t* oda,
	struct emv_rsa_icc_pkey_t* pkey
);

/**
 * Retrieve Signed Dynamic Application Data (SDAD) and optionally validate the
 * hash.
 * @remark See EMV 4.4 Book 2, 6.5.2
 * @remark See EMV 4.4 Book 2, 6.6.2
 *
 * This function will perform decryption and validate the data header, trailer,
 * format and hash algorithm indicator to confirm that decryption succeeded
 * with the appropriate ICC public key. If the concatenated data according to
 * the Dynamic Data Authentication Data Object List (DDOL) is provided, this
 * function will also validate the hash.
 *
 * @param sdad Signed Dynamic Application Data (field 9F4B)
 * @param sdad_len Length of Signed Dynamic Application Data in bytes
 * @param icc_pkey ICC public key
 * @param ddol_data Concatenated data according to Dynamic Data Authentication
 *                  Data Object List (DDOL)
 * @param ddol_data_len Length of concatenated DDOL data in bytes
 * @param data Retrieved Signed Dynamic Application Data output
 *
 * @return Zero if retrieved and validated.
 * @return Less than zero for error.
 * @return Greater than zero if decryption succeeded but hash validation was
 *         not possible or failed.
 */
int emv_rsa_retrieve_sdad(
	const uint8_t* sdad,
	size_t sdad_len,
	const struct emv_rsa_icc_pkey_t* icc_pkey,
	const void* ddol_data,
	size_t ddol_data_len,
	struct emv_rsa_sdad_t* data
);

__END_DECLS

#endif
