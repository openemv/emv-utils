/**
 * @file emv_strings.h
 * @brief EMV string helper functions
 *
 * Copyright (c) 2021-2024 Leon Lynch
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

#ifndef EMV_STRINGS_H
#define EMV_STRINGS_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

// Forward declarations
struct emv_tlv_t;

__BEGIN_DECLS

/**
 * EMV data element formats
 * @remark See EMV 4.4 Book 1, 4.3
 */
enum emv_format_t {
	/**
	 * Alphabetic data.
	 * Single character per byte.
	 * a-z, A-Z only.
	 */
	EMV_FORMAT_A,

	/**
	 * Alphanumeric data.
	 * Single character per byte.
	 * a-z, A-Z, 0-9 only.
	 */
	EMV_FORMAT_AN,

	/**
	 * Alphanumeric special data.
	 * Single character per byte.
	 * Encoded using ISO/IEC 8859 common character set.
	 *
	 * @note The Application Label
	 *       (@ref EMV_TAG_50_APPLICATION_LABEL) field is limited to
	 *       a-z, A-Z, 0-9 and the space special character.
	 *
	 * @note The Application Preferred Name
	 *       (@ref EMV_TAG_9F12_APPLICATION_PREFERRED_NAME) field
	 *       may encode non-control characters from the ISO/IEC 8859 part
	 *       designated by the Issuer Code Table Index field
	 *       (@ref EMV_TAG_9F11_ISSUER_CODE_TABLE_INDEX).
	 */
	EMV_FORMAT_ANS,

	/**
	 * Fixed length binary data.
	 * Encoded according to the field type.
	 */
	EMV_FORMAT_B,

	/**
	 * Compressed numeric data.
	 * Two decimal digits per byte.
	 * Left justified and padded with trailing 'F's.
	 */
	EMV_FORMAT_CN,

	/**
	 * Numeric data.
	 * Two decimal digits per byte.
	 * Right justified and padded with leading zeroes.
	 * Also referred to as Binary Coded Decimal (BCD) or Unsigned Packed
	 */
	EMV_FORMAT_N,

	/**
	 * Variable length binary data.
	 * Encoded according to the field type.
	 */
	EMV_FORMAT_VAR,

	/**
	 * Data Object List (DOL).
	 * Encoded according to EMV 4.4 Book 3, 5.4
	 */
	EMV_FORMAT_DOL,

	/**
	 * Tag List
	 */
	EMV_FORMAT_TAG_LIST,
};

/**
 * EMV TLV information as human readable strings
 * @remark See EMV 4.4 Book 1, Annex B
 * @remark See EMV 4.4 Book 3, Annex A
 * @remark See ISO 7816-4:2005, 5.2.4
 */
struct emv_tlv_info_t {
	const char* tag_name;                       ///< Tag name, if available. Otherwise NULL.
	const char* tag_desc;                       ///< Tag description, if available. Otherwise NULL.
	enum emv_format_t format;                   ///< Value format. @see emv_format_t
};

/**
 * Initialise EMV strings. This will load ISO 3166, ISO 4217,
 * and ISO 639 strings from the iso-codes package.
 *
 * @param isocodes_path Override directory path where iso-codes JSON files can
 *                      be found. NULL for default path (recommended).
 * @param mcc_path Override path of mcc-codes JSON file. NULL for default
 *                 path (recommended).
 * @return Zero for success. Less than zero for internal error. Greater than zero if iso-codes package not found.
 */
int emv_strings_init(const char* isocodes_path, const char* mcc_path);

/**
 * Retrieve EMV TLV information, if available, and convert value to human
 * readable UTF-8 string(s), if possible.
 * @note @c value_str output will be empty if human readable string is not available
 *
 * @param tlv Decoded EMV TLV structure
 * @param info EMV TLV information output. See @ref emv_tlv_info_t
 * @param value_str Value string buffer output. NULL to ignore.
 * @param value_str_len Length of value string buffer in bytes. Zero to ignore.
 * @return Zero for success. Non-zero for error.
 */
int emv_tlv_get_info(
	const struct emv_tlv_t* tlv,
	struct emv_tlv_info_t* info,
	char* value_str,
	size_t value_str_len
);

