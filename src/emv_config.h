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
	 * Populate after @ref emv_ctx_init() and before EMV processing using
	 * @ref emv_config_data_set().
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
 * Set EMV TLV list for application independent data of EMV configuration.
 *
 * This function will validate the provided EMV TLV list using
 * @ref emv_tlv_list_is_empty() and @ref emv_tlv_list_has_duplicate(), and then
 * replace the existing application independent data of the EMV configuration
 * with the provided EMV TLV list. The provided EMV TLV list will be empty if
 * the function succeeds.
 *
 * @param ctx EMV processing context
 * @param data EMV TLV list containing application independent data. This list
 *             will be empty if the function succeeds.
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_config_data_set(
	struct emv_ctx_t* ctx,
	struct emv_tlv_list_t* data
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
