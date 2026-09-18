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
 * @remark See EMV Contactless Book B v2.11, 3.1.1
 * @remark See EMV Contactless Book C-2 v2.11, 6.3.3
 * @remark See EMV Contactless Book C-2 v2.11, 6.5.3
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

__END_DECLS

#endif
