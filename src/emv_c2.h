/**
 * @file emv_c2.h
 * @brief High level EMV contactless kernel C-2 interface
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

#ifndef EMV_C2_H
#define EMV_C2_H

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

// Forward declarations
struct emv_ctx_t;

/**
 * Initiate EMV contactless kernel C-2 processing by assessing the Processing
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
 * @remark See EMV Contactless Book C-2 v2.12, 6.3.3
 * @remark See EMV Contactless Book C-2 v2.12, 6.5.3
 *
 * @param ctx EMV processing context
 * @param pos_entry_mode Point-of-Service (POS) Entry Mode (field 9F39) value.
 *                       See @ref pos-entry-mode-values "values".
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_c2_initiate_kernel_processing(
	struct emv_ctx_t* ctx,
	uint8_t pos_entry_mode
);

/**
 * Read EMV application data for kernel C-2 by performing READ RECORD for all
 * records specified by the Application File Locator (AFL), checking that there
 * are no redundant TLV fields provided by the card, and checking for the
 * mandatory fields.
 *
 * This implementation assumes that the "Read all records even when no CDA"
 * flag is enabled.
 *
 * While reading the application records, this function will also concatenate
 * the data required for Offline Data Authentication (ODA) and update
 * @ref emv_ctx_t.oda accordingly.
 *
 * @note Upon success, this function will append the application data to
 *       @ref emv_ctx_t.icc
 *
 * @remark See EMV Contactless Book C-2 v2.12, 6.7.3
 * @remark See EMV Contactless Book C-2 v2.12, 6.8.3
 *
 * @param ctx EMV processing context
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_c2_read_application_data(struct emv_ctx_t* ctx);

/**
 * Perform Cardholder Verification for kernel C-2 by populating the CVM
 * Capability of @ref EMV_TAG_9F33_TERMINAL_CAPABILITIES according to
 * @ref EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, performing CDCVM if
 * supported by both the card and @ref EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION,
 * and otherwise processing the Cardholder Verification (CV) Rules of
 * @ref EMV_TAG_8E_CVM_LIST.
 *
 * This function will push @ref EMV_TAG_9F34_CVM_RESULTS onto
 * @ref emv_ctx_t.terminal and update it to reflect the selected CVM, and
 * update @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS to reflect the
 * outcome. Upon success, @ref emv_cvm_ctx_t.state will be
 * @ref EMV_CVM_STATE_COMPLETE.
 *
 * Kernel C-2 does not perform offline PIN verification and this function will
 * return @ref EMV_ERROR_INVALID_CONFIG if either
 * @ref EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED or
 * @ref EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED indicates offline PIN
 * verification.
 *
 * The caller should use @ref emv_cvm_get() to retrieve the selected CVM. If it
 * indicates @ref EMV_CVM_ONLINE_PIN, then PIN entry should be performed after
 * card interaction is complete. The caller should not use
 * @ref emv_pin_entry_result() to indicate the PIN entry result.
 *
 * @remark See EMV Contactless Book C-2 v2.12, 6.11.3, S456.30
 * @remark See EMV Contactless Book C-2 v2.12, 7.1
 *
 * @param ctx EMV processing context
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for EMV processing outcome. See @ref emv_outcome_t
 */
int emv_c2_cardholder_verification(struct emv_ctx_t* ctx);

__END_DECLS

#endif
