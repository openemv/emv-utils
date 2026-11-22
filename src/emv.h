/**
 * @file emv.h
 * @brief High level EMV library interface
 *
 * Copyright 2023-2026 Leon Lynch
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

#include "emv_config.h"
#include "emv_tlv.h"
#include "emv_oda_types.h"
#include "emv_cvm_types.h"

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
	 * @brief Terminal Transport Layer (TTL) context.
	 *
	 * Populated by @ref emv_ctx_init() or @ref emv_card_activated() before
	 * EMV processing.
	 */
	struct emv_ttl_t* ttl;

	/**
	 * @brief Terminal configuration data.
	 * @remark See EMV 4.4 Book 4, 10
	 *
	 * Populate after @ref emv_ctx_init() and before EMV processing by
	 * using:
	 * - @ref emv_config_data_set()
	 * - @ref emv_config_app_create()
	 *
	 * Mandatory fields are validated by @ref emv_ctx_prepare_txn().
	 */
	struct emv_config_t config;

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
	 * These fields are required for cashback transactions:
	 * - @ref EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC
	 * - @ref EMV_TAG_9F04_AMOUNT_OTHER_BINARY
	 *
	 * Mandatory fields are validated by @ref emv_ctx_prepare_txn().
	 */
	struct emv_tlv_list_t params;

	/**
	 * @brief Currently selected application.
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
	 * - @ref emv_cardholder_verification()
	 * - @ref emv_online_processing()
	 * - @ref emv_completion()
	 */
	struct emv_tlv_list_t terminal;

	/**
	 * @brief Completion data for the current transaction.
	 *
	 * Populated by @ref emv_completion().
	 */
	struct emv_tlv_list_t completion;

	/**
	 * @brief Offline Data Authentication (ODA) context.
	 *
	 * Populated and used by:
	 * - @ref emv_initiate_application_processing()
	 * - @ref emv_read_application_data()
	 * - @ref emv_offline_data_authentication()
	 */
	struct emv_oda_ctx_t oda;

	/**
	 * @brief Cardholder Verification Method (CVM) context.
	 *
	 * Populated and used by @ref emv_cardholder_verification().
	 */
	struct emv_cvm_ctx_t cvm;

	/// @cond INTERNAL
	/**
	 * @brief Various cached fields for internal use
	 *
	 * - Transaction amount populated by @ref emv_ctx_prepare_txn().
	 * - Kernel ID populated by individual contactless kernel interfaces
	 * - Remaining fields populated by @ref emv_initiate_application_processing()
	 */
	uint32_t txn_amount;
	const struct emv_tlv_t* kernel_id;
	const struct emv_tlv_t* aid;
	const struct emv_tlv_t* tvr;
	const struct emv_tlv_t* tsi;
	const struct emv_tlv_t* aip;
	const struct emv_tlv_t* afl;
	/// @endcond
};

/**
 * @brief EMV transaction log entry
 */
struct emv_txn_log_entry_t {
	uint8_t pan[10]; ///< Primary Account Number (PAN) in EMV format 'cn'
	uint8_t pan_seq; ///< Primary Account Number (PAN) Sequence Number
	uint8_t txn_date[3]; ///< Transaction date in EMV format 'n' as YYMMDD
	uint32_t txn_amount; ///< Transaction amount in binary format
};

/**
 * @brief EMV errors
 *
 * These are typically for internal errors and errors caused by invalid use of
 * the API functions in this header, and must have values less than zero.
 */
enum emv_error_t {
	EMV_ERROR_INTERNAL = -1, ///< Internal error
	EMV_ERROR_INVALID_PARAMETER = -2, ///< Invalid function parameter
	EMV_ERROR_INVALID_CONFIG = -3, ///< Invalid configuration
};

/**
 * @brief EMV processing outcomes
 *
 * These indicate the EMV processing outcome, if any, and must have values
 * greater than zero.
 */
