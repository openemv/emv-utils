/**
 * @file emv_debug.c
 * @brief EMV debug implementation
 *
 * Copyright 2021, 2024 Leon Lynch
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

#include "emv_debug.h"
#include "emv_utils_config.h"

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

// TODO: replace with HAL interface in future
#ifdef HAVE_TIME_H
#include <time.h>
#endif

static unsigned int debug_sources_mask = EMV_DEBUG_SOURCE_NONE;
static enum emv_debug_level_t debug_level = EMV_DEBUG_NONE;
static emv_debug_func_t debug_func = NULL;

int emv_debug_init(
	unsigned int sources_mask,
	enum emv_debug_level_t level,
	emv_debug_func_t func
)
{
	debug_sources_mask = sources_mask;
	debug_level = level;
	debug_func = func;

	return 0;
}

void emv_debug_internal(
	enum emv_debug_source_t source,
	enum emv_debug_level_t level,
	enum emv_debug_type_t debug_type,
	const char* fmt,
	const void* buf,
	size_t buf_len,
	...
)
{
	int r;
	char str[1024];
	struct timespec t;
	uint32_t timestamp;

	if (!debug_func) {
		return;
	}

	if ((debug_sources_mask & source) == 0) {
		return;
	}

	if (level > debug_level) {
		return;
	}

	va_list ap;
	va_start(ap, buf_len);
	r = vsnprintf(str, sizeof(str), fmt, ap);
	va_end(ap);
	if (r < 0) {
		// vsnprintf() error
		return;
	}

	// TODO: replace with HAL interface in future
#if defined(HAVE_TIMESPEC_GET)
	timespec_get(&t, TIME_UTC);
#elif defined(HAVE_CLOCK_GETTIME)
	clock_gettime(CLOCK_MONOTONIC, &t);
#else
#error "No platform function for current time"
#endif

	// Pack timespec fields into 32-bit timestamp with microsecond granularity
	timestamp = (uint32_t)(((t.tv_sec * 1000000) + (t.tv_nsec / 1000)));

	debug_func(timestamp, source, level, debug_type, str, buf, buf_len);
}
