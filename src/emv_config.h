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
#include <stdbool.h>
#include <stdint.h>

__BEGIN_DECLS

// Forward declarations
struct emv_ctx_t;
struct emv_app_t;

/**
 * @brief EMV application configuration
 */
struct emv_config_app_t {
	/**
	 * @brief Application Identifier (AID)
	 *
	 * Populated by @ref emv_config_app_create().
	 */
	uint8_t aid[16];

	/**
	 * @brief Length of Application Identifier (AID) in bytes.
	 * Must be 5 - 16 bytes.
	 *
	 * Populated by @ref emv_config_app_create().
	 */
	unsigned int aid_len;

	/**
	 * @brief Application Selection Indicator (ASI)
	 * @remark See EMV 4.4 Book 1, 12.3.1
	 *
	 * Populated by @ref emv_config_app_create().
	 *
	 * See @ref emv-asi-values "Application Selection Indicator (ASI)"
	 */
	uint8_t asi;

	/**
	 * @brief Application dependent data
	 * @remark See EMV 4.4 Book 4, 10.2
	 *
	 * Populated by @ref emv_config_app_create().
	 */
	struct emv_tlv_list_t data;

	/**
	 * @brief Target percentage to be used for random transaction selection
	 * during terminal risk management. Value must be 0 to 99. Set to zero to
	 * disable random transaction selection.
	 *
	 * Populate after @ref emv_config_app_create() and before EMV processing.
	 */
	unsigned int random_selection_percentage;

	/**
	 * @brief Maximum target percentage to be used for biased random
	 * transaction selection. Value must be 0 to 99 and must be greater than or
	 * equal to @ref emv_config_app_t.random_selection_percentage.
	 *
	 * Populate after @ref emv_config_app_create() and before EMV processing.
	 */
	unsigned int random_selection_max_percentage;

	/**
	 * @brief Threshold value for biased random transaction selection during
	 * terminal risk management. Value must be zero or a positive number less
	 * than the floor limit.
	 *
	 * Populate after @ref emv_config_app_create() and before EMV processing.
	 */
	unsigned int random_selection_threshold;

	/// Next EMV application configuration in list
	struct emv_config_app_t* next;
};

/**
 * EMV application configuration iterator
 * @note Do not modify the EMV configuration while using the iterator.
 */
struct emv_config_app_itr_t {
	/// @cond INTERNAL
	const struct emv_config_app_t* app;
	/// @endcond
};

/**
 * @brief EMV configuration
 *
 * Initialised as part of @ref emv_ctx_t by @ref emv_ctx_init() and similarly
 * cleared by @ref emv_ctx_clear(). Follow the instructions for each field.
 *
 * This configuration structure holds terminal resident data fields for both
 * application independent data and application dependent data used during EMV
 * processing. However, this excludes:
 * - Transaction parameters that are unique for a specific transaction, which
 *   are provided by @ref emv_ctx_t.params instead.
 * - Certificate Authority Public Keys (CAPK), which are provided by
 *   @ref emv_capk_lookup() instead.
 *
 * Data fields should be retrieved by @ref emv_config_data_get() which will
 * search the application dependent data fields before the application
 * independent data fields, thus allowing the latter to be set as defaults that
 * can optionally be overridden for specific applications.
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

	/**
	 * @brief Application dependent data
	 * @remark See EMV 4.4 Book 4, 10.2
	 *
	 * Populate after @ref emv_ctx_init() and before EMV processing using
	 * @ref emv_config_app_create()
	 */
	struct emv_config_app_t* supported_apps;
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
 * @ref emv_tlv_list_has_duplicate() and then replace the existing application
 * independent data of the EMV configuration with the provided EMV TLV list.
 * The provided EMV TLV list will be empty if the function succeeds.
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
 * Create a supported application for EMV configuration (enabled by default)
 *
 * The caller is responsible for creating EMV application configurations in the
 * appropriate order. They will be processed from first to last during
 * application discovery and therefore EMV application configurations for
 * exact matches should be created before EMV application configurations for
 * partial matches that may match the same Application Identifier (AID).
 *
 * This function will validate the provided EMV TLV list using
 * @ref emv_tlv_list_has_duplicate() and then move the data to the application
 * dependent data of the EMV application configuration. If not NULL, the
 * provided EMV TLV list will be empty if the function succeeds.
 *
 * @param ctx EMV processing context
 * @param aid Application Identifier (AID). Must be 5 - 16 bytes.
 * @param aid_len Length of Application Identifier (AID) in bytes.
 *                Must be 5 - 16 bytes.
 * @param asi Application Selection Indicator (ASI).
 *            See @ref emv-asi-values "Application Selection Indicator (ASI)"
 * @param data EMV TLV list containing application dependent data. If not NULL,
 *             this list will be empty if the function succeeds. NULL to
 *             ignore.
 * @param app EMV application configuration output. NULL to ignore.
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_config_app_create(
	struct emv_ctx_t* ctx,
	const void* aid,
	unsigned int aid_len,
	uint8_t asi,
	struct emv_tlv_list_t* data,
	struct emv_config_app_t** app
);

/**
 * Enable/disable supported application for EMV configuration
 *
 * This function allows an EMV application configuration to be disabled and
 * therefore not considered during application discovery.
 *
 * @param app EMV application configuration
 * @param enabled Boolean indicating whether EMV application configuration must
 *                be enabled
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_config_app_set_enable(struct emv_config_app_t* app, bool enabled);

/**
 * Initialise EMV application configuration iterator for supported applications
 *
 * @param config EMV configuration
 * @param itr EMV application configuration iterator
 *
 * @return Zero for success. Less than zero for internal error.
 */
int emv_config_app_itr_init(
	const struct emv_config_t* config,
	struct emv_config_app_itr_t* itr
);

/**
 * Retrieve next supported EMV application configuration.
 *
 * This function will skip EMV application configurations when their
 * @ref emv-asi-values "Application Selection Indicator (ASI)" flags indicate
 * that they are disabled.
 *
 * @return Pointer to EMV application configuration. Do NOT free.
 *         NULL for end of list.
 */
const struct emv_config_app_t* emv_config_app_itr_next(
	struct emv_config_app_itr_t* itr
);

/**
 * Find matching EMV application configuration for provided EMV application.
 *
 * This function will compare the provided EMV application to the supported
 * applications of the EMV configuration according to their individual
 * @ref emv-asi-values "Application Selection Indicator (ASI)" flags.
 *
 * @param config EMV configuration
 * @param app EMV application
 *
 * @return Pointer to matching EMV application configuration. Do NOT free.
 *         NULL if no matching EMV application configuration found.
 */
const struct emv_config_app_t* emv_config_app_find_supported(
	const struct emv_config_t* config,
	const struct emv_app_t* app
);

/**
 * Retrieve EMV TLV field from EMV configuration.
 *
 * This function will search the application dependent data of the selected
 * application before searching the application independent data, thus allowing
 * the latter to be set as defaults that can optionally be overridden for
 * specific applications.
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
