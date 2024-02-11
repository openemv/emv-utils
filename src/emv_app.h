/**
 * @file emv_app.h
 * @brief EMV application abstraction and helper functions
 *
 * Copyright (c) 2021, 2024 Leon Lynch
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

#ifndef EMV_APP_H
#define EMV_APP_H

#include <emv_tlv.h>

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdbool.h>

__BEGIN_DECLS

/**
 * EMV application
 */
struct emv_app_t {
	/**
	 * Application Identifier (AID) field, as provided by:
	 * - Application Dedicated File (ADF) Name (field 4F)
	 * - Dedicated File (DF) Name (field 84)
	 * if found in @c tlv_list
	 */
	const struct emv_tlv_t* aid;

	/**
	 * Application display name, as provided by:
	 * - Application Preferred Name (field 9F12, max 16 chars)
	 * - Application Label (field 50, max 16 chars)
	 * - Application Dedicated File (ADF) Name (field 4F, max 32 chars)
	 * - Dedicated File (DF) Name (field 84, max 32 chars)
	 * in order of priority, if found in @c tlv_list
	 */
	char display_name[33];

	/**
	 * Application priority ordering, as provided by Application Priority
	 * Indicator (field 87), bits 1 to 4. Valid range is 1 to 15, with 1 being
	 * the highest priority. Zero if not available.
	 * See EMV 4.4 Book 1, 12.4 for usage
	 */
	uint8_t priority;

	/**
	 * Boolean indicating whether application requires cardholder confirmation
	 * for selection, even if it is the only application.
	 * See EMV 4.4 Book 1, 12.4 for usage
	 */
	bool confirmation_required;

	/**
	 * Application TLVs, as provided by:
	 * - Application Template (field 61) provided by PSE record
	 * - File Control Information (FCI) template (field 6F) provided by application selection
	 */
	struct emv_tlv_list_t tlv_list;

	/// Next EMV application in list
	struct emv_app_t* next;
};

/**
 * EMV application list
 * @note Use the various @c emv_app_list_*() functions to manipulate the list
 */
struct emv_app_list_t {
	struct emv_app_t* front;                    ///< Pointer to front of list
	struct emv_app_t* back;                     ///< Pointer to end of list
};

/**
 * Create new EMV application from encoded EMV data. The data should be the
 * content of the Application Template (field 61) provided by a PSE record.
 *
 * @param pse_tlv_list PSE TLV list obtained from PSE FCI
 * @param pse_dir_entry PSE directory entry (content of Application Template, field 61)
 * @param pse_dir_entry_len Length of PSE directory entry in bytes
 * @return New EMV application object. NULL for error.
 *         Use @ref emv_app_free() to free memory.
 */
struct emv_app_t* emv_app_create_from_pse(
	struct emv_tlv_list_t* pse_tlv_list,
	const void* pse_dir_entry,
	size_t pse_dir_entry_len
);

/**
 * Create new EMV application from encoded EMV data. The data should be the
 * File Control Information (FCI) Template (field 6F) provided by application
 * selection.
 *
 * @param fci FCI data provided by application selection
 * @param fci_len Length of FCI data in bytes
 * @return New EMV application object. NULL for error.
 *         Use @ref emv_app_free() to free memory.
 */
struct emv_app_t* emv_app_create_from_fci(const void* fci, size_t fci_len);

/**
 * Free EMV application and associated EMV TLVs
 * @note This function should not be used to free EMV applications that are elements of a list
 * @param app EMV application to free
 * @return Zero for success. Non-zero if it is unsafe to free the EMV application.
 */
int emv_app_free(struct emv_app_t* app);

/**
 * Determine whether EMV application is supported
 * @param app EMV application
 * @param supported_aids Supported AID (field 9F06) list including ASI flags
 * @return Boolean indicating whether EMV application is supported
 */
bool emv_app_is_supported(
	const struct emv_app_t* app,
	const struct emv_tlv_list_t* supported_aids
);

/// Static initialiser for @ref emv_app_list_t
#define EMV_APP_LIST_INIT { NULL, NULL }

/**
 * Determine whether EMV application list is empty
 * @param list EMV application list
 * @return Boolean indicating whether EMV application list is empty
 */
bool emv_app_list_is_empty(const struct emv_app_list_t* list);

/**
 * Clear EMV application list
 * @note This function will call @ref emv_app_free() for every element
 * @param list EMV application list
 */
void emv_app_list_clear(struct emv_app_list_t* list);

/**
 * Push EMV application on to the back of an EMV application list
 * @note This function will push @c app onto the list without copying it
 * @param list EMV application list
 * @param app EMV application
 * @return Zero for success. Less than zero for error.
 */
int emv_app_list_push(struct emv_app_list_t* list, struct emv_app_t* app);

/**
 * Pop EMV application from the front of an EMV application list
 * @param list EMV application list
 * @return EMV application. Use @ref emv_tlv_free() to free memory.
 */
struct emv_app_t* emv_app_list_pop(struct emv_app_list_t* list);

/**
 * Remove EMV application from EMV application list by index
 * @param list EMV application list
 * @param index Index (starting from zero) of EMV application to remove
 * @return EMV application. Use @ref emv_tlv_free() to free memory.
 */
struct emv_app_t* emv_app_list_remove_index(
	struct emv_app_list_t* list,
	unsigned int index
);

/**
 * Sort EMV application list according to the priority field
 * @param list EMV application list
 * @return Zero for success. Less than zero for error.
 */
int emv_app_list_sort_priority(struct emv_app_list_t* list);

/**
 * Determine whether cardholder application selection is required
 * @note This function should only be used once during transaction processing
 * for the initial candidate application list. If it is determined that
 * cardholder application selection is required, it continues to be required
 * even after the application that required it has been removed from the
 * candidate application list.
 * @param list EMV application list
 * @return Boolean indicating whether cardholder application selection is required
 */
bool emv_app_list_selection_is_required(const struct emv_app_list_t* list);

__END_DECLS

#endif
