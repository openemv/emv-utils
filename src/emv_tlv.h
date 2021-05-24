/**
 * @file emv_tlv.h
 * @brief EMV TLV structures and helper functions
 *
 * Copyright (c) 2021 Leon Lynch
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

#ifndef EMV_TLV_H
#define EMV_TLV_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

__BEGIN_DECLS

/**
 * EMV TLV field
 * @note The first 3 members of this structure are intentionally identical to @ref iso8825_tlv_t
 */
struct emv_tlv_t {
	unsigned int tag;                           ///< EMV tag
	unsigned int length;                        ///< Length of @c value in bytes
	uint8_t* value;                             ///< EMV value buffer

	struct emv_tlv_t* next;                     ///< Next EMV TLV field in list
};

/**
 * EMV TLV list
 * @note Use the various @c emv_tlv_list_*() functions to manipulate the list
 */
struct emv_tlv_list_t {
	struct emv_tlv_t* front;                    ///< Pointer to front of list
	struct emv_tlv_t* back;                     ///< Pointer to end of list
};

/**
 * EMV data element formats
 * @remark See EMV 4.3 Book 1, 4.3
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
};

/**
 * EMV TLV information as human readable strings
 * @remark See EMV 4.3 Book 1, Annex B
 * @remark See EMV 4.3 Book 3, Annex A
 * @remark See ISO 7816-4:2005, 5.2.4
 */
struct emv_tlv_info_t {
	const char* tag_name;                       ///< Tag name, if available. Otherwise NULL.
	const char* tag_desc;                       ///< Tag description, if available. Otherwise NULL.
	enum emv_format_t format;                   ///< Value format. @see emv_format_t
};

/// Static initialiser for @ref emv_tlv_t
#define EMV_TLV_INIT { 0, 0, NULL, NULL }

/// Static initialiser for @ref emv_tlv_list_t
#define EMV_TLV_LIST_INIT { NULL, NULL }

/**
 * Free EMV TLV field
 * @note This function should not be used to free EMV TLV fields that are elements of a list
 * @param tlv EMV TLV field to free
 * @return Zero for success. Non-zero if it is unsafe to free the EMV TLV field.
 */
int emv_tlv_free(struct emv_tlv_t* tlv);

/**
 * Determine whether EMV TLV list is empty
 * @return Boolean indicating whether EMV TLV list is empty
 */
bool emv_tlv_list_is_empty(const struct emv_tlv_list_t* list);

/**
 * Clear EMV TLV list
 * @note This function will call @ref emv_tlv_free() for every element
 * @param list EMV TLV list object
 */
void emv_tlv_list_clear(struct emv_tlv_list_t* list);

/**
 * Push EMV TLV field on to the back of an EMV TLV list
 * @note This function will copy the data from the @c value parameter
 * @param list EMV TLV list
 * @param tag EMV TLV tag
 * @param length EMV TLV length
 * @param value EMV TLV value
 * @return Zero for success. Less than zero for error.
 */
int emv_tlv_list_push(struct emv_tlv_list_t* list, unsigned int tag, unsigned int length, const uint8_t* value);

/**
 * Pop EMV TLV field from the front of an EMV TLV list
 * @param list EMV TLV list
 * @return EMV TLV field. Use @ref emv_tlv_free() to free memory.
 */
struct emv_tlv_t* emv_tlv_list_pop(struct emv_tlv_list_t* list);

/**
 * Find EMV TLV field in an EMV TLV list
 * @param list EMV TLV list
 * @param tag EMV tag to find
 * @return EMV TLV field. Do NOT free. NULL if not found.
 */
struct emv_tlv_t* emv_tlv_list_find(struct emv_tlv_list_t* list, unsigned int tag);

/**
 * Parse EMV data.
 * This function will recursively parse ISO 8825-1 BER encoded EMV data and
 * output a flat list which omits the constructed/template fields.
 *
 * @note The @c list parameter is not cleared when the function fails. This
 * allows the caller to inspect the list after parsing failed but requires
 * the caller to clear the list using @ref emv_tlv_list_clear() to avoid
 * memory leaks.
 *
 * @param ptr Encoded EMV data
 * @param len Length of encoded EMV data in bytes
 * @param list Decoded EMV TLV list output.
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_tlv_parse(const void* ptr, size_t len, struct emv_tlv_list_t* list);

/**
 * Retrieve EMV TLV information, if available, and convert value to human
 * readable string(s), if possible.
 * @note @c value_str output will be empty if human readable string is not available
 *
 * @param tlv Decoded EMV TLV structure
 * @param info EMV TLV information output. See @ref emv_tlv_info_t
 * @param value_str Value string buffer output. NULL to ignore.
 * @param value_str_len Length of value string buffer in bytes. Zero to ignore.
 * @return Zero for success. Less than zero for error.
 */
int emv_tlv_get_info(
	const struct emv_tlv_t* tlv,
	struct emv_tlv_info_t* info,
	char* value_str,
	size_t value_str_len
);

__END_DECLS

#endif
