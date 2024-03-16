/**
 * @file emv.h
 * @brief High level EMV library interface
 *
 * Copyright (c) 2023-2024 Leon Lynch
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

#include <sys/cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

// Forward declarations
struct emv_ttl_t;
struct emv_tlv_list_t;
struct emv_app_list_t;
struct emv_app_t;

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
 * @param ttl EMV Terminal Transport Layer context
 * @param supported_aids Supported AID (field 9F06) list including ASI flags
 * @param app_list Candidate application list output
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_build_candidate_list(
	struct emv_ttl_t* ttl,
	const struct emv_tlv_list_t* supported_aids,
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
 * @param ttl EMV Terminal Transport Layer context
 * @param app_list Candidate application list
 * @param index Index (starting from zero) of EMV application to select
 * @param selected_app Selected EMV application output. Use @ref emv_app_free() to free memory.
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_select_application(
	struct emv_ttl_t* ttl,
	struct emv_app_list_t* app_list,
	unsigned int index,
	struct emv_app_t** selected_app
);

/**
 * Initiate EMV application processing by assessing the Processing Options
 * Data Object List (PDOL) and performing GET PROCESSING OPTIONS.
 * @note Upon success, this function will also move the selected application's
 *       TLV data to the ICC data output and append the output of
 *       GET PROCESSING OPTIONS.
 * @remark See EMV 4.4 Book 3, 10.1
 * @remark See EMV 4.4 Book 4, 6.3.1
 *
 * @param ttl EMV Terminal Transport Layer context
 * @param selected_app Selected EMV application
 * @param source1 EMV TLV list used as primary source. Required.
 * @param source2 EMV TLV list used as secondary source. NULL to ignore.
 * @param icc ICC data output
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_initiate_application_processing(
	struct emv_ttl_t* ttl,
	struct emv_app_t* selected_app,
	const struct emv_tlv_list_t* source1,
	const struct emv_tlv_list_t* source2,
	struct emv_tlv_list_t* icc
);

/**
 * Read EMV application data by performing READ RECORD for all records
 * specified by the Application File Locator (AFL), checking that there are no
 * redundant TLV fields provided by the application records, and checking for
 * the mandatory fields.
 * @note Upon success, this function will append the TLV data to the ICC data
 *       output
 * @remark See EMV 4.4 Book 3, 10.2
 *
 * @param ttl EMV Terminal Transport Layer context
 * @param icc ICC data output
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_read_application_data(
	struct emv_ttl_t* ttl,
	struct emv_tlv_list_t* icc
);

__END_DECLS

#endif
