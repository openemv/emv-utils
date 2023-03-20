/**
 * @file emv.h
 * @brief High level EMV library interface
 *
 * Copyright (c) 2023 Leon Lynch
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

#ifndef EMV_H
#define EMV_H

#include <sys/cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

/**
 * Retrieve EMV library version string
 * @return Pointer to null-terminated string. Do not free.
 */
const char* emv_lib_version_string(void);

/**
 * Parse the ISO 7816 Answer To Reset (ATR) message and determine whether the
 * card is suitable for EMV processing
 * @remark See EMV Level 1 Contact Interface Specification v1.0, 8.3
 *
 * @param atr ATR data
 * @param atr_len Length of ATR data
 *
 * @return Zero for success
 * @return Less than zero for internal error
 * @return Greater than zero for parse error or card is not accepted
 */
int emv_atr_parse(const void* atr, size_t atr_len);

__END_DECLS

#endif
