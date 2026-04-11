/**
 * @file emv_config_xml.c
 * @brief EMV configuration XML loading functions
 *
 * Copyright 2026 Leon Lynch
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

#include "emv_config_xml.h"
#include "emv.h"

#include <libxml/parser.h>

int emv_config_xml_load(struct emv_ctx_t* ctx, const char* filename)
{
	if (!ctx || !filename) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	return EMV_ERROR_INTERNAL;
}

int emv_config_xml_load_buf(struct emv_ctx_t* ctx, const void* buf, size_t len)
{
	if (!ctx || !buf || !len) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	return EMV_ERROR_INTERNAL;
}
