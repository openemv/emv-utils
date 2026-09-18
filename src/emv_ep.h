/**
 * @file emv_ep.h
 * @brief High level EMV contactless Entry Point interface
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
struct emv_ctx_t;
struct emv_app_list_t;
struct emv_tlv_list_t;

/**
 * EMV contactless application combination
 */
struct emv_ep_app_t {
	/**
	 * @brief EMV application configuration for this combination
	 *
	 * Populated by @ref emv_ep_preprocess via @ref emv_ep_app_list_push().
	 */
	const struct emv_config_app_t* config;

	/**
	 * @brief Kernel Identifier - Terminal (field 96)
	 * Must be 8 bytes.
	 * @remark See EMV Contactless Book B v2.11, Table 3-7
	 *
	 * Populated by @ref emv_ep_preprocess via @ref emv_ep_app_list_push().
	 */
	uint8_t kernel_id[8];

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
 * @param config_app EMV application configuration
 * @param kernel_id Kernel Identifier - Terminal (field 96). Must be 8 bytes.
 *
 * @return Zero for success. Less than zero for error.
 */
int emv_ep_app_list_push(
	struct emv_ep_app_list_t* list,
	const struct emv_config_app_t* config_app,
	const uint8_t* kernel_id
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
 * Build contactless candidate application list using Proximity Payment System
 * Environment (PPSE) together with provided application combination
 * pre-processing list, and then sort according to Application
 * Priority Indicator. This function is intended for contactless cards, not
 * contact cards. Use @ref emv_build_candidate_list() for contact cards.
 * @remark See EMV Contactless Book B v2.11, 3.3
 *
 * @param ctx EMV processing context
 * @param ep_list EMV contactless application combination list
 * @param app_list Candidate application list output
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_ep_build_candidate_list(
	const struct emv_ctx_t* ctx,
	const struct emv_ep_app_list_t* ep_list,
	struct emv_app_list_t* app_list
);

/**
 * Select contactless EMV application by highest priority in candidate
 * application list. The candidate application list will be updated by removing
 * the selected application regardless of processing outcome. If application
 * selection fails this function will return @ref EMV_OUTCOME_TRY_AGAIN or
 * @ref EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD, depending on whether the
 * candidate application list is empty or not.
 * @remark See EMV 4.4 Book 1, 12.4
 * @remark See EMV 4.4 Book 4, 11.3
 * @remark See EMV Contactless Book B v2.11, 3.3.3
 *
 * @param ctx EMV processing context
 * @param app_list Candidate application list
 * @param kernel_id Selected kernel ID output. Must be 3 bytes.
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_ep_select_application(
	struct emv_ctx_t* ctx,
	struct emv_app_list_t* app_list,
	uint8_t* kernel_id
);

/**
 * Create EMV fields commonly used by contactless kernels and expected by
 * various EMV processing functions.
 * @remark See EMV Contactless Book B v2.11, 3.4
 *
 * @note This function is intended to be used by individual contactless kernel
 *       implementations and should only be used directly for use cases beyond
 *       EMV requirements.
 *
 * This function populates @ref emv_ctx_t.terminal with these fields:
 * - @ref EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL
 * - @ref EMV_TAG_9F39_POS_ENTRY_MODE
 * - @ref EMV_TAG_9F06_AID
 * - @ref EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION
 * - @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS
 * - @ref EMV_TAG_9F37_UNPREDICTABLE_NUMBER
 * - @ref EMV_TAG_9F66_TTQ (if configured)
 *
 * @param ctx EMV processing context
 * @param pos_entry_mode Point-of-Service (POS) Entry Mode (field 9F39) value.
 *                       See @ref pos-entry-mode-values "values".
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_ep_create_common_kernel_data(
	struct emv_ctx_t* ctx,
	uint8_t pos_entry_mode
);

/**
 * Initiate contactless kernel processing by assessing the Processing Options
 * Data Object List (PDOL), performing GET PROCESSING OPTIONS, and making the
 * response available to the calling contactless kernel.
 *
 * @note This function is intended to be used by individual contactless kernel
 *       implementations and should only be used directly for use cases beyond
 *       EMV requirements.
 *
 * @param ctx EMV processing context
 * @param gpo_list List to which decoded GPO response fields will be appended
 * @param gpo_tal_result GPO result provided by TAL
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_ep_initiate_kernel_processing(
	struct emv_ctx_t* ctx,
	struct emv_tlv_list_t* gpo_list,
	int* gpo_tal_result
);

__END_DECLS

#endif
