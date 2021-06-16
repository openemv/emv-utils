/**
 * @file emv_tlv.c
 * @brief EMV TLV structures and helper functions
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

#include "emv_tlv.h"
#include "iso8825_ber.h"

#include <stdbool.h>
#include <stdlib.h> // for malloc() and free()
#include <string.h>
#include <assert.h>

// Helper functions
static inline bool emv_tlv_list_is_valid(const struct emv_tlv_list_t* list);
static struct emv_tlv_t* emv_tlv_alloc(unsigned int tag, unsigned int length, const uint8_t* value, uint8_t flags);

static inline bool emv_tlv_list_is_valid(const struct emv_tlv_list_t* list)
{
	if (!list) {
		return false;
	}

	if (list->front && !list->back) {
		return false;
	}

	if (!list->front && list->back) {
		return false;
	}

	return true;
}

static struct emv_tlv_t* emv_tlv_alloc(unsigned int tag, unsigned int length, const uint8_t* value, uint8_t flags)
{
	struct emv_tlv_t* tlv;

	tlv = malloc(sizeof(*tlv));
	if (!tlv) {
		return NULL;
	}

	tlv->tag = tag;
	tlv->length = length;
	tlv->value = malloc(length);
	if (!tlv->value) {
		free(tlv);
		return NULL;
	}
	memcpy(tlv->value, value, length);
	tlv->flags = flags;
	tlv->next = NULL;

	return tlv;
}

int emv_tlv_free(struct emv_tlv_t* tlv)
{
	if (!tlv) {
		return -1;
	}
	if (tlv->next) {
		// EMV TLV field is part of a list; unsafe to free
		return 1;
	}

	if (tlv->value) {
		free(tlv->value);
		tlv->value = NULL;
	}
	free(tlv);

	return 0;
}

bool emv_tlv_list_is_empty(const struct emv_tlv_list_t* list)
{
	if (!emv_tlv_list_is_valid(list)) {
		// Indicate that the list is empty to dissuade the caller from
		// attempting to access it
		return true;
	}

	return !list->front;
}

void emv_tlv_list_clear(struct emv_tlv_list_t* list)
{
	if (!emv_tlv_list_is_valid(list)) {
		list->front = NULL;
		list->back = NULL;
		return;
	}

	while (list->front) {
		struct emv_tlv_t* tlv;
		int r;
		int emv_tlv_is_safe_to_free;

		tlv = emv_tlv_list_pop(list);
		r = emv_tlv_free(tlv);

		emv_tlv_is_safe_to_free = r;
		assert(emv_tlv_is_safe_to_free == 0);
	}
	assert(list->front == NULL);
	assert(list->back == NULL);
}

int emv_tlv_list_push(
	struct emv_tlv_list_t* list,
	unsigned int tag,
	unsigned int length,
	const uint8_t* value,
	uint8_t flags
)
{
	struct emv_tlv_t* tlv;

	if (!emv_tlv_list_is_valid(list)) {
		return -1;
	}

	tlv = emv_tlv_alloc(tag, length, value, flags);
	if (!tlv) {
		return -2;
	}

	if (list->back) {
		list->back->next = tlv;
		list->back = tlv;
	} else {
		list->front = tlv;
		list->back = tlv;
	}

	return 0;
}

struct emv_tlv_t* emv_tlv_list_pop(struct emv_tlv_list_t* list)
{
	struct emv_tlv_t* tlv = NULL;

	if (!emv_tlv_list_is_valid(list)) {
		return NULL;
	}

	if (list->front) {
		tlv = list->front;
		list->front = tlv->next;
		if (!list->front) {
			list->back = NULL;
		}

		tlv->next = NULL;
	}

	return tlv;
}

struct emv_tlv_t* emv_tlv_list_find(struct emv_tlv_list_t* list, unsigned int tag)
{
	struct emv_tlv_t* tlv = NULL;

	if (!emv_tlv_list_is_valid(list)) {
		return NULL;
	}

	for (tlv = list->front; tlv != NULL; tlv = tlv->next) {
		if (tlv->tag == tag) {
			return tlv;
		}
	}

	return NULL;
}

int emv_tlv_parse(const void* ptr, size_t len, struct emv_tlv_list_t* list)
{
	int r;
	struct iso8825_ber_itr_t itr;
	struct iso8825_tlv_t tlv;

	r = iso8825_ber_itr_init(ptr, len, &itr);
	if (r) {
		return -1;
	}

	while ((r = iso8825_ber_itr_next(&itr, &tlv)) > 0) {
		if (iso8825_ber_is_constructed(&tlv)) {
			// Recurse into constructed/template field but omit it from the list
			emv_tlv_parse(tlv.value, tlv.length, list);
		} else {
			r = emv_tlv_list_push(list, tlv.tag, tlv.length, tlv.value, 0);
			if (r) {
				return -2;
			}
		}
	}

	if (r < 0) {
		// BER decoding error
		return 1;
	}

	return 0;
}
