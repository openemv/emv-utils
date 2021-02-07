/**
 * @file pcsc.c
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

#include "pcsc.h"

#include <pcsclite.h>
#include <winscard.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct pcsc_t {
	SCARDCONTEXT context;

	DWORD reader_strings_size;
	LPSTR reader_strings;

	size_t reader_count;
	struct pcsc_reader_t* readers;
};

struct pcsc_reader_t {
	struct pcsc_t* pcsc;
	LPCSTR name;
};

int pcsc_init(pcsc_ctx_t* ctx)
{
	struct pcsc_t* pcsc;
	LONG result;
	LPSTR current_reader_name;

	if (!ctx) {
		return -1;
	}

	*ctx = malloc(sizeof(struct pcsc_t));
	pcsc = *ctx;
	memset(pcsc, 0, sizeof(*pcsc));

	// Create the PC/SC context
	result = SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &pcsc->context);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardEstablishContext() failed; result=0x%lx [%s]\n", result, pcsc_stringify_error(result));
		pcsc_release(ctx);
		return -1;
	}

	// Get the size of the reader list
	result = SCardListReaders(pcsc->context, NULL, NULL, &pcsc->reader_strings_size);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardListReaders() failed; result=0x%lx [%s]\n", result, pcsc_stringify_error(result));
		pcsc_release(ctx);
		return -1;
	}

	// Allocate and retrieve the reader list
	pcsc->reader_strings = malloc(pcsc->reader_strings_size);
	result = SCardListReaders(pcsc->context, NULL, pcsc->reader_strings, &pcsc->reader_strings_size);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardListReaders() failed; result=0x%lx [%s]\n", result, pcsc_stringify_error(result));
		pcsc_release(ctx);
		return -1;
	}

	// Parse and count readers
	current_reader_name = pcsc->reader_strings;
	pcsc->reader_count = 0;
	while (*current_reader_name) {
		current_reader_name += strlen(current_reader_name) + 1;
		pcsc->reader_count++;
	}

	// Allocate and populate reader objects
	pcsc->readers = calloc(pcsc->reader_count, sizeof(struct pcsc_reader_t));
	memset(pcsc->readers, 0, pcsc->reader_count * sizeof(struct pcsc_reader_t));
	current_reader_name = pcsc->reader_strings;
	for (size_t i = 0; i < pcsc->reader_count; ++i) {
		pcsc->readers[i].pcsc = pcsc;
		pcsc->readers[i].name = current_reader_name;
		current_reader_name += strlen(current_reader_name) + 1;
	}

	// Success
	return 0;
}

void pcsc_release(pcsc_ctx_t* ctx)
{
	struct pcsc_t* pcsc;
	LONG result;

	if (!ctx) {
		return;
	}
	if (!*ctx) {
		return;
	}
	pcsc = *ctx;

	// Release the PC/SC context
	result = SCardReleaseContext(pcsc->context);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardReleaseContext() failed; result=0x%lx [%s]\n", result, pcsc_stringify_error(result));
	}

	// Cleanup the PC/SC variables
	if (pcsc->reader_strings) {
		free(pcsc->reader_strings);
		pcsc->reader_strings = NULL;
	}
	if (pcsc->readers) {
		free(pcsc->readers);
		pcsc->readers = NULL;
	}
	free(pcsc);
	*ctx = NULL;
}

size_t pcsc_get_reader_count(pcsc_ctx_t ctx)
{
	struct pcsc_t* pcsc;

	if (!ctx) {
		return 0;
	}
	pcsc = ctx;

	return pcsc->reader_count;
}

pcsc_reader_ctx_t pcsc_get_reader(pcsc_ctx_t ctx, size_t idx)
{
	struct pcsc_t* pcsc;

	if (!ctx) {
		return NULL;
	}
	pcsc = ctx;

	if (idx >= pcsc->reader_count) {
		return NULL;
	}

	return &pcsc->readers[idx];
}

const char* pcsc_reader_get_name(pcsc_reader_ctx_t reader_ctx)
{
	struct pcsc_reader_t* reader;

	if (!reader_ctx) {
		return NULL;
	}
	reader = reader_ctx;

	return reader->name;
}

int pcsc_reader_get_state(pcsc_reader_ctx_t reader_ctx, unsigned int* state)
{
	struct pcsc_reader_t* reader;
	LONG result;
	SCARD_READERSTATE reader_state;

	if (!reader_ctx) {
		return -1;
	}
	if (!state) {
		return -1;
	}
	reader = reader_ctx;

	memset(&reader_state, 0, sizeof(reader_state));
	reader_state.szReader = reader->name;
	reader_state.dwCurrentState = SCARD_STATE_UNAWARE;

	result = SCardGetStatusChange(
		reader->pcsc->context,
		INFINITE,
		&reader_state,
		1
	);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardGetStatusChange() failed; result=0x%lx [%s]\n", result, pcsc_stringify_error(result));
		return -1;
	}

	*state = reader_state.dwEventState;

	return 0;
}
