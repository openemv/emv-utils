/**
 * @file emv_oda.h
 * @brief EMV Offline Data Authentication (ODA) helper functions
 *
 * Copyright 2025 Leon Lynch
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

#ifndef EMV_ODA_H
#define EMV_ODA_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

// Forward declarations
struct emv_tlv_t;
struct emv_tlv_list_t;

/**
 * EMV Offline Data Authentication (ODA) errors.
 *
 * These errors indicate that the session should be terminated.
 */
enum emv_oda_error_t {
	EMV_ODA_ERROR_INTERNAL = -1, ///< Internal error
	EMV_ODA_ERROR_INVALID_PARAMETER = -2, ///< Invalid function parameter
	EMV_ODA_ERROR_AFL_INVALID = -3, ///< Application File Locator (AFL) is invalid
	EMV_ODA_ERROR_TERMINAL_DATA_MISSING = -4, ///< Mandatory terminal data required by ODA method is missing
};

/**
 * EMV Offline Data Authentication (ODA) results.
 *
 * These results indicate the reason why processing did not succeed but also
 * that the session may continue.
 */
enum emv_oda_result_t {
	EMV_ODA_NO_SUPPORTED_METHOD = 1, ///< No supported ODA method
	EMV_ODA_ICC_DATA_MISSING, ///< Mandatory ICC data required by ODA method is missing
	EMV_ODA_SDA_FAILED, ///< Static Data Authentication (SDA) failed
	EMV_ODA_DDA_FAILED, ///< Dynamic Data Authentication (DDA) failed
};

/**
 * EMV Offline Data Authentication (ODA) context
 */
struct emv_oda_ctx_t {
	// Buffer for record data
	uint8_t* buf; ///< Offline Data Authentication (ODA) buffer
	unsigned int buf_len; ///< Length of Offline Data Authentication (ODA) buffer

	// Cached terminal fields
	const struct emv_tlv_t* aid;
	const struct emv_tlv_t* tvr;
	const struct emv_tlv_t* tsi;
};

/**
 * Initialise Offline Data Authentication (ODA) context
 *
 * @param ctx Offline Data Authentication (ODA) context
 * @param afl Application File Locator (AFL) field. Must be multiples of 4 bytes.
 * @param afl_len Length of Application File Locator (AFL) field. Must be multiples of 4 bytes.
 *
 * @return Zero for success.
 * @return Less than zero for error. See @ref emv_oda_error_t
 */
int emv_oda_init(
	struct emv_oda_ctx_t* ctx,
	const uint8_t* afl,
	size_t afl_len
);

/**
 * Clear Offline Data Authentication (ODA) context
 *
 * @param ctx Offline Data Authentication (ODA) context
 *
 * @return Zero for success.
 * @return Less than zero for error. See @ref emv_oda_error_t
 */
int emv_oda_clear(struct emv_oda_ctx_t* ctx);

/**
 * Append Offline Data Authentication (ODA) record
 *
 * @param ctx Offline Data Authentication (ODA) context
 * @param record Record data
 * @param record_len Length of record data in bytes
 *
 * @return Zero for success.
 * @return Less than zero for error. See @ref emv_oda_error_t
 */
int emv_oda_append_record(
	struct emv_oda_ctx_t* ctx,
	const void* record,
	unsigned int record_len
);

/**
 * Select and apply Offline Data Authentication (ODA).
 * @remark See EMV 4.4 Book 3, 10.3
 *
 * This function requires:
 * - @p config must contain @ref EMV_TAG_9F33_TERMINAL_CAPABILITIES
 * - @p icc must contain @ref EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE as
 *   well as all of the fields required by the selected ODA method
 * - @p terminal must contain these fields:
 *   - @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS
 *   - @ref EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION
 *   - @ref EMV_TAG_9F06_AID
 *
 * This function will also add fields to @p icc and @p terminal based on the
 * selected ODA method and reflect the outcome by updating the values of
 * @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS and
 * @ref EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION.
 *
 * @param ctx Offline Data Authentication (ODA) context
 * @param config Terminal configuration
 * @param icc ICC data for current application
 * @param terminal Terminal data for the current transaction
 *
 * @return Zero for success.
 * @return Less than zero indicates that the terminal should terminate the
 *         card session. See @ref emv_oda_error_t
 * @return Greater than zero indicates that offline data authentication is
 *         either not possible or has failed, but that the terminal may
 *         continue the card session. See @ref emv_oda_result_t
 */
