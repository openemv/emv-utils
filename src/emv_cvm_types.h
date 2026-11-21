/**
 * @file emv_cvm_types.h
 * @brief EMV Cardholder Verification Method (CVM) types used by high level EMV
 *        library interface
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

#ifndef EMV_CVM_TYPES_H
#define EMV_CVM_TYPES_H

#include "emv_fields.h"

#include <sys/cdefs.h>

__BEGIN_DECLS

/**
 * EMV Cardholder Verification state
 */
enum emv_cvm_state_t {
	EMV_CVM_STATE_NONE = 0, ///< Cardholder verification not yet started
	EMV_CVM_STATE_PIN_ENTRY, ///< PIN entry required for the CVM indicated by @ref EMV_TAG_9F34_CVM_RESULTS
	EMV_CVM_STATE_CVM_PERFORMED, ///< CVM indicated by @ref EMV_TAG_9F34_CVM_RESULTS was performed and its outcome is indicated by the CVM Result in byte 3
	EMV_CVM_STATE_COMPLETE, ///< Cardholder verification complete
};

/**
 * EMV PIN entry result
 */
enum emv_pin_entry_result_t {
	EMV_PIN_ENTRY_CAPTURED = 1, ///< PIN captured by PIN pad
	EMV_PIN_ENTRY_BYPASSED, ///< PIN entry bypassed by merchant or cardholder
	EMV_PIN_ENTRY_PIN_PAD_FAILED, ///< PIN pad not present or not working
};

/**
 * EMV Cardholder Verification Method (CVM) context
 */
struct emv_cvm_ctx_t {
	/// Current Cardholder Verification state
	enum emv_cvm_state_t state;

	/// Cardholder Verification Method (CVM) List amounts
	struct emv_cvmlist_amounts_t amounts;

	/**
	 * Cardholder Verification Method (CVM) List iterator positioned after
	 * the current Cardholder Verification (CV) Rule
	 */
	struct emv_cvmlist_itr_t itr;
};

__END_DECLS

#endif
