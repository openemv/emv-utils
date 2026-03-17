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
#include <string.h>

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
	struct emv_tlv_t* tlv;

	if (!ctx) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	tlv = emv_tlv_list_find(&ctx->config.data, tag);
	if (tlv) {
		// Overwrite existing field
		r = emv_tlv_update_value(tlv, length, value);
	} else {
		// Add new field
		r = emv_tlv_list_push(
			&ctx->config.data,
			tag,
			length,
			value,
			0
		);
	}
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
	if (!oid ||
		oid->length < 2 ||
		oid->length > sizeof(oid->value) / sizeof(oid->value[0])
	) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	// Find existing ASN.1 object with matching OID
	for (struct emv_tlv_t* tlv = ctx->config.data.front; tlv != NULL; tlv = tlv->next) {
		struct iso8825_oid_t cur_oid;

		r = iso8825_ber_asn1_object_decode(&tlv->ber, &cur_oid);
		if (r <= 0) {
			// Not an ASN.1 object
			continue;
		}

		if (cur_oid.length != oid->length ||
			memcmp(cur_oid.value, oid->value, sizeof(oid->value[0]) * oid->length) != 0
		) {
			// ASN.1 OID does not match
			continue;
		}

		// Remove matching ASN.1 OID object
		r = emv_tlv_list_remove(&ctx->config.data, tlv);
		if (r) {
			return EMV_ERROR_INTERNAL;
		}

		break;
	}

	// Add new ASN.1 object
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