int emv_oda_apply(
	struct emv_oda_ctx_t* ctx,
	const struct emv_tlv_list_t* config,
	struct emv_tlv_list_t* icc,
	struct emv_tlv_list_t* terminal
);

/**
 * Apply Static Data Authentication (SDA).
 * @remark See EMV 4.4 Book 2, 5
 *
 * @note This function is intended to be used by @ref emv_oda_apply() and
 *       should only be used directly for use cases beyond EMV requirements.
 *       If in doubt, always use @ref emv_oda_apply() instead.
 *
 * This function requires:
 * - @p icc must contain these fields:
 *   - @ref EMV_TAG_8F_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX
 *   - @ref EMV_TAG_90_ISSUER_PUBLIC_KEY_CERTIFICATE
 *   - @ref EMV_TAG_92_ISSUER_PUBLIC_KEY_REMAINDER (may be absent if empty)
 *   - @ref EMV_TAG_93_SIGNED_STATIC_APPLICATION_DATA
 *   - @ref EMV_TAG_9F32_ISSUER_PUBLIC_KEY_EXPONENT
 * - @p terminal must contain these fields:
 *   - @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS
 *   - @ref EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION
 *   - @ref EMV_TAG_9F06_AID
 *
 * Upon success, this function will update @p terminal to append
 * @ref EMV_TAG_9F22_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX and update @p icc
 * to append @ref EMV_TAG_9F45_DATA_AUTHENTICATION_CODE.
 *
 * @param ctx Offline Data Authentication (ODA) context
 * @param icc ICC data for current application
 * @param terminal Terminal data for the current transaction
 *
 * @return Zero for success.
 * @return Less than zero for error. See @ref emv_oda_error_t
 * @return Greater than zero indicates that static data authentication is
 *         either not possible or has failed, but that the terminal may
 *         continue the card session. See @ref emv_oda_result_t
 */
int emv_oda_apply_sda(
	struct emv_oda_ctx_t* ctx,
	struct emv_tlv_list_t* icc,
	struct emv_tlv_list_t* terminal
);

/**
 * Apply Dynamic Data Authentication (DDA).
 * @remark See EMV 4.4 Book 2, 6.5
 *
 * @note This function is intended to be used by @ref emv_oda_apply() and
 *       should only be used directly for use cases beyond EMV requirements.
 *       If in doubt, always use @ref emv_oda_apply() instead.
 *
 * This function requires:
 * - @p icc must contain these fields:
 *   - @ref EMV_TAG_8F_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX
 *   - @ref EMV_TAG_90_ISSUER_PUBLIC_KEY_CERTIFICATE
 *   - @ref EMV_TAG_92_ISSUER_PUBLIC_KEY_REMAINDER (may be absent if empty)
 *   - @ref EMV_TAG_9F32_ISSUER_PUBLIC_KEY_EXPONENT
 *   - @ref EMV_TAG_9F46_ICC_PUBLIC_KEY_CERTIFICATE
 *   - @ref EMV_TAG_9F47_ICC_PUBLIC_KEY_EXPONENT
 *   - @ref EMV_TAG_9F48_ICC_PUBLIC_KEY_REMAINDER (may be absent if empty)
 * - @p terminal must contain these fields:
 *   - @ref EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS
 *   - @ref EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION
 *   - @ref EMV_TAG_9F06_AID
 *
 * @param ctx Offline Data Authentication (ODA) context
 * @param icc ICC data for current application
 * @param terminal Terminal data for the current transaction
 *
 * Upon success, this function will update @p terminal to append
 * @ref EMV_TAG_9F22_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX.
 *
 * @return Zero for success.
 * @return Less than zero for error. See @ref emv_oda_error_t
 * @return Greater than zero indicates that dynamic data authentication is
 *         either not possible or has failed, but that the terminal may
 *         continue the card session. See @ref emv_oda_result_t
 */
int emv_oda_apply_dda(
	struct emv_oda_ctx_t* ctx,
	const struct emv_tlv_list_t* icc,
	struct emv_tlv_list_t* terminal
);

__END_DECLS

#endif
