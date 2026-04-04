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
#include "emv_app.h"

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
	if (data) {
		r = emv_tlv_list_append(&tmp->data, data);
		if (r) {
			r = EMV_ERROR_INTERNAL;
			goto error;
		}
	}

	r = emv_config_app_list_push(&ctx->config.supported_apps, tmp);
	if (r) {
		r = EMV_ERROR_INTERNAL;
		goto error;
	}
	if (app) {
		*app = tmp;
	}

	return 0;

error:
	emv_tlv_list_clear(&tmp->data);
	free(tmp);
	tmp = NULL;
	return r;
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

int emv_config_app_itr_init(
	const struct emv_config_t* config,
	struct emv_config_app_itr_t* itr
)
{
	if (!config) {
		return -1;
	}
	if (!itr) {
		return -2;
	}

	memset(itr, 0, sizeof(*itr));
	itr->app = config->supported_apps;

	return 0;
}

const struct emv_config_app_t* emv_config_app_itr_next(
	struct emv_config_app_itr_t* itr
)
{
	const struct emv_config_app_t* app;

	if (!itr || !itr->app) {
		return NULL;
	}

	while ((app = itr->app) != NULL) {
		itr->app = itr->app->next;

		if ((app->asi & EMV_ASI_DISABLED) != 0) {
			// Skip disabled EMV application configuration
			continue;
		}

		break;
	}

	return app;
}

bool emv_config_app_is_supported(
	const struct emv_config_t* config,
	const struct emv_app_t* app
)
{
	int r;
	struct emv_config_app_itr_t itr;
	const struct emv_config_app_t* config_app;

	if (!config || !config->supported_apps) {
		// Invalid EMV configuration; not supported
		return false;
	}

	if (!app || !app->aid) {
		// Invalid app; not supported
		return false;
	}

	r = emv_config_app_itr_init(config, &itr);
	if (r) {
		// Internal error
		return false;
	}

	// See EMV 4.4 Book 1, 12.3.1
	while ((config_app = emv_config_app_itr_next(&itr)) != NULL) {

		if (config_app->asi == EMV_ASI_EXACT_MATCH &&
			config_app->aid_len == app->aid->length &&
			memcmp(config_app->aid, app->aid->value, config_app->aid_len) == 0
		) {
			// Exact match found; supported
			return true;
		}

		if (config_app->asi == EMV_ASI_PARTIAL_MATCH &&
			config_app->aid_len <= app->aid->length &&
			memcmp(config_app->aid, app->aid->value, config_app->aid_len) == 0
		) {
			// Partial match found; supported
			return true;
		}
	}

	return false;
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
