/**
 * @file emv_config.c
 * @brief EMV configuration abstraction and helper functions
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

#include "emv_config.h"
#include "emv.h"
#include "emv_tlv.h"

#include <stddef.h>

int emv_config_clear(struct emv_config_t* config)
{
	if (!config) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	emv_tlv_list_clear(&config->data);

	return 0;
}

int emv_config_data_set(
	struct emv_ctx_t* ctx,
	unsigned int tag,
	unsigned int length,
	const uint8_t* value
)
{
	int r;

	if (!ctx) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	r = emv_tlv_list_push(
		&ctx->config.data,
		tag,
		length,
		value,
		0
	);
	if (r) {
		return EMV_ERROR_INTERNAL;
	}

	return 0;
}

int emv_config_data_set_asn1_object(
	struct emv_ctx_t* ctx,
	const struct iso8825_oid_t* oid,
	unsigned int ber_length,
	const uint8_t* ber_bytes
)
{
	int r;

	if (!ctx) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	r = emv_tlv_list_push_asn1_object(
		&ctx->config.data,
		oid,
		ber_length,
		ber_bytes
	);
	if (r) {
		return EMV_ERROR_INTERNAL;
	}

	return 0;
}

const struct emv_tlv_t* emv_config_data_get(
	const struct emv_ctx_t* ctx,
	unsigned int tag
)
{
	if (!ctx) {
		return NULL;
	}

	return emv_tlv_list_find_const(&ctx->config.data, tag);
}
