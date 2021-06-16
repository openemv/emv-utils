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

#include <iso8825_ber.h>

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

__BEGIN_DECLS

/**
 * EMV TLV field
 * @note The first 4 members of this structure are intentionally identical to @ref iso8825_tlv_t
 */
struct emv_tlv_t {
	union {
		struct {
			unsigned int tag;                   ///< EMV tag
			unsigned int length;                ///< Length of @c value in bytes
			uint8_t* value;                     ///< EMV value buffer
			uint8_t flags;                      ///< EMV field specific flags, eg ASI for AID
		};
		struct iso8825_tlv_t ber;               ///< Alternative BER access to EMV TLV field
	};

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
 * @param flags EMV field specific flags
 * @return Zero for success. Less than zero for error.
 */
int emv_tlv_list_push(
	struct emv_tlv_list_t* list,
	unsigned int tag,
	unsigned int length,
	const uint8_t* value,
	uint8_t flags
);

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

__END_DECLS

#endif
