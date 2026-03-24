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
#include "emv_fields.h"

#include <stddef.h>
#include <stdlib.h> // For malloc() and free()
#include <stdint.h>
#include <string.h>

static void emv_config_app_list_clear(struct emv_config_app_t** list)
{
	if (!list || !*list) {
		return;
	}

	while (*list != NULL) {
		struct emv_config_app_t* app = *list;
		*list = app->next;

		emv_tlv_list_clear(&app->data);
		free(app);
	}
}

static int emv_config_app_list_push(
	struct emv_config_app_t** list,
	struct emv_config_app_t* app
)
{
	struct emv_config_app_t* itr;

	if (!list) {
		return -1;
	}
	if (!app) {
		return -2;
	}
	if (app->next) {
		return -3;
	}

	if (!*list) {
		// Only app in list
		*list = app;
		return 0;
	}

	itr = *list;
	while (itr->next) {
		itr = itr->next;
	}

	// Last app in list
	itr->next = app;
	return 0;
}

int emv_config_clear(struct emv_config_t* config)
{
	if (!config) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	emv_tlv_list_clear(&config->data);
	emv_config_app_list_clear(&config->supported_apps);

	return 0;
}

int emv_config_data_set(
	struct emv_ctx_t* ctx,
	struct emv_tlv_list_t* data
)
{
	int r;

	if (!ctx || !data) {
		return EMV_ERROR_INVALID_PARAMETER;
	}
	if (emv_tlv_list_is_empty(data)) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	if (emv_tlv_list_has_duplicate(data)) {
		return EMV_ERROR_INVALID_CONFIG;
	}

	emv_tlv_list_clear(&ctx->config.data);
	r = emv_tlv_list_append(&ctx->config.data, data);
	if (r) {
		return EMV_ERROR_INTERNAL;
	}

	return 0;
}

int emv_config_app_create(
	struct emv_ctx_t* ctx,
	const void* aid,
	unsigned int aid_len,
	uint8_t asi,
	struct emv_tlv_list_t* data,
	struct emv_config_app_t** app
)
{
	int r;
	struct emv_config_app_t* tmp;

	if (!ctx) {
		return EMV_ERROR_INVALID_PARAMETER;
	}
	if (!aid || aid_len < 5 || aid_len > 16) {
		return EMV_ERROR_INVALID_PARAMETER;
	}
	if (data && emv_tlv_list_has_duplicate(data)) {
		return EMV_ERROR_INVALID_CONFIG;
	}
	if (app) {
		*app = NULL;
	}

	tmp = malloc(sizeof(*tmp));
	if (!tmp) {
		return EMV_ERROR_INTERNAL;
	}
	memset(tmp, 0, sizeof(*tmp));

	memcpy(tmp->aid, aid, aid_len);
	tmp->aid_len = aid_len;
	tmp->asi = asi;

	r = emv_config_app_list_push(&ctx->config.supported_apps, tmp);
	if (r) {
		free(tmp);
		return EMV_ERROR_INTERNAL;
	}
	if (data) {
		r = emv_tlv_list_append(&tmp->data, data);
		if (r) {
			return EMV_ERROR_INTERNAL;
		}
	}
	if (app) {
		*app = tmp;
	}

	// HACK: For backward compatibility until supported_aids has been removed
	emv_tlv_list_push(&ctx->supported_aids, 0x9F06, aid_len, aid, asi);

	return 0;
}

int emv_config_app_set_enable(struct emv_config_app_t* app, bool enabled)
{
	if (!app) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	if (enabled) {
		app->asi &= ~EMV_ASI_DISABLED;
	} else {
		app->asi |= EMV_ASI_DISABLED;
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
