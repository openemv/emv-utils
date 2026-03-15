/**
 * @file emv_config.h
 * @brief EMV configuration abstraction and helper functions
 *
 * Copyright 2026 Leon Lynch
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

#ifndef EMV_CONFIG_H
#define EMV_CONFIG_H

#include "emv_tlv.h"

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

// Forward declarations
struct emv_ctx_t;

/**
 * @brief EMV configuration
 *
 * Initialised as part of @ref emv_ctx_t by @ref emv_ctx_init() and similarly
 * cleared by @ref emv_ctx_clear(). Follow the instructions for each field.
 *
 * This configuration structure holds terminal resident data fields for
 * application independent data used during EMV processing.
 * However, this excludes:
 * - Transaction parameters that are unique for a specific transaction, which
 *   are provided by @ref emv_ctx_t.params instead.
 * - Certificate Authority Public Keys (CAPK), which are provided by
 *   @ref emv_capk_lookup() instead.
 *
 * Data fields should be retrieved by @ref emv_config_data_get().
 *
 * @remark See EMV 4.4 Book 4, 10
 */
struct emv_config_t {
	/**
	 * @brief Application independent data
	 * @remark See EMV 4.4 Book 4, 10.1
	 *
	 * Populate after @ref emv_ctx_init() and before EMV processing by
	 * using:
	 * - @ref emv_config_data_set()
	 * - @ref emv_config_data_set_asn1_object()
	 */
	struct emv_tlv_list_t data;
};

/**
 * Clear EMV configuration.
 *
 * This functions will clear dynamically allocated memory used by members of
 * the EMV configuration instance, but will not free the EMV configuration
 * instance itself.
 *
 * @param config EMV configuration
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_config_clear(struct emv_config_t* config);

/**
 * Set EMV TLV field for application independent data of EMV configuration.
 *
 * @param ctx EMV processing context
 * @param tag EMV TLV tag
 * @param length EMV TLV length
 * @param value EMV TLV value
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_config_data_set(
	struct emv_ctx_t* ctx,
	unsigned int tag,
	unsigned int length,
	const uint8_t* value
);

/**
 * Set encoded ASN.1 object for application independent data of EMV
 * configuration.
 *
 * @param ctx EMV processing context
 * @param oid Decoded object identifier (OID)
 * @param ber_length Length of remaining BER encoded subfields
 * @param ber_bytes Remaining BER encoded subfields
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_config_data_set_asn1_object(
	struct emv_ctx_t* ctx,
	const struct iso8825_oid_t* oid,
	unsigned int ber_length,
	const uint8_t* ber_bytes
);

/**
 * Retrieve EMV TLV field from EMV configuration.
 *
 * @param ctx EMV processing context
 * @param tag EMV tag to find
 *
 * @return EMV TLV field. Do NOT free. NULL if not found.
 */
const struct emv_tlv_t* emv_config_data_get(
	const struct emv_ctx_t* ctx,
	unsigned int tag
);

__END_DECLS

#endif
