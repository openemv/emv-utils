/**
 * @file emv_c3.h
 * @brief High level EMV contactless kernel C-3 interface
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

#ifndef EMV_C3_H
#define EMV_C3_H

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

// Forward declarations
struct emv_ctx_t;

/**
 * Initiate EMV contactless kernel C-3 processing by assessing the Processing
 * Options Data Object List (PDOL), performing GET PROCESSING OPTIONS, and
 * populating terminal fields related to Entry Point pre-processing.
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
 *       including @ref EMV_TAG_9F39_POS_ENTRY_MODE, @ref EMV_TAG_9F06_AID, and
 *       @ref EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL.
 *
 * @remark See EMV Contactless Book B v2.12, 3.1.1
 * @remark See EMV Contactless Book C-3 v2.12, 2.4.1
 * @remark See EMV Contactless Book C-3 v2.12, 5.2
 *
 * @param ctx EMV processing context
 * @param pos_entry_mode Point-of-Service (POS) Entry Mode (field 9F39) value.
 *                       See @ref pos-entry-mode-values "values".
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_c3_initiate_kernel_processing(
	struct emv_ctx_t* ctx,
	uint8_t pos_entry_mode
);

/**
 * Read EMV application data for kernel C-3 by performing READ RECORD for all
 * records specified by the Application File Locator (AFL), if present,
 * checking for the mandatory fields, and checking that there are no redundant
 * TLV fields provided by the card.
 *
 * While reading the application records, this function will also concatenate
 * the data required for Offline Data Authentication (ODA) and update
 * @ref emv_ctx_t.oda accordingly.
 *
 * @note Upon success, this function will append the application data to
 *       @ref emv_ctx_t.icc
 *
 * @remark See EMV Contactless Book C-3 v2.12, 2.4.2
 * @remark See EMV Contactless Book C-3 v2.12, 5.3
 * @remark See EMV Contactless Book C-3 v2.12, 5.4.2
 *
 * @param ctx EMV processing context
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_c3_read_application_data(struct emv_ctx_t* ctx);

/**
 * Perform Card Action Analysis for kernel C-3 by determining the card
 * transaction disposition from the cryptogram type returned by the card
 * during GET PROCESSING OPTIONS and the Terminal Transaction Qualifiers (TTQ).
 *
 * If the card did not provide @ref EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA,
 * this function will determine the cryptogram type from
 * @ref EMV_TAG_9F10_ISSUER_APPLICATION_DATA instead.
 *
 * This function will push @ref EMV_TAG_8A_AUTHORISATION_RESPONSE_CODE onto
 * @ref emv_ctx_t.terminal for offline approval (@ref EMV_OUTCOME_APPROVED)
 * and offline decline (@ref EMV_OUTCOME_DECLINED).
 *
 * @note Unlike @ref emv_card_action_analysis(), this function does not
 *       perform GENERATE APPLICATION CRYPTOGRAM and only considers the
 *       cryptogram type and the TTQ.
 *
 * @remark See EMV Contactless Book C-3 v2.12, 5.4.3
 * @remark See EMV Contactless Book C-3 v2.12, 5.8.1.1
 * @remark See EMV Contactless Book C-3 v2.12, 5.9.1
 *
 * @param ctx EMV processing context
 *
 * @return Zero for online authorisation. See @ref emv_online_processing()
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_c3_card_action_analysis(struct emv_ctx_t* ctx);

__END_DECLS

#endif
