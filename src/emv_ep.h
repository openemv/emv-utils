/**
 * @file emv_ep.h
 * @brief EMV contactless Entry Point helper functions
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

#ifndef EMV_EP_H
#define EMV_EP_H

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdbool.h>

__BEGIN_DECLS

// Forward declarations
struct emv_config_app_t;
struct emv_config_t;
struct emv_app_t;

/**
 * EMV contactless application combination
 */
struct emv_ep_app_t {
	/**
	 * @brief Application Identifier (AID)
	 *
	 * Populated by @ref emv_ep_preprocess via @ref emv_ep_app_list_push().
	 */
	uint8_t aid[16];

	/**
	 * @brief Length of Application Identifier (AID) in bytes.
	 * Must be 5 - 16 bytes.
	 *
	 * Populated by @ref emv_ep_preprocess via @ref emv_ep_app_list_push().
	 */
	unsigned int aid_len;

	/**
	 * @brief Kernel Identifier - Terminal (field 96)
	 * Must be 8 bytes.
	 * @remark See EMV Contactless Book B v2.11, Table 3-7
	 *
	 * Populated by @ref emv_ep_preprocess via @ref emv_ep_app_list_push().
	 */
	uint8_t kernel_id[8];

	/**
	 * @brief EMV application configuration for this combination
	 *
	 * Populated by @ref emv_ep_preprocess via @ref emv_ep_app_list_push().
	 */
	const struct emv_config_app_t* config;

	/// Next application combination in list
	struct emv_ep_app_t* next;
};

/**
 * EMV contactless application combination list
 * @note Use the various @c emv_ep_app_list_*() functions to manipulate the list
 */
struct emv_ep_app_list_t {
	struct emv_ep_app_t* front;                 ///< Pointer to front of list
	struct emv_ep_app_t* back;                  ///< Pointer to end of list
};

/// Static initialiser for @ref emv_ep_app_list_t
#define EMV_EP_APP_LIST_INIT { NULL, NULL }

/**
 * Push EMV contactless application combination on to the back of a list
 *
 * @param list EMV contactless application combination list
 * @param aid Application Identifier (AID) field
 * @param aid_len Length of Application Identifier (AID) field in bytes
 * @param kernel_id Kernel Identifier - Terminal (field 96). Must be 8 bytes.
 * @param config_app EMV application configuration
 *
 * @return Zero for success. Less than zero for error.
 */
int emv_ep_app_list_push(
	struct emv_ep_app_list_t* list,
	const uint8_t* aid,
	unsigned int aid_len,
	const uint8_t* kernel_id,
	const struct emv_config_app_t* config_app
);

/**
 * Determine whether EMV contactless application combination list is empty
 *
 * @param list EMV contactless application combination list
 *
 * @return Boolean indicating whether EMV contactless application combination
 *         list is empty
 */
bool emv_ep_app_list_is_empty(const struct emv_ep_app_list_t* list);

/**
 * Clear EMV contactless application combination list
 * @note This function will call @ref emv_ep_app_free() for every element
 *
 * @param list EMV contactless application combination list
 */
void emv_ep_app_list_clear(struct emv_ep_app_list_t* list);

/**
 * Perform EMV contactless Entry Point pre-processing and create the list of
 * valid application combinations
 * @remark See EMV Contactless Book B v2.11, 3.1
 *
 * @param config EMV configuration containing supported applications
 * @param amount Current transaction amount
 * @param list EMV contactless application combination list output
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_ep_preprocess(
	const struct emv_config_t* config,
	uint32_t amount,
	struct emv_ep_app_list_t* list
);

/**
 * Find matching EMV application configuration for provided EMV application.
 *
 * This function will compare the provided EMV application to the supported
 * contactless application combinations according to their:
 * - Application Identifier (AID)
 * - @ref emv-asi-values "Application Selection Indicator (ASI)" flags
 * - Kernel Identifier
 *
 * @remark See EMV Contactless Book B v2.11, 3.3.2.5
 *
 * @param list EMV contactless application combination list
 * @param app EMV application
 *
 * @return Pointer to matching EMV application configuration. Do NOT free.
 *         NULL if no matching EMV application configuration found.
 */
const struct emv_config_app_t* emv_ep_find_supported_combination(
	const struct emv_ep_app_list_t* list,
	const struct emv_app_t* app
);

__END_DECLS

#endif