enum emv_outcome_t {
	// Generic EMV outcomes
	EMV_OUTCOME_APPROVED = 1, ///< Transaction is approved
	EMV_OUTCOME_DECLINED = 2, ///< Transaction is declined
	EMV_OUTCOME_CARD_ERROR = 3, ///< Malfunction of the card or non-conformance to Answer To Reset (ATR)
	EMV_OUTCOME_CARD_BLOCKED = 4, ///< Card blocked
	EMV_OUTCOME_NOT_ACCEPTED = 5, ///< Card not accepted or no supported applications
	EMV_OUTCOME_TRY_AGAIN = 6, ///< Try again by selecting a different application
	EMV_OUTCOME_GPO_NOT_ACCEPTED = 7, ///< Processing conditions not accepted

	// Contactless outcomes
	// See EMV Contactless Book A v2.12, Annex B
	EMV_OUTCOME_TRY_ANOTHER_INTERFACE = 20, ///< Contactless not possible; insert or swipe card
	EMV_OUTCOME_TRY_AGAIN_SEE_PHONE, ///< Contactless field off request; see phone for instructions
	EMV_OUTCOME_SELECT_NEXT, ///< Select next contactless application
	EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD, ///< Contactless not possible; insert, swipe or try another card
	EMV_OUTCOME_END_APPLICATION_RESTART, ///< Restart contactless; present card again
};

/**
 * Retrieve EMV library version string
 * @return Pointer to null-terminated string. Do not free.
 */
const char* emv_lib_version_string(void);

/**
 * Initialise EMV processing context
 *
 * @param ctx EMV processing context
 * @param ttl Terminal Transport Layer (TTL) context. NULL if not available.
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
 * Prepare EMV processing context for current transaction.
 *
 * This function will ensure that mandatory configuration fields and mandatory
 * transaction parameters are populated and have valid length. It also decodes,
 * validates and caches the transaction amount for other EMV processing
 * functions to use.
 *
 * @note This function must be called for every transaction after
 *       @ref emv_ctx_t.config and @ref emv_ctx_t.params have been populated
 *       and before EMV processing.
 *
 * @param ctx EMV processing context
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_ctx_prepare_txn(struct emv_ctx_t* ctx);

/**
 * Retrieve string associated with error value
 * @param error Error value. Must be less than zero.
 * @return Pointer to null-terminated string. Do not free.
 */
const char* emv_error_get_string(enum emv_error_t error);

