/**
 * @file emv_cvm.h
 * @brief EMV Cardholder Verification Method (CVM) helper functions
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

#ifndef EMV_CVM_H
#define EMV_CVM_H

#include "emv_cvm_types.h"

#include <sys/cdefs.h>
#include <stdbool.h>
#include <stdint.h>

__BEGIN_DECLS

// Forward declarations
struct emv_ctx_t;

/**
 * EMV Cardholder Verification Method (CVM) errors.
 *
 * These errors indicate that the session should be terminated.
 */
enum emv_cvm_error_t {
	EMV_CVM_ERROR_INTERNAL = -1, ///< Internal error
	EMV_CVM_ERROR_INVALID_PARAMETER = -2, ///< Invalid function parameter
	EMV_CVM_ERROR_CVM_LIST_INVALID = -3, ///< Cardholder Verification Method (CVM) List is invalid
};

/**
 * Initialise Cardholder Verification Method (CVM) context
 *
 * @param ctx Cardholder Verification Method (CVM) context
 *
 * @return Zero for success.
 * @return Less than zero for error. See @ref emv_cvm_error_t
 */
int emv_cvm_init(struct emv_cvm_ctx_t* ctx);

/**
 * Create or update @ref EMV_TAG_9F34_CVM_RESULTS in @ref emv_ctx_t.terminal
 *
 * @param ctx EMV processing context
 * @param cvm CVM Performed (byte 1)
 * @param cvm_cond CVM Condition (byte 2)
 * @param result CVM Result (byte 3)
 *
 * @return Zero for success.
 * @return Less than zero for error. See @ref emv_cvm_error_t
 */
int emv_cvm_results_set(
	struct emv_ctx_t* ctx,
	uint8_t cvm,
	uint8_t cvm_cond,
	uint8_t result
);

/**
 * Update @ref EMV_TAG_9F34_CVM_RESULTS in @ref emv_ctx_t.terminal to indicate
 * that cardholder verification was not successful. The CVM Performed and CVM
 * Condition of the last CVM performed, if any, are retained.
 *
 * @note This function does not update
 *       @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS or
 *       @ref EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION.
 *
 * @remark See EMV 4.4 Book 3, 10.5.5, Figure 12
 * @remark See EMV 4.4 Book 4, 6.3.4.5
 *
 * @param ctx EMV processing context
 *
 * @return Zero for success.
 * @return Less than zero for error. See @ref emv_cvm_error_t
 */
int emv_cvm_results_set_failed(struct emv_ctx_t* ctx);

/**
 * Initialise Cardholder Verification Method (CVM) List processing by
 * populating @ref emv_cvm_ctx_t.amounts and @ref emv_cvm_ctx_t.itr from
 * @ref EMV_TAG_8E_CVM_LIST in @ref emv_ctx_t.icc.
 *
 * @note This function does not update
 *       @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS.
 *
 * @remark See EMV 4.4 Book 3, 10.5
 *
 * @param ctx EMV processing context
 *
 * @return Zero for success.
 * @return Less than zero for error. See @ref emv_cvm_error_t
 * @return Greater than zero if CVM List is not present or does not contain
 *         any Cardholder Verification (CV) Rules.
 */
int emv_cvm_list_init(struct emv_ctx_t* ctx);

/**
 * Determine whether Cardholder Verification Method (CVM) Code is recognised.
 * Biometric CVMs are not recognised.
 *
 * @remark See EMV 4.4 Book 3, 10.5.5
 * @remark See EMV 4.4 Book 3, Annex C3, Table 43
 * @remark See EMV 4.4 Book 4, 6.3.4
 *
 * @param cvm CVM Code without
 *            @ref EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL
 *
 * @return Boolean indicating whether CVM Code is recognised
 */
bool emv_cvm_is_recognised(uint8_t cvm);

/**
 * Determine whether Cardholder Verification Method (CVM) Code is supported
 * according to the CVM Capability of
 * @ref EMV_TAG_9F33_TERMINAL_CAPABILITIES.
 *
 * @remark See EMV 4.4 Book 4, 6.3.4
 * @remark See EMV 4.4 Book 4, Annex A2, Table 26
 *
 * @param cvm CVM Code without
 *            @ref EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL
 * @param cvm_caps CVM Capability (byte 2 of
 *                 @ref EMV_TAG_9F33_TERMINAL_CAPABILITIES)
 *
 * @return Boolean indicating whether CVM Code is supported
 */
bool emv_cvm_is_supported(uint8_t cvm, uint8_t cvm_caps);

/**
 * Determine whether Cardholder Verification Method (CVM) Condition Code is
 * satisfied.
 *
 * CVM Conditions are evaluated using the transaction amount, the CVM List
 * amounts in @ref emv_cvm_ctx_t.amounts, @ref EMV_TAG_9F35_TERMINAL_TYPE,
 * @ref EMV_TAG_9C_TRANSACTION_TYPE,
 * @ref EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE and
 * @ref EMV_TAG_9F42_APPLICATION_CURRENCY_CODE. If the data required by the
 * CVM Condition is not present, or the CVM Condition Code is not understood,
 * the CVM Condition is not satisfied.
 *
 * @remark See EMV 4.4 Book 3, 10.5
 * @remark See EMV 4.4 Book 3, Annex C3, Table 44
 *
 * @param ctx EMV processing context
 * @param cvm_cond CVM Condition Code
 * @param cvm_supported Boolean indicating whether the CVM is recognised and
 *                      supported, for use by
 *                      @ref EMV_CV_RULE_COND_CVM_SUPPORTED
 *
 * @return Boolean indicating whether CVM Condition is satisfied
 */
bool emv_cvm_condition_is_satisfied(
	const struct emv_ctx_t* ctx,
	uint8_t cvm_cond,
	bool cvm_supported
);

__END_DECLS

#endif
