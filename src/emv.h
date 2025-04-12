/**
 * @file emv.h
 * @brief High level EMV library interface
 *
 * Copyright 2023-2025 Leon Lynch
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

#ifndef EMV_H
#define EMV_H

#include "emv_tlv.h"
#include "emv_oda.h"

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

// Forward declarations
struct emv_ttl_t;
struct emv_app_list_t;
struct emv_app_t;

/**
 * @brief EMV processing context
 *
 * Initialise using @ref emv_ctx_init() and then follow the instructions for
 * populating remaining fields.
 *
 * Reset using @ref emv_ctx_reset() to prepare for next transaction using
 * the same configuration.
 *
 * Clear using @ref emv_ctx_clear().
 */
struct emv_ctx_t {
	/**
	 * @brief Terminal Transport Layer (TTL) context
	 *
	 * Populated by @ref emv_ctx_init().
	 */
	struct emv_ttl_t* ttl;

	/**
	 * @brief Terminal configuration.
	 *
	 * Populate after @ref emv_ctx_init() and before EMV processing by
	 * using @ref emv_tlv_list_push().
	 */
	struct emv_tlv_list_t config;

	/**
	 * @brief List of supported applications.
	 *
	 * Populate after @ref emv_ctx_init() and before EMV processing
	 * using @ref emv_tlv_list_push(). Each entry is a @ref EMV_TAG_9F06_AID
	 * field containing a supported Application Identifier (AID) with
	 * @ref emv_tlv_t.flags set to either @ref EMV_ASI_EXACT_MATCH or
	 * @ref EMV_ASI_PARTIAL_MATCH.
	 *
	 */
	struct emv_tlv_list_t supported_aids;

	/**
	 * @brief Parameters for current transaction.
	 *
	 * Populate after @ref emv_ctx_init() and before EMV processing by
	 * using @ref emv_tlv_list_push().
	 *
	 * The minimum required fields for transaction processing are:
	 * - @ref EMV_TAG_9F41_TRANSACTION_SEQUENCE_COUNTER
	 * - @ref EMV_TAG_9A_TRANSACTION_DATE
	 * - @ref EMV_TAG_9F21_TRANSACTION_TIME
	 * - @ref EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE
	 * - @ref EMV_TAG_5F36_TRANSACTION_CURRENCY_EXPONENT
	 * - @ref EMV_TAG_9C_TRANSACTION_TYPE
	 * - @ref EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC
	 * - @ref EMV_TAG_81_AMOUNT_AUTHORISED_BINARY
	 *
	 * Optional fields are:
	 * - @ref EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC
	 * - @ref EMV_TAG_9F04_AMOUNT_OTHER_BINARY
	 */
	struct emv_tlv_list_t params;

	/**
	 * @brief Currently selected application
	 *
	 * Populated by @ref emv_select_application() and used by
	 * @ref emv_initiate_application_processing().
	 */
	struct emv_app_t* selected_app;

	/**
	 * @brief Integrated Circuit Card (ICC) data for current application.
	 *
	 * Populated and used by:
	 * - @ref emv_initiate_application_processing()
	 * - @ref emv_read_application_data()
	 * - @ref emv_offline_data_authentication()
	 */
	struct emv_tlv_list_t icc;

	/**
	 * @brief Terminal data for current transaction.
	 *
	 * Populated and used by:
	 * - @ref emv_initiate_application_processing()
	 * - @ref emv_offline_data_authentication()
	 */
	struct emv_tlv_list_t terminal;

	/**
	 * @brief Offline Data Authentication (ODA) context.
	 *
	 * Populated and used by:
	 * - @ref emv_read_application_data()
	 * - @ref emv_offline_data_authentication()
	 */
	struct emv_oda_ctx_t oda;

	/**
	 * @brief Various cached fields for internal use
	 *
	 * Populated by @ref emv_initiate_application_processing() and used by
	 * various functions.
	 * @cond INTERNAL
	 */
	const struct emv_tlv_t* aid;
	const struct emv_tlv_t* tvr;
	const struct emv_tlv_t* tsi;
	const struct emv_tlv_t* aip;
	const struct emv_tlv_t* afl;
	/// @endcond
};

/**
 * EMV errors
 * These are typically for internal errors and errors caused by invalid use of
 * the API functions in this header, and must have values less than zero.
 */
enum emv_error_t {
	EMV_ERROR_INTERNAL = -1, ///< Internal error
	EMV_ERROR_INVALID_PARAMETER = -2, ///< Invalid function parameter
};

/**
 * EMV processing outcomes
 * These indicate the EMV processing outcome, if any, and must have values
 * greater than zero.
 */
enum emv_outcome_t {
	EMV_OUTCOME_CARD_ERROR = 1, ///< Malfunction of the card or non-conformance to Answer To Reset (ATR)
	EMV_OUTCOME_CARD_BLOCKED = 2, ///< Card blocked
	EMV_OUTCOME_NOT_ACCEPTED = 3, ///< Card not accepted or no supported applications
	EMV_OUTCOME_TRY_AGAIN = 4, ///< Try again by selecting a different application
	EMV_OUTCOME_GPO_NOT_ACCEPTED = 5, ///< Processing conditions not accepted
};

/**
 * Retrieve EMV library version string
 * @return Pointer to null-terminated string. Do not free.
 */
const char* emv_lib_version_string(void);

