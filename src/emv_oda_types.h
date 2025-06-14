/**
 * @file emv_oda_types.h
 * @brief EMV Offline Data Authentication (ODA) types used by high level EMV
 *        library interface
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

#ifndef EMV_ODA_TYPES_H
#define EMV_ODA_TYPES_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

/**
 * EMV Offline Data Authentication (ODA) method
 */
enum emv_oda_method_t {
	EMV_ODA_METHOD_NONE = 0,
	EMV_ODA_METHOD_SDA, ///< Static Data Authentication (SDA)
	EMV_ODA_METHOD_DDA, ///< Dynamic Data Authentication (DDA)
	EMV_ODA_METHOD_CDA, ///< Combined DDA/Application Cryptogram Generation (CDA)
	EMV_ODA_METHOD_XDA, ///< Extended Data Authentication (XDA)
};

/**
 * ICC public key
 * @remark See EMV 4.4 Book 2, 6.4, Table 14
 *
 * This structure is intended to represent the complete and validated Issuer
 * Public Key created from the combination of these fields:
 * - @ref EMV_TAG_9F46_ICC_PUBLIC_KEY_CERTIFICATE
 * - @ref EMV_TAG_9F48_ICC_PUBLIC_KEY_REMAINDER
 * - @ref EMV_TAG_9F47_ICC_PUBLIC_KEY_EXPONENT
 */
struct emv_rsa_icc_pkey_t {
	uint8_t format; ///< Certificate Format. Must be @ref EMV_RSA_FORMAT_ICC_CERT.
	uint8_t pan[10]; ///< Application PAN (padded to the right with hex 'F's).
	uint8_t cert_exp[2]; ///< Certificate Expiration Date (MMYY)
	uint8_t cert_sn[3]; ///< Binary number unique to this certificate
	uint8_t hash_id; ///< Hash algorithm indicator. Must be @ref EMV_PKEY_HASH_SHA1.
	uint8_t alg_id; ///< Public key algorithm indicator. Must be @ref EMV_PKEY_SIG_RSA_SHA1.
	uint8_t modulus_len; ///< Public key modulus length in bytes
	uint8_t exponent_len; ///< Public key exponent length in bytes
	uint8_t modulus[1984 / 8]; ///< Public key modulus
	uint8_t exponent[3]; ///< Public key exponent
	uint8_t hash[20]; ///< Hash used for ICC public key validation
};

/**
 * EMV Offline Data Authentication (ODA) context
 */
struct emv_oda_ctx_t {
	uint8_t* record_buf; ///< Application record buffer
	unsigned int record_buf_len; ///< Length of application record buffer

	/**
	 * Cached Processing Options Data Object List (PDOL) data for validating
	 * Transaction Data Hash Code. PDOL data has a maximum length of
	 * @ref EMV_CAPDU_DATA_MAX minus 3 bytes to allow for tag 83 and its length
	 * in the GPO data.
	 */
	uint8_t pdol_data[255 - 3];
	size_t pdol_data_len; ///< Length of cached PDOL data in bytes

	/**
	 * Cached Card Risk Management Data Object List 1 (CDOL1) data for
	 * validating Transaction Data Hash Code. CDOL1 data has a maximum length
	 * of @ref EMV_CAPDU_DATA_MAX.
	 */
	uint8_t cdol1_data[255];
	size_t cdol1_data_len; ///< Length of cached CDOL1 data in bytes

	/**
	 * Cached GENAC response excluding Signed Dynamic Application Data (SDAD)
	 * for validating Transaction Data Hash Code. GENAC response has a maximum
	 * length of @ref EMV_RAPDU_DATA_MAX minus minimum length of 512-bit SDAD.
	 */
	uint8_t genac_data[256 - 64];
	size_t genac_data_len; ///< Length of cached GENAC response data in bytes

	/// Currently selected Offline Data Authentication (ODA) method
	enum emv_oda_method_t method;

	/**
	 * Currently retrieved ICC public key for use during processing of
	 * Combined DDA/Application Cryptogram Generation (CDA)
	 */
	struct emv_rsa_icc_pkey_t icc_pkey;
};

__END_DECLS

#endif
