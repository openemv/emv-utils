/**
 * @file pcsc.h
 * @brief PC/SC abstraction
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

#ifndef PCSC_H
#define PCSC_H

#include <sys/cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

// Forward declarations
typedef void* pcsc_ctx_t; ///< PC/SC context pointer type
typedef void* pcsc_reader_ctx_t; ///< PC/SC reader context pointer type

/**
 * Initialize PC/SC context
 * @param ctx PC/SC context pointer
 * @return Zero for success. Less than zero for error.
 */
int pcsc_init(pcsc_ctx_t* ctx);

/**
 * Release PC/SC context
 * @param ctx PC/SC context pointer
 */
void pcsc_release(pcsc_ctx_t* ctx);

/**
 * Retrieve number of available PC/SC readers
 * @param ctx PC/SC context
 * @return Number of available readers
 */
size_t pcsc_get_reader_count(pcsc_ctx_t ctx);

/**
 * Retrieve PC/SC reader context
 * @param ctx PC/SC context
 * @param idx PC/SC reader index
 * @return PC/SC reader context. NULL for error. Do not @ref free()
 */
pcsc_reader_ctx_t pcsc_get_reader(pcsc_ctx_t ctx, size_t idx);

/**
 * Retrieve PC/SC reader name
 * @param reader_ctx PC/SC reader context
 * @return PC/SC reader name. NULL for error. Do not @ref free()
 */
const char* pcsc_reader_get_name(pcsc_reader_ctx_t reader_ctx);

__END_DECLS

#endif
