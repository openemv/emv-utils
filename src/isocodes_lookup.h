/**
 * @file isocodes_lookup.h
 * @brief Wrapper for iso-codes package
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

#ifndef ISOCODES_LOOKUP_H
#define ISOCODES_LOOKUP_H

#include <sys/cdefs.h>

__BEGIN_DECLS

/**
 * Initialise lookup data from installed iso-codes package
 * @return Zero for success. Less than zero for internal error. Greater than zero if iso-codes package not found.
 */
int isocodes_init(void);

/**
 * Lookup currency name by ISO 4217 3-digit alpha code
 * @param alpha3 ISO 4217 3-digit alpha code
 * @return Currency name. NULL if not found.
 */
const char* isocodes_lookup_currency_by_alpha3(const char* alpha3);

/**
 * Lookup currency name by ISO 4217 3-digit numeric code
 * @param numeric ISO 4217 3-digit numeric code
 * @return Currency name. NULL if not found.
 */
const char* isocodes_lookup_currency_by_numeric(unsigned int numeric);

__END_DECLS

#endif