/**
 * Initialize EMV processing context
 *
 * @param ctx EMV processing context
 * @param ttl Terminal Transport Layer (TTL) context
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_ctx_init(struct emv_ctx_t* ctx, struct emv_ttl_t* ttl);

/**
 * Reset EMV processing context for next transaction.
 *
 * This function will clear these transaction specific context members:
 * - @ref emv_ctx_t.params
 * - @ref emv_ctx_t.icc
 * - @ref emv_ctx_t.terminal
 *
 * And this function will preserve these members that can be reused for the
 * next transaction:
 * - @ref emv_ctx_t.ttl
 * - @ref emv_ctx_t.config
 * - @ref emv_ctx_t.supported_aids
 *
 * @param ctx EMV processing context
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_ctx_reset(struct emv_ctx_t* ctx);

/**
 * Clear EMV processing context
 *
 * This functions will clear dynamically allocated memory used by members of
 * the EMV processing context, but will not free the EMV processing context
 * structure itself.
 *
 * @param ctx EMV processing context
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_ctx_clear(struct emv_ctx_t* ctx);

/**
 * Retrieve string associated with error value
 * @param error Error value. Must be less than zero.
 * @return Pointer to null-terminated string. Do not free.
 */
const char* emv_error_get_string(enum emv_error_t error);

/**
 * Retrieve string associated with outcome value
 * @remark See EMV 4.4 Book 4, 11.2
 * @remark See EMV Contactless Book A v2.10, 9.4
 *
 * @param outcome Outcome value. Must be greater than zero.
 * @return Pointer to null-terminated string. Do not free.
 */
const char* emv_outcome_get_string(enum emv_outcome_t outcome);

/**
 * Parse the ISO 7816 Answer To Reset (ATR) message and determine whether the
 * card is suitable for EMV processing
 * @remark See EMV Level 1 Contact Interface Specification v1.0, 8.3
 *
 * @param atr ATR data
 * @param atr_len Length of ATR data
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_atr_parse(const void* atr, size_t atr_len);

/**
 * Build candidate application list using Payment System Environment (PSE) or
 * discovery of supported AIDs, and then sort according to Application Priority
 * Indicator
 * @remark See EMV 4.4 Book 1, 12.3
 *
 * @param ctx EMV processing context
 * @param app_list Candidate application list output
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_build_candidate_list(
	const struct emv_ctx_t* ctx,
	struct emv_app_list_t* app_list
);

/**
 * Select EMV application by index from the candidate application list. The
 * candidate application list will be updated by removing the selected
 * application regardless of processing outcome. If application selection fails
 * this function will return either @ref EMV_OUTCOME_NOT_ACCEPTED or
 * @ref EMV_OUTCOME_TRY_AGAIN, depending on whether the candidate application
 * list is empty or not.
 * @remark See EMV 4.4 Book 1, 12.4
 * @remark See EMV 4.4 Book 4, 11.3
 *
 * @param ctx EMV processing context
 * @param app_list Candidate application list
 * @param index Index (starting from zero) of EMV application to select
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_select_application(
	struct emv_ctx_t* ctx,
	struct emv_app_list_t* app_list,
	unsigned int index
);

/**
 * Initiate EMV application processing by assessing the Processing Options
 * Data Object List (PDOL) and performing GET PROCESSING OPTIONS.
 *
 * When building the PDOL data required for GET PROCESSING OPTIONS, this
 * function will search the TLV lists in this order:
 * - @ref emv_ctx_t.params
 * - @ref emv_ctx_t.config
 * - @ref emv_ctx_t.terminal
 *
 * @note This function clears @ref emv_ctx_t.icc and @ref emv_ctx_t.terminal
 *       and then populates them appropriately. Upon success, the selected
 *       application's TLV data will be moved to @ref emv_ctx_t.icc and the
 *       output of GET PROCESSING OPTIONS will be appended. Upon success,
 *       @ref emv_ctx_t.terminal will be populated with various fields,
 *       including @ref EMV_TAG_9F39_POS_ENTRY_MODE and @ref EMV_TAG_9F06_AID.
 *
 * @remark See EMV 4.4 Book 3, 10.1
 * @remark See EMV 4.4 Book 4, 6.3.1
 *
 * @param ctx EMV processing context
 * @param pos_entry_mode Point-of-Service (POS) Entry Mode (field 9F39) value.
 *                       See @ref pos-entry-mode-values "values".
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_initiate_application_processing(
	struct emv_ctx_t* ctx,
	uint8_t pos_entry_mode
);

/**
 * Read EMV application data by performing READ RECORD for all records
 * specified by the Application File Locator (AFL), checking that there are no
 * redundant TLV fields provided by the application records, and checking for
 * the mandatory fields.
 *
 * While reading the application records, this function will also concatenate
 * the data required for Offline Data Authentication (ODA) and update
 * @ref emv_ctx_t.oda accordingly.
 *
 * @note Upon success, this function will append the application data to
 *       @ref emv_ctx_t.icc
 *
 * @remark See EMV 4.4 Book 3, 10.2
 *
 * @param ctx EMV processing context
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_read_application_data(struct emv_ctx_t* ctx);

/**
 * Perform Offline Data Authentication (ODA) by selecting and applying the
 * appropriate ODA method.
 *
 * The ODA method is selected based on card support indicated by
 * @ref EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE and terminal support
 * indicated by @ref EMV_TAG_9F33_TERMINAL_CAPABILITIES. This function will
 * update @ref EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION based on the selected
 * method and update @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS based on the
 * outcome.
 *
 * @remark See EMV 4.4 Book 3, 10.3
 *
 * @param ctx EMV processing context
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_offline_data_authentication(struct emv_ctx_t* ctx);

__END_DECLS

#endif
