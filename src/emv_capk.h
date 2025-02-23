/**
 * @file emv_capk.h
 * @brief EMV Certificate Authority Public Key (CAPK) helper functions
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

#ifndef EMV_CAPK_H
#define EMV_CAPK_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

#define EMV_CAPK_RID_LEN (5) ///< Length of Registered Application Provider Identifier (RID) in bytes

/**
 * @name Certificate Authority Public Key (CAPK) hash algorithms
 * @remark See EMV 4.4 Book 2, Annex B2.3, Table 47
 * @anchor capk-hash-values
 */
/// @{
#define EMV_CAPK_HASH_SHA1      (0x01) ///< SHA-1
#define EMV_CAPK_HASH_SHA256    (0x02) ///< SHA-256
#define EMV_CAPK_HASH_SHA512    (0x03) ///< SHA-512
#define EMV_CAPK_HASH_SHA3_256  (0x04) ///< SHA-3 256
#define EMV_CAPK_HASH_SHA3_512  (0x05) ///< SHA-3 512
#define EMV_CAPK_HASH_SM3       (0x80) ///< SM3
/// @}

/**
 * Certificate Authority Public Key (CAPK)
 * @remark See EMV 4.4 Book 2, 11.2.2, Table 30
 * @remark See EMV 4.4 Book 2, 11.2.2, Table 31
 */
struct emv_capk_t {
	const uint8_t* rid; ///< Registered Application Provider Identifier (RID). Must be 5 bytes. See @ref EMV_CAPK_RID_LEN.
	uint8_t index; ///< CAPK index
	uint8_t hash_id; ///< Hash algorithm indicator. See @ref capk-hash-values "Certificate Authority Public Key (CAPK) hash algorithms"
	const void* modulus; ///< CAPK modulus
	size_t modulus_len; ///< Length of CAPK modulus in bytes
	const void* exponent; ///< CAPK exponent
	size_t exponent_len; ///< Length of CAPK exponent in bytes
	const void* hash; ///< CAPK hash of RID, index, modulus and exponent
	size_t hash_len; ///< Length of CAPK hash in bytes
};

/// Certificate Authority Public Key (CAPK) iterator
struct emv_capk_itr_t {
	unsigned int idx; ///<< Current list index
};

/**
 * Lookup Certificate Authority Public Key (CAPK)
 *
 * @param rid Registered Application Provider Identifier (RID). Must be 5 bytes.
 * @param index CAPK index
 * @return Pointer to Certificate Authority Public Key (CAPK). Do NOT free. NULL if not found.
 */
const struct emv_capk_t* emv_capk_lookup(const uint8_t* rid, uint8_t index);

/**
 * Initialise Certificate Authority Public Key (CAPK) iterator
 *
 * @param itr Certificate Authority Public Key (CAPK) iterator output
 * @return Zero for success. Non-zero for error.
 */
int emv_capk_itr_init(struct emv_capk_itr_t* itr);

/**
 * Retrieve next Certificate Authority Public Key (CAPK) and advance iterator
 * @param itr Certificate Authority Public Key (CAPK) iterator
 * @return Pointer to Certificate Authority Public Key (CAPK). Do NOT free. NULL for end of list.
 */
const struct emv_capk_t* emv_capk_itr_next(struct emv_capk_itr_t* itr);

__END_DECLS

#endif
