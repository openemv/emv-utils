/**
 * @file print_helpers.h
 * @brief Helper functions for command line output
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

#ifndef PRINT_HELPERS_H
#define PRINT_HELPERS_H

#include <stddef.h>

// Forward declarations
struct iso7816_atr_info_t;

/**
 * Print buffer as hex digits
 * @param buf_name Name of buffer in ASCII
 * @param buf Buffer
 * @param length Length of buffer in bytes
 */
void print_buf(const char* buf_name, const void* buf, size_t length);

/**
 * Print ATR details, including historical bytes
 * @param atr_info Parsed ATR info
 */
void print_atr(const struct iso7816_atr_info_t* atr_info);

/**
 * Print ATR historical bytes
 * @param atr_info Parsed ATR info
 */
void print_atr_historical_bytes(const struct iso7816_atr_info_t* atr_info);

#endif