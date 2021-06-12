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
struct emv_tlv_list_t;

/**
 * Read Payment System Environment (PSE) records and build application list
 * @remark See EMV 4.3 Book 1, 12.3.2
 *
 * @param ttl EMV Terminal Transport Layer context
 * @param app_list EMV application list output
 *
 * @return Zero for success
 * @return Less than zero indicates that the terminal should terminate the
 *         card session.
 * @return Greater than zero indicates that reading of PSE records failed and
 *         that the terminal may continue the card session. Typically the list
 *         of AIDs method (@ref emv_tal_find_supported_apps()) would be next.
 */
int emv_tal_read_pse(
	struct emv_ttl_t* ttl,
	struct emv_app_list_t* app_list
);

/**
 * Find supported applications and build application list
 * @remark See EMV 4.3 Book 1, 12.3.3
 *
 * @param ttl EMV Terminal Transport Layer context
 * @param supported_aids Supported AID (field 9F06) list including ASI flags
 * @param app_list Candidate application list output
 *
 * @return Zero for success
 * @return Less than zero indicates that the terminal should terminate the
 *         card session.
 */
int emv_tal_find_supported_apps(
	struct emv_ttl_t* ttl,
	struct emv_tlv_list_t* supported_aids,
	struct emv_app_list_t* app_list
);

__END_DECLS

#endif