/**
 * Retrieve string associated with outcome value
 * @remark See EMV 4.4 Book 4, 11.2
 * @remark See EMV Contactless Book A v2.12, 9.4
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
 * Parse the ISO 14443 Answer To Select (ATS) message and determine whether the
 * contactless type A card is suitable for EMV processing
 * @remark See EMV Level 1 Contactless Interface Specification v4.0, 5.7.2
 *
 * @param ats ATS data (starting with mandatory TL byte)
 * @param ats_len Length of ATS data in bytes
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_ats_parse(const void* ats, size_t ats_len);

/**
 * Parse the ISO 14443 Answer To Request Type B (ATQB) message and determine
 * whether the contactless type B card is suitable for EMV processing
 * @remark See EMV Level 1 Contactless Interface Specification v4.0, 6.3.2
 *
 * @param atqb ATQB data (starting with mandatory 0x50 marker byte)
 * @param atqb_len Length of ATQB data in bytes (12 for basic, 13 for extended)
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_atqb_parse(const void* atqb, size_t atqb_len);

/**
 * Indicate that EMV card has been presented to reader
 *
 * @note The value of @ref emv_ttl_t.contactless should be populated accurately
 *       and will be used by other EMV processing functions to determine
 *       whether EMV contact processing or EMV contactless processing should be
 *       applied.
 *
 * @param ctx EMV processing context
 * @param ttl Terminal Transport Layer (TTL) context
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_card_activated(struct emv_ctx_t* ctx, struct emv_ttl_t* ttl);

/**
 * Build candidate application list using Payment System Environment (PSE) or
 * discovery of supported AIDs, and then sort according to Application Priority
 * Indicator. This function is intended for contact cards, not contactless
 * cards. Use @ref emv_ep_build_candidate_list() for contactless cards.
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
 * list is empty or not. This function is intended for contact cards, not
 * contactless cards. Use @ref emv_ep_select_application() for contactless
 * cards.
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
 * - @ref emv_ctx_t.terminal
 * - @ref emv_ctx_t.params
 * - @ref emv_ctx_t.config
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
 * Perform EMV Offline Data Authentication (ODA) by selecting and applying the
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

/**
 * Perform EMV Processing Restrictions to determine the compatibility of this
 * implementation and the current configuration with the card application.
 *
 * The following compatibility checks will be performed:
 * - Application Version Number
 * - Application Usage Control
 * - Application Effective/Expiration Dates
 *
 * While performing the compatibility checks, this function will update
 * @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS to reflect the outcomes of
 * those compatibility checks.
 *
 * This function will use values of @ref EMV_TAG_9F35_TERMINAL_TYPE and
 * @ref EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES to determine whether the
 * processing restrictions for ATMs or non-ATMs should be applied. See
 * See EMV 4.4 Book 4, Annex A1 for how this is determined.
 *
 * @remark See EMV 4.4 Book 3, 10.4
 *
 * @param ctx EMV processing context
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_processing_restrictions(struct emv_ctx_t* ctx);

/**
 * Perform EMV Cardholder Verification by processing the Cardholder
 * Verification (CV) Rules of @ref EMV_TAG_8E_CVM_LIST in order and selecting
 * the first Cardholder Verification Method (CVM) for which the CVM Condition
 * is satisfied and which the terminal recognises and supports.
 *
 * CVM Conditions are evaluated using the transaction amount,
 * @ref EMV_TAG_9F35_TERMINAL_TYPE, @ref EMV_TAG_9C_TRANSACTION_TYPE,
 * @ref EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE and, if available,
 * @ref EMV_TAG_9F42_APPLICATION_CURRENCY_CODE. Terminal support for each CVM
 * is determined by @ref EMV_TAG_9F33_TERMINAL_CAPABILITIES. Biometric CVMs are
 * not recognised.
 *
 * This function will push @ref EMV_TAG_9F34_CVM_RESULTS onto
 * @ref emv_ctx_t.terminal and update it to reflect the selected CVM, update
 * @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS to reflect the outcome, and
 * update @ref EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION to indicate that it
 * has been performed when CVM List processing is completed.
 *
 * Upon success, @ref emv_cvm_ctx_t.state will be one of:
 * - @ref EMV_CVM_STATE_COMPLETE when cardholder verification is complete
 * - @ref EMV_CVM_STATE_PIN_ENTRY when PIN entry is required for the CVM
 *   indicated by @ref EMV_TAG_9F34_CVM_RESULTS
 *
 * Calling this function in either of these states has no effect.
 *
 * When PIN entry is required, the caller must perform PIN entry, indicate the
 * outcome using @ref emv_pin_entry_result(), and then call this function
 * again to either complete cardholder verification or continue with the next
 * CV Rule.
 *
 * @remark See EMV 4.4 Book 3, 10.5
 * @remark See EMV 4.4 Book 4, 6.3.4
 *
 * @param ctx EMV processing context
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_cardholder_verification(struct emv_ctx_t* ctx);

/**
 * Indicate the outcome of PIN entry for the CVM selected by
 * @ref emv_cardholder_verification(). This function only indicates whether the
 * PIN pad captured the PIN and does not verify the PIN.
 *
 * This function will update @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS and
 * @ref EMV_TAG_9F34_CVM_RESULTS to reflect the outcome, and will update
 * @ref emv_cvm_ctx_t.state to @ref EMV_CVM_STATE_CVM_PERFORMED. The caller
 * must then call @ref emv_cardholder_verification() again.
 *
 * @note Offline PIN verification is not yet supported and this function will
 *       indicate that PIN pad is not present or not working if the result is
 *       provided as @ref EMV_PIN_ENTRY_CAPTURED for offline PIN verification.
 *
 * @remark See EMV 4.4 Book 3, 10.5
 * @remark See EMV 4.4 Book 4, 6.3.4
 *
 * @param ctx EMV processing context. @ref emv_cvm_ctx_t.state must be
 *            @ref EMV_CVM_STATE_PIN_ENTRY.
 * @param result PIN entry result
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_pin_entry_result(
	struct emv_ctx_t* ctx,
	enum emv_pin_entry_result_t result
);

/**
 * Perform EMV Terminal Risk Management to identify risks to be considered for
 * online authorisation. This function will perform terminal risk management
 * regardless of whether the ICC indicates that it is mandatory in
 * @ref EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE.
 *
 * Terminal risk management consists of:
 * - Floor limit checking
 * - Random transaction selection
 * - Velocity checking
 *
 * While performing terminal risk management, this function will update
 * @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS to reflect the outcomes and
 * update @ref EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION to indicate that it
 * has been performed.
 *
 * @remark See EMV 4.4 Book 3, 10.6
 *
 * @param ctx EMV processing context
 * @param txn_log Ordered transaction log containing previously approved
 *                transactions with the oldest entry first and the newest entry
 *                last. NULL to ignore.
 * @param txn_log_cnt Number of transactions entries in @p txn_log.
 *                    Zero to ignore.
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_terminal_risk_management(struct emv_ctx_t* ctx,
	const struct emv_txn_log_entry_t* txn_log,
	size_t txn_log_cnt
);

/**
 * Perform EMV Card Action Analysis to determine the risk management decision
 * by the ICC as indicated in the response from GENERATE APPLICATION CRYPTOGRAM
 * (GENAC1).
 *
 * If CDA or XDA were selected during Offline Data Authentication (ODA), this
 * function will request the appropriate signature and process the resulting
 * Signed Dynamic Application Data (SDAD) to extract the signed ICC fields.
 *
 * This function will push @ref EMV_TAG_8A_AUTHORISATION_RESPONSE_CODE onto
 * @ref emv_ctx_t.terminal for offline decisions (@ref EMV_TTL_GENAC_TYPE_AAC
 * and @ref EMV_TTL_GENAC_TYPE_TC).
 *
 * @note This function is not yet fully implemented and relies on the caller to
 *       provide the Terminal Action Analysis decision.
 *
 * @remark See EMV 4.4 Book 3, 10.8
 * @remark See EMV 4.4 Book 4, 6.3.2.1
 *
 * @param ctx EMV processing context
 * @param ref_ctrl Reference control parameter for GENAC1; use one of
 *                 @ref EMV_TTL_GENAC_TYPE_AAC (offline decline),
 *                 @ref EMV_TTL_GENAC_TYPE_TC (offline approve), or
 *                 @ref EMV_TTL_GENAC_TYPE_ARQC (request online authorisation)
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_card_action_analysis(struct emv_ctx_t* ctx, uint8_t ref_ctrl);

/**
 * Perform EMV Online Processing and optional issuer authentication.
 *
 * This function will push @ref EMV_TAG_8A_AUTHORISATION_RESPONSE_CODE and
 * optionally @ref EMV_TAG_91_ISSUER_AUTHENTICATION_DATA onto
 * @ref emv_ctx_t.terminal for use by @ref emv_completion().
 *
 * This function will only perform issuer authentication when
 * @ref EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE indicates that the card
 * application supports it (excluding contactless cards) and @p iad of the
 * appropriate length is provided.
 *
 * While performing issuer authentication, this function will update
 * @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS to reflect the outcome and
 * update @ref EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION to indicate that it
 * has been performed.
 *
 * @remark See EMV 4.4 Book 3, 10.9
 * @remark See EMV 4.4 Book 3, Annex F
 *
 * @param ctx EMV processing context
 * @param arc Authorisation Response Code of 2 bytes
 * @param iad Issuer Authentication Data (tag 91) from the online response;
 *            Must be 8 to 16 bytes, or NULL if unavailable
 * @param iad_len Length of @p iad in bytes
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_online_processing(
	struct emv_ctx_t* ctx,
	const uint8_t arc[2],
	const uint8_t* iad,
	size_t iad_len
);

/**
 * Perform EMV Completion to determine the risk management decision by the ICC
 * as indicated in the response from GENERATE APPLICATION CRYPTOGRAM (GENAC2).
 *
 * If CDA was selected during Offline Data Authentication and has not yet
 * failed, this function will request a CDA signature on GENAC2 and process
 * the resulting Signed Dynamic Application Data (SDAD) to extract the
 * signed ICC fields.
 *
 * This function will push @ref EMV_TAG_8A_AUTHORISATION_RESPONSE_CODE onto
 * @ref emv_ctx_t.terminal for offline decisions (@ref EMV_TTL_GENAC_TYPE_AAC
 * and @ref EMV_TTL_GENAC_TYPE_TC) if @ref emv_online_processing() was not
 * called, to indicate that online authorisation was not possible.
 *
 * GENAC2 response fields will be appended to @ref emv_ctx_t.completion.
 *
 * @remark See EMV 4.4 Book 3, 10.11
 *
 * @param ctx EMV processing context
 * @param ref_ctrl Reference control parameter for GENAC2; use one of
 *                 @ref EMV_TTL_GENAC_TYPE_AAC (decline),
 *                 @ref EMV_TTL_GENAC_TYPE_TC (approve)
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_completion(struct emv_ctx_t* ctx, uint8_t ref_ctrl);

/**
 * Compute length of encoded EMV transaction record required by
 * @ref emv_txn_record_encode()
 *
 * @param ctx EMV processing context
 *
 * @return Length of encoded EMV transaction record
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_txn_record_compute_encoded_length(const struct emv_ctx_t* ctx);

/**
 * Encode EMV transaction record
 *
 * This function will encode the terminal data, ICC data, transaction
 * parameters, and configuration data (in the order provided by
 * @ref emv_tlv_sources_init_from_ctx()) using ISO 8825-1 BER. If more than one
 * of these data sources provides the same EMV tag, only the value of the
 * highest priority data source is encoded.
 *
 * @note During encoding, these Offline Data Authentication (ODA) and PIN
 *       encipherment fields are omitted:
 *       - @ref EMV_TAG_90_ISSUER_PUBLIC_KEY_CERTIFICATE
 *       - @ref EMV_TAG_92_ISSUER_PUBLIC_KEY_REMAINDER
 *       - @ref EMV_TAG_9F32_ISSUER_PUBLIC_KEY_EXPONENT
 *       - @ref EMV_TAG_9F46_ICC_PUBLIC_KEY_CERTIFICATE
 *       - @ref EMV_TAG_9F47_ICC_PUBLIC_KEY_EXPONENT
 *       - @ref EMV_TAG_9F48_ICC_PUBLIC_KEY_REMAINDER
 *       - @ref EMV_TAG_93_SIGNED_STATIC_APPLICATION_DATA
 *       - @ref EMV_TAG_9F4B_SIGNED_DYNAMIC_APPLICATION_DATA
 *       - @ref EMV_TAG_9F2D_ICC_PIN_ENCIPHERMENT_PUBLIC_KEY_CERTIFICATE
 *       - @ref EMV_TAG_9F2E_ICC_PIN_ENCIPHERMENT_PUBLIC_KEY_EXPONENT
 *       - @ref EMV_TAG_9F2F_ICC_PIN_ENCIPHERMENT_PUBLIC_KEY_REMAINDER
 *
 * @param ctx EMV processing context
 * @param ptr Encoded EMV transaction record
 * @param len Length of encoded EMV transaction record output in bytes
 *
 * @return Zero for success
 * @return Greater than zero if output data length too small
 * @return Less than zero for errors. See @ref emv_error_t
 */
int emv_txn_record_encode(const struct emv_ctx_t* ctx, void* ptr, size_t* len);

__END_DECLS

#endif
