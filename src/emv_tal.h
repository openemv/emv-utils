/**
 * @file emv_tal.h
 * @brief EMV Terminal Application Layer (TAL)
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

#ifndef EMV_TAL_H
#define EMV_TAL_H

#include <sys/cdefs.h>

__BEGIN_DECLS

// Forward declarations
struct emv_ttl_t;
struct emv_app_list_t;

/**
 * Read Payment System Environment (PSE) records and build application list
 *
 * @param ttl EMV Terminal Transport Layer context
 * @param app_list EMV application list output
 *
 * @return Zero for success
 * @return Less than zero indicates that the terminal should terminate the
 *         card session.
 * @return Greater than zero indicates that reading of PSE records failed and
 *         that the terminal may use the list of AIDs method.
 */
int emv_tal_read_pse(
	struct emv_ttl_t* ttl,
	struct emv_app_list_t* app_list
);

__END_DECLS

#endif
