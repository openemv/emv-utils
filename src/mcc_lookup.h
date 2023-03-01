/**
 * @file mcc_lookup.h
 * @brief ISO 18245 Merchant Category Code (MCC) lookup helper functions
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

#ifndef MCC_LOOKUP_H
#define MCC_LOOKUP_H

#include <sys/cdefs.h>

__BEGIN_DECLS

/**
 * Initialise Merchant Category Code (MCC) data
 * @param path Override path of mcc-codes JSON file. NULL for default path.
 * @return Zero for success. Less than zero for internal error. Greater than zero if mcc-codes JSON file not found.
 */
int mcc_init(const char* path);

/**
 * Lookup Merchant Category Code (MCC) string
 * @param mcc Merchant Category Code (MCC)
 * @return Merchant Category Code (MCC) string. NULL if not found.
 */
const char* mcc_lookup(unsigned int mcc);

__END_DECLS

#endif
