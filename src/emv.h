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

__BEGIN_DECLS

/**
 * Retrieve EMV library version string
 * @return Pointer to null-terminated string. Do not free.
 */
const char* emv_lib_version_string(void);

__END_DECLS

#endif
