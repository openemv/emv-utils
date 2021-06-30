/**
 * @file emv_dol.h
 * @brief EMV Data Object List processing functions
 * @remark See EMV 4.3 Book 3, 5.4
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


#ifndef EMV_DOL_H
#define EMV_DOL_H

#include <sys/cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

/// EMV Data Object List (DOL) entry
struct emv_dol_entry_t {
	unsigned int tag;
	unsigned int length;
};

/// EMV Data Object List (DOL) iterator
struct emv_dol_itr_t {
	const void* ptr;
	size_t len;
};

/**
 * Decode EMV Data Object List (DOL) entry
 * @remark See EMV 4.3 Book 3, 5.4
 *
 * @param ptr Encoded EMV Data Object List (DOL) data
 * @param len Length of encoded EMV Data Object List (DOL) data in bytes
 * @param entry Decoded Data Object List (DOL) entry output
 * @return Number of bytes consumed. Zero for end of data. Less than zero for error.
 */
int emv_dol_decode(const void* ptr, size_t len, struct emv_dol_entry_t* entry);

/**
 * Initialize Data Object List (DOL) iterator
 * @param ptr DOL encoded data
 * @param len Length of DOL encoded data in bytes
 * @param itr DOL iterator output
 * @return Zero for success. Less than zero for error.
 */
int emv_dol_itr_init(const void* ptr, size_t len, struct emv_dol_itr_t* itr);

/**
 * Decode next Data Object List (DOL) entry and advance iterator
 * @param itr DOL iterator
 * @param entry Decoded Data Object List entry output
 * @return Number of bytes consumed. Zero for end of data. Less than zero for error.
 */
int emv_dol_itr_next(struct emv_dol_itr_t* itr, struct emv_dol_entry_t* entry);

__END_DECLS

#endif
