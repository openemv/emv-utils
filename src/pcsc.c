/**
 * @file pcsc.c
 * @brief PC/SC abstraction
 *
 * Copyright (c) 2021, 2022 Leon Lynch
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

#include <winscard.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef USE_PCSCLITE
// Only PCSCLite provides pcsc_stringify_error()
static char* pcsc_stringify_error(unsigned int result)
{
	static char str[16];
	snprintf(str, sizeof(str), "0x%08X", result);
	return str;
}
#endif

struct pcsc_t {
	SCARDCONTEXT context;

	DWORD reader_strings_size;
	LPSTR reader_strings;

	size_t reader_count;
	struct pcsc_reader_t* readers;
	SCARD_READERSTATE* reader_states;
};

struct pcsc_reader_t {
	struct pcsc_t* pcsc;
	LPCSTR name;

	SCARDHANDLE card;
	BYTE atr[PCSC_MAX_ATR_SIZE];
	DWORD atr_len;
	DWORD protocol;
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

	// Allocate and populate reader states
	pcsc->reader_states = calloc(pcsc->reader_count, sizeof(SCARD_READERSTATE));
	memset(pcsc->reader_states, 0, pcsc->reader_count * sizeof(SCARD_READERSTATE));
	for (size_t i = 0; i < pcsc->reader_count; ++i) {
		pcsc->reader_states[i].szReader = pcsc->readers[i].name;
		pcsc->reader_states[i].pvUserData = &pcsc->readers[i];
		pcsc->reader_states[i].dwCurrentState = SCARD_STATE_UNAWARE;
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
	if (pcsc->reader_states) {
		free(pcsc->reader_states);
		pcsc->reader_states = NULL;
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

int pcsc_wait_for_card(pcsc_ctx_t ctx, unsigned long timeout_ms, size_t* idx)
{
	struct pcsc_t* pcsc;
	LONG result;

	if (!ctx) {
		return -1;
	}
	pcsc = ctx;

	// Prepare reader states for card detection
	for (size_t i = 0; i < pcsc->reader_count; ++i) {
		pcsc->reader_states[i].dwCurrentState = SCARD_STATE_EMPTY;
	}

	// Wait for empty state to change
	result = SCardGetStatusChange(
		pcsc->context,
		timeout_ms,
		pcsc->reader_states,
		pcsc->reader_count
	);
	if (result == SCARD_E_TIMEOUT) {
		// Timeout
		return 1;
	}
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardGetStatusChange() failed; result=0x%lx [%s]\n", result, pcsc_stringify_error(result));
		return -1;
	}

	// Find first reader with card
	for (size_t i = 0; i < pcsc->reader_count; ++i) {
		if (pcsc->reader_states[i].dwEventState & SCARD_STATE_PRESENT) {
			*idx = i;
			return 0;
		}
	}

	// No cards detected
	*idx = -1;
	return 1;
}

int pcsc_reader_connect(pcsc_reader_ctx_t reader_ctx)
{
	struct pcsc_reader_t* reader;
	LONG result;
	DWORD state;

	if (!reader_ctx) {
		return -1;
	}
	reader = reader_ctx;

	// Connect to reader and power up the card
	result = SCardConnect(
		reader->pcsc->context,
		reader->name,
		SCARD_SHARE_EXCLUSIVE,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
		&reader->card,
		&reader->protocol
	);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardConnect() failed; result=0x%lx [%s]\n", result, pcsc_stringify_error(result));
		return -1;
	}

	state = 0;
	reader->atr_len = sizeof(reader->atr);
	result = SCardStatus(reader->card, NULL, NULL, &state, &reader->protocol, reader->atr, &reader->atr_len);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardStatus() failed; result=0x%lx [%s]\n", result, pcsc_stringify_error(result));
		pcsc_reader_disconnect(reader);
		return -1;
	}

	return 0;
}

int pcsc_reader_disconnect(pcsc_reader_ctx_t reader_ctx)
{
	struct pcsc_reader_t* reader;
	LONG result;

	if (!reader_ctx) {
		return -1;
	}
	reader = reader_ctx;

	// Disconnect from reader and unpower card
	result = SCardDisconnect(reader->card, SCARD_UNPOWER_CARD);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardDisconnect() failed; result=0x%lx [%s]\n", result, pcsc_stringify_error(result));
		return -1;
	}

	// Clear card attributes
	reader->card = 0;
	memset(reader->atr, 0, sizeof(reader->atr));
	reader->atr_len = 0;
	reader->protocol = SCARD_PROTOCOL_UNDEFINED;

	return 0;
}

int pcsc_reader_get_atr(pcsc_reader_ctx_t reader_ctx, uint8_t* atr, size_t* atr_len)
{
	struct pcsc_reader_t* reader;

	if (!reader_ctx || !atr || !atr_len) {
		return -1;
	}
	reader = reader_ctx;

	if (!reader->atr_len) {
		return -1;
	}

	memcpy(atr, reader->atr, reader->atr_len);
	*atr_len = reader->atr_len;

	return 0;
}

int pcsc_reader_trx(
	pcsc_reader_ctx_t reader_ctx,
	const void* tx_buf,
	size_t tx_buf_len,
	void* rx_buf,
	size_t* rx_buf_len
)
{
	struct pcsc_reader_t* reader;
	LONG result;
	DWORD rx_len;

	if (!reader_ctx || !tx_buf || !tx_buf_len || !rx_buf || !rx_buf_len) {
		return -1;
	}
	reader = reader_ctx;
	rx_len = *rx_buf_len;

	switch (reader->protocol) {
		case SCARD_PROTOCOL_T0:
			result = SCardTransmit(reader->card, SCARD_PCI_T0, tx_buf, tx_buf_len, NULL, rx_buf, &rx_len);
			break;

		case SCARD_PROTOCOL_T1:
			result = SCardTransmit(reader->card, SCARD_PCI_T1, tx_buf, tx_buf_len, NULL, rx_buf, &rx_len);
			break;
	}

	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardTransmit() failed; result=0x%lx [%s]\n", result, pcsc_stringify_error(result));
		return -1;
	}

	*rx_buf_len = rx_len;

	return 0;
}
