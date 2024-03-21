/**
 * @file emv_dol.h
 * @brief EMV Data Object List (DOL) processing functions
 * @remark See EMV 4.4 Book 3, 5.4
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


#ifndef EMV_DOL_H
#define EMV_DOL_H

#include <sys/cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

// Forward declarations
struct emv_tlv_list_t;

/// EMV Data Object List (DOL) entry
struct emv_dol_entry_t {
	unsigned int tag; ///< EMV tag
	unsigned int length; ///< Expected length
};

/// EMV Data Object List (DOL) iterator
struct emv_dol_itr_t {
	const void* ptr; ///< Encoded EMV Data Object List (DOL)
	size_t len; ///< Length of encoded EMV Data Object List (DOL) in bytes
};

/**
 * Decode EMV Data Object List (DOL) entry
 * @remark See EMV 4.4 Book 3, 5.4
 *
 * @param ptr Encoded EMV Data Object List (DOL) entry
 * @param len Length of encoded EMV Data Object List (DOL) entry in bytes
 * @param entry Decoded Data Object List (DOL) entry output
 * @return Number of bytes consumed. Zero for end of data. Less than zero for error.
 */
int emv_dol_decode(const void* ptr, size_t len, struct emv_dol_entry_t* entry);

/**
 * Initialise Data Object List (DOL) iterator
 * @param ptr Encoded EMV Data Object List (DOL)
 * @param len Length of encoded EMV Data Object List (DOL) in bytes
 * @param itr DOL iterator output
 * @return Zero for success. Less than zero for error.
 */
int emv_dol_itr_init(const void* ptr, size_t len, struct emv_dol_itr_t* itr);

/**
 * Decode next Data Object List (DOL) entry and advance iterator
 * @param itr DOL iterator
 * @param entry Decoded Data Object List (DOL) entry output
 * @return Number of bytes consumed. Zero for end of data. Less than zero for error.
 */
int emv_dol_itr_next(struct emv_dol_itr_t* itr, struct emv_dol_entry_t* entry);

/**
 * Compute concatenated data length required by Data Object List (DOL)
 * @param ptr Encoded EMV Data Object List (DOL)
 * @param len Length of encoded EMV Data Object List (DOL) in bytes
 * @return Length of command data. Less than zero for error.
 */
int emv_dol_compute_data_length(const void* ptr, size_t len);

/**
 * Build concatenated data according to Data Object List (DOL)
 * @param ptr Encoded EMV Data Object List (DOL)
 * @param len Length of encoded EMV Data Object List (DOL) in bytes
 * @param source1 EMV TLV list used as primary source. Required.
 * @param source2 EMV TLV list used as secondary source. NULL to ignore.
 * @param data Concatenated data output
 * @param data_len Length of concatenated data output in bytes
 * @return Zero for success. Less than zero for internal error. Greater than zero if output data length too small.
 */
int emv_dol_build_data(
	const void* ptr,
	size_t len,
	const struct emv_tlv_list_t* source1,
	const struct emv_tlv_list_t* source2,
	void* data,
	size_t* data_len
);

__END_DECLS

#endif
