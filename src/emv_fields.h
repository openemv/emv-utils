/**
 * @file emv_fields.h
 * @brief EMV field definitions
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

#ifndef EMV_FIELDS_H
#define EMV_FIELDS_H

#include <sys/cdefs.h>

__BEGIN_DECLS

// Application Priority Indicator
// See EMV 4.3 Book 1, 12.2.3, table 48
#define EMV_APP_PRIORITY_INDICATOR_MASK                         (0x0F)
#define EMV_APP_PRIORITY_INDICATOR_CONF_REQUIRED                (0x80)

__END_DECLS

#endif