/**
 * Stringify EMV format "a".
 * See @ref EMV_FORMAT_A
 *
 * @param buf Buffer containing EMV format "a" data
 * @param buf_len Length of buffer in bytes
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_format_a_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);

/**
 * Stringify EMV format "an".
 * See @ref EMV_FORMAT_AN
 *
 * @param buf Buffer containing EMV format "an" data
 * @param buf_len Length of buffer in bytes
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_format_an_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);

/**
 * Stringify EMV format "ans", with special characters limited to space
 * character, to UTF-8.
 * See @ref EMV_FORMAT_ANS regarding @ref EMV_TAG_50_APPLICATION_LABEL
 * @remark See ISO/IEC 8859
 *
 * @param buf Buffer containing EMV format "ans" data
 * @param buf_len Length of buffer in bytes
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_format_ans_only_space_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);

/**
 * Stringify EMV format "ans" (using ISO/IEC 8859 common character set) to
 * UTF-8.
 * See @ref EMV_FORMAT_ANS
 * @remark See ISO/IEC 8859
 *
 * @param buf Buffer containing EMV format "ans" data
 * @param buf_len Length of buffer in bytes
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_format_ans_ccs_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);

/**
 * Stringify EMV format "cn" (trailing 'F's will be omitted).
 * See @ref EMV_FORMAT_CN
 *
 * @param buf Buffer containing EMV format "cn" data
 * @param buf_len Length of buffer in bytes
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_format_cn_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);

/**
 * Stringify EMV format "n" (leading zeros will be omitted).
 * See @ref EMV_FORMAT_N
 *
 * @param buf Buffer containing EMV format "n" data
 * @param buf_len Length of buffer in bytes
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_format_n_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);

/**
 * Convert string to EMV format "cn" (two decimal digits per byte,
 * left justified, padded with trailing 'F's).
 * See @ref EMV_FORMAT_CN
 *
 * @param str NULL-terminated string
 * @param buf Output buffer
 * @param buf_len Length of output buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_str_to_format_cn(const char* str, uint8_t* buf, size_t buf_len);

/**
 * Convert string to EMV format "n" (two decimal digits per byte,
 * right justified and padded with leading zeroes).
 * See @ref EMV_FORMAT_N
 *
 * @param str NULL-terminated string
 * @param buf Output buffer
 * @param buf_len Length of output buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_str_to_format_n(const char* str, uint8_t* buf, size_t buf_len);

/**
 * Stringify EMV format "b" amount field
 * @note This function can only be used for amount fields
 * @param buf Buffer containing EMV format "b" amount
 * @param buf_len Length of buffer in bytes
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_amount_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);

/**
 * Stringify Transaction Date (field 9A) using ISO 8601 YYYY-MM-DD format
 * @param buf Transaction date field
 * @param buf_len Length of transaction date field. Must be 3 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_date_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);

/**
 * Stringify Transaction Time (field 9F21) using ISO 8601 hh:mm:ss format
 * @param buf Transaction time field
 * @param buf_len Length of transaction time field. Must be 3 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_time_get_string(const uint8_t* buf, size_t buf_len, char* str, size_t str_len);

/**
 * Stringify Transaction Type (field 9C)
 * @param txn_type Transaction type field
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_transaction_type_get_string(
	uint8_t txn_type,
	char* str,
	size_t str_len
);

/**
 * Stringify Merchant Category Code (field 9F15)
 * @param mcc Merchant Category Code (MCC) field. Must be 2 bytes.
 * @param mcc_len Length of Merchant Category Code (MCC) field. Must be 2 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_mcc_get_string(
	const uint8_t* mcc,
	size_t mcc_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Terminal Type (field 9F35)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param term_type Terminal type field
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_term_type_get_string_list(
	uint8_t term_type,
	char* str,
	size_t str_len
);

/**
 * Stringify Terminal Capabilities (field 9F33)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param term_caps Terminal capabilities field. Must be 3 bytes.
 * @param term_caps_len Length of terminal capabilities field. Must be 3 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_term_caps_get_string_list(
	const uint8_t* term_caps,
	size_t term_caps_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Point-of-Service (POS) Entry Mode (field 9F39)
 * @param pos_entry_mode POS entry mode field
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_pos_entry_mode_get_string(
	uint8_t pos_entry_mode,
	char* str,
	size_t str_len
);

/**
 * Stringify Additional Terminal Capabilities (field 9F40)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param addl_term_caps Additional terminal capabilities field. Must be 5 bytes.
 * @param addl_term_caps_len Length of additional terminal capabilities field. Must be 5 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_addl_term_caps_get_string_list(
	const uint8_t* addl_term_caps,
	size_t addl_term_caps_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Application Identifier (field 4F, 84, or 9F06)
 * @param aid Application Identifier (AID) field. Must be 5 to 16 bytes.
 * @param aid_len Length of Application Identifier (AID) field. Must be 5 to 16 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_aid_get_string(
	const uint8_t* aid,
	size_t aid_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Application Selection Registered Proprietary Data (field 9F0A)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param asrpd Application Selection Registered Proprietary Data (ASRPD) field.
 * @param asrpd_len Length of Application Selection Registered Proprietary Data (ASRPD) field.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_asrpd_get_string_list(
	const uint8_t* asrpd,
	size_t asrpd_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Application Interchange Profile (field 82)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param aip Application Interchange Profile (AIP) field. Must be 2 bytes.
 * @param aip_len Length of Application Interchange Profile (AIP) field. Must be 2 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_aip_get_string_list(
	const uint8_t* aip,
	size_t aip_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Application File Locator (field 94)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param afl Application File Locator (AFL) field. Must be multiples of 4 bytes.
 * @param afl_len Length of Application File Locator (AFL) field. Must be multiples of 4 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_afl_get_string_list(
	const uint8_t* afl,
	size_t afl_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Application Usage Control (field 9F07)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param auc Application Usage Control (AUC) field. Must be 2 bytes.
 * @param auc_len Length of Application Usage Control (AUC) field. Must be 2 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_app_usage_control_get_string_list(
	const uint8_t* auc,
	size_t auc_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Track 2 Equivalent Data (field 57)
 * @param track2 Track 2 Equivalent Data field
 * @param track2_len Length of Track 2 Equivalent Data field
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_track2_equivalent_data_get_string(
	const uint8_t* track2,
	size_t track2_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Application Reference Currency (field 9F3B)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param arc Application Reference Currency field. Must be multiples of 2 bytes.
 * @param arc_len Length of Application Reference Currency field. Must be multiples of 2 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_app_reference_currency_get_string_list(
	const uint8_t* arc,
	size_t arc_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Language Preference (field 5F2D)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param lp Language Preference field. Must be multiples of 2 bytes.
 * @param lp_len Length of Language Preference field. Must be multiples of 2 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_language_preference_get_string_list(
	const uint8_t* lp,
	size_t lp_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Account Type (field 5F57)
 * @param account_type Account Type field
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_account_type_get_string(
	uint8_t account_type,
	char* str,
	size_t str_len
);

/**
 * Stringify Cardholder Verification Method (CVM) List (field 8E)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param cvmlist Cardholder Verification Method (CVM) List field
 * @param cvmlist_len Length of Cardholder Verification Method (CVM) List field
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_cvm_list_get_string_list(
	const uint8_t* cvmlist,
	size_t cvmlist_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Cardholder Verification Method (CVM) Results (field 9F34)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param cvmresults Cardholder Verification Method (CVM) Results field. Must be 3 bytes.
 * @param cvmresults_len Length of Cardholder Verification Method (CVM) Results field. Must be 3 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_cvm_results_get_string_list(
	const uint8_t* cvmresults,
	size_t cvmresults_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Terminal Verification Results (field 95)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param tvr Terminal Verification Results (TVR) field. Must be 5 bytes.
 * @param tvr_len Length of Terminal Verification Results (TVR) field. Must be 5 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_tvr_get_string_list(
	const uint8_t* tvr,
	size_t tvr_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Transaction Status Information (field 9B)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param tsi Transaction Status Information (TSI) field. Must be 2 bytes.
 * @param tsi_len Length of Transaction Status Information (TSI) field. Must be 2 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_tsi_get_string_list(
	const uint8_t* tsi,
	size_t tsi_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Cryptogram Information Data (field 9F27)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param cid Cryptogram Information Data (field 9F27)
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_cid_get_string_list(
	uint8_t cid,
	char* str,
	size_t str_len
);

/**
 * Stringify Issuer Application Data (field 9F10)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param iad Issuer Application Data (IAD) field. Must be 1 to 32 bytes.
 * @param iad_len Length of Issuer Application Data (IAD) field. Must be 1 to 32 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_iad_get_string_list(
	const uint8_t* iad,
	size_t iad_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Terminal Risk Management Data (field 9F1D)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param trmd Terminal Risk Management Data field. Must be 8 bytes.
 * @param trmd_len Length of Terminal Risk Management Data field. Must be 8 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_terminal_risk_management_data_get_string_list(
	const uint8_t* trmd,
	size_t trmd_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Terminal Transaction Qualifiers (field 9F66)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param ttq Terminal Transaction Qualifiers (TTQ) field. Must be 4 bytes.
 * @param ttq_len Length of Terminal Transaction Qualifiers (TTQ) field. Must be 4 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_ttq_get_string_list(
	const uint8_t* ttq,
	size_t ttq_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Card Transaction Qualifiers (field 9F6C)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param ctq Card Transaction Qualifiers (CTQ) field. Must be 2 bytes.
 * @param ctq_len Length of Card Transaction Qualifiers (CTQ) field. Must be 2 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_ctq_get_string_list(
	const uint8_t* ctq,
	size_t ctq_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Amex Contactless Reader Capabilities (field 9F6D)
 * @param cl_reader_caps Contactless Reader Capabilities field
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_amex_cl_reader_caps_get_string(
	uint8_t cl_reader_caps,
	char* str,
	size_t str_len
);

/**
 * Stringify Mastercard Third Party Data (field 9F6E)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param tpd Third Party Data field. Must be 5 to 32 bytes.
 * @param tpd_len Length of Third Party Data field. Must be 5 to 32 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_mastercard_third_party_data_get_string_list(
	const uint8_t* tpd,
	size_t tpd_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Visa Form Factor Indicator (field 9F6E)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param ffi Form Factor Indicator (FFI) field. Must be 4 bytes.
 * @param ffi_len Length of Form Factor Indicator (FFI) field. Must be 4 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_visa_form_factor_indicator_get_string_list(
	const uint8_t* ffi,
	size_t ffi_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Amex Enhanced Contactless Reader Capabilities (field 9F6E)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param enh_cl_reader_caps Enhanced Contactless Reader Capabilities field. Must be 4 bytes.
 * @param enh_cl_reader_caps_len Length of Enhanced Contactless Reader Capabilities field. Must be 4 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_amex_enh_cl_reader_caps_get_string_list(
	const uint8_t* enh_cl_reader_caps,
	size_t enh_cl_reader_caps_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Authorisation Response Code (field 8A)
 * @param arc Authorisation Response Code field. Must be 2 bytes.
 * @param arc_len Length of Authorisation Response Code field. Must be 2 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_auth_response_code_get_string(
	const void* arc,
	size_t arc_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Issuer Authentication Data (field 91)
 * @note Strings in output buffer are delimited using "\n", including the last string
 * @param iad Issuer Authentication Data field. Must be 8 to 16 bytes.
 * @param iad_len Length of Issuer Authentication Data field. Must be 8 to 16 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_issuer_auth_data_get_string_list(
	const uint8_t* iad,
	size_t iad_len,
	char* str,
	size_t str_len
);

/**
 * Stringify Command Application Protocol Data Unit (C-APDU)
 * @param c_apdu Command Application Protocol Data Unit (C-APDU). Must be at least 4 bytes.
 * @param c_apdu_len Length of Command Application Protocol Data Unit (C-APDU). Must be at least 4 bytes.
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_capdu_get_string(
	const uint8_t* c_apdu,
	size_t c_apdu_len,
	char* str,
	size_t str_len
);

__END_DECLS

#endif
