/**
 * @file pcsc.c
 * @brief PC/SC abstraction
 *
 * Copyright (c) 2021-2022, 2024 Leon Lynch
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
#ifdef USE_PCSCLITE
// Only PCSCLite provides reader.h
#include <reader.h>
#endif
// Compatibility helpers must not conflict with winscard.h or reader.h
#include "pcsc_compat.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// For ntohl and friends
#if defined(HAVE_ARPA_INET_H)
#include <arpa/inet.h>
#elif defined(HAVE_WINSOCK_H)
#include <winsock.h>
#endif

struct pcsc_t {
	SCARDCONTEXT context;

	DWORD reader_strings_size;
	LPSTR reader_strings;

	size_t reader_count;
	struct pcsc_reader_t* readers;
	SCARD_READERSTATE* reader_states;
};

struct pcsc_reader_features_t {
	// Populated by SCardControl(CM_IOCTL_GET_FEATURE_REQUEST)
	uint8_t buf[PCSC_MAX_BUFFER_SIZE];
	DWORD buf_len;

	// Populated by SCardControl(IFD_PIN_PROPERTIES)
	PIN_PROPERTIES_STRUCTURE pin_properties;
	DWORD pin_properties_len;

	// Populated by SCardControl(IFD_DISPLAY_PROPERTIES)
	DISPLAY_PROPERTIES_STRUCTURE display_properties;
	DWORD display_properties_len;

	// Populated by SCardControl(GET_TLV_PROPERTIES)
	uint8_t properties[PCSC_MAX_BUFFER_SIZE];
	DWORD properties_len;
};

struct pcsc_reader_t {
	// Populated by pcsc_init()
	struct pcsc_t* pcsc;
	LPCSTR name;

	// Populated by pcsc_reader_populate_features()
	struct pcsc_reader_features_t features;

	// Populated by SCardConnect()
	SCARDHANDLE card;
	DWORD protocol;

	// Populated by SCardStatus()
	BYTE atr[PCSC_MAX_ATR_SIZE];
	DWORD atr_len;

	// Populated by pcsc_reader_connect()
	uint8_t uid[10]; // See PC/SC Part 3 Rev 2.01.09, 3.2.2.1.3
	size_t uid_len;
	enum pcsc_card_type_t type;
};

// Helper functions
static int pcsc_reader_populate_features(struct pcsc_reader_t* reader);
static int pcsc_reader_get_feature(struct pcsc_reader_t* reader, unsigned int feature, LPDWORD control_code);
static int pcsc_reader_internal_get_uid(pcsc_reader_ctx_t reader_ctx, uint8_t* uid, size_t* uid_len);

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
		fprintf(stderr, "SCardEstablishContext() failed; result=0x%x [%s]\n", (unsigned int)result, pcsc_stringify_error(result));
		pcsc_release(ctx);
		return -1;
	}

	// Get the size of the reader list
	result = SCardListReaders(pcsc->context, NULL, NULL, &pcsc->reader_strings_size);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardListReaders() failed; result=0x%x [%s]\n", (unsigned int)result, pcsc_stringify_error(result));
		pcsc_release(ctx);
		return -1;
	}

	// Allocate and retrieve the reader list
	pcsc->reader_strings = malloc(pcsc->reader_strings_size);
	result = SCardListReaders(pcsc->context, NULL, pcsc->reader_strings, &pcsc->reader_strings_size);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardListReaders() failed; result=0x%x [%s]\n", (unsigned int)result, pcsc_stringify_error(result));
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

	// Populate features for each reader
	for (size_t i = 0; i < pcsc->reader_count; ++i) {
		// Intentionally ignore errors
		pcsc_reader_populate_features(&pcsc->readers[i]);
	}

	// Success
	return 0;
}

static int pcsc_reader_populate_features(struct pcsc_reader_t* reader)
{
	int r;
	LONG result;
	SCARDHANDLE card;
	DWORD protocol;
	const PCSC_TLV_STRUCTURE* pcsc_tlv;
	DWORD control_code;

	if (!reader) {
		return -1;
	}

	// Connect without negotiating the card protocol because the card
	// may not be present yet
	result = SCardConnect(
		reader->pcsc->context,
		reader->name,
		SCARD_SHARE_DIRECT,
		0,
		&card,
		&protocol
	);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardConnect(SCARD_SHARE_DIRECT) failed; result=0x%x [%s]\n", (unsigned int)result, pcsc_stringify_error(result));
		r = -2;
		goto exit;
	}

	// Request reader features
	result = SCardControl(
		card,
		CM_IOCTL_GET_FEATURE_REQUEST,
		NULL,
		0,
		reader->features.buf,
		sizeof(reader->features.buf),
		&reader->features.buf_len
	);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardControl(CM_IOCTL_GET_FEATURE_REQUEST) failed; result=0x%x [%s]\n", (unsigned int)result, pcsc_stringify_error(result));
		r = -3;
		goto exit;
	}

	// Validate feature list buffer length
	if (reader->features.buf_len % sizeof(PCSC_TLV_STRUCTURE) != 0) {
		fprintf(stderr, "Failed to parse PC/SC reader features\n");
		reader->features.buf_len = 0; // Invalidate buffer length
		r = -4;
		goto exit;
	}

	// Validate feature list TLV lengths
	// See PC/SC Part 10 Rev 2.02.09, 2.2
	pcsc_tlv = (PCSC_TLV_STRUCTURE*)reader->features.buf;
	while ((void*)pcsc_tlv - (void*)reader->features.buf < reader->features.buf_len) {
		if (pcsc_tlv->length != 4) {
			fprintf(stderr, "Failed to parse PC/SC reader features\n");
			r = -5;
			goto exit;
		}
		++pcsc_tlv;
	}

	r = pcsc_reader_get_feature(reader, PCSC_FEATURE_IFD_PIN_PROPERTIES, &control_code);
	if (r < 0) {
		r = -6;
		goto exit;
	}
	if (r == 0) {
		// Request reader TLV properties
		result = SCardControl(
			card,
			control_code,
			NULL,
			0,
			&reader->features.pin_properties,
			sizeof(reader->features.pin_properties),
			&reader->features.pin_properties_len
		);
		if (result != SCARD_S_SUCCESS) {
			fprintf(stderr, "SCardControl(%04X) failed; result=0x%x [%s]\n", (unsigned int)control_code, (unsigned int)result, pcsc_stringify_error(result));
			r = -7;
			goto exit;
		}
		if (reader->features.pin_properties_len != sizeof(reader->features.pin_properties)) {
			fprintf(stderr, "Failed to parse PC/SC reader IFD PIN properties\n");
			reader->features.pin_properties_len = 0; // Invalidate buffer length
			r = -8;
			goto exit;
		}
	}

	r = pcsc_reader_get_feature(reader, PCSC_FEATURE_IFD_DISPLAY_PROPERTIES, &control_code);
	if (r < 0) {
		r = -9;
		goto exit;
	}
	if (r == 0) {
		// Request reader TLV properties
		result = SCardControl(
			card,
			control_code,
			NULL,
			0,
			&reader->features.display_properties,
			sizeof(reader->features.display_properties),
			&reader->features.display_properties_len
		);
		if (result != SCARD_S_SUCCESS) {
			fprintf(stderr, "SCardControl(%04X) failed; result=0x%x [%s]\n", (unsigned int)control_code, (unsigned int)result, pcsc_stringify_error(result));
			r = -10;
			goto exit;
		}
		if (reader->features.display_properties_len != sizeof(reader->features.display_properties)) {
			fprintf(stderr, "Failed to parse PC/SC reader IFD display properties\n");
			reader->features.display_properties_len = 0; // Invalidate buffer length
			r = -11;
			goto exit;
		}
	}

	r = pcsc_reader_get_feature(reader, PCSC_FEATURE_GET_TLV_PROPERTIES, &control_code);
	if (r < 0) {
		r = -12;
		goto exit;
	}
	if (r == 0) {
		// Request reader TLV properties
		result = SCardControl(
			card,
			control_code,
			NULL,
			0,
			reader->features.properties,
			sizeof(reader->features.properties),
			&reader->features.properties_len
		);
		if (result != SCARD_S_SUCCESS) {
			fprintf(stderr, "SCardControl(%04X) failed; result=0x%x [%s]\n", (unsigned int)control_code, (unsigned int)result, pcsc_stringify_error(result));
			r = -13;
			goto exit;
		}
	}

	// Success
	r = 0;
	goto exit;

exit:
	// Disconnect from card reader and leave card as-is if present
	result = SCardDisconnect(card, SCARD_LEAVE_CARD);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardDisconnect(SCARD_LEAVE_CARD) failed; result=0x%x [%s]\n", (unsigned int)result, pcsc_stringify_error(result));
		// Intentionally ignore errors
	}

	return r;
}

static int pcsc_reader_get_feature(
	struct pcsc_reader_t* reader,
	unsigned int feature,
	LPDWORD control_code
)
{
	const PCSC_TLV_STRUCTURE* pcsc_tlv;

	if (!reader) {
		return -1;
	}

	if (!reader->features.buf_len) {
		return 1;
	}

	pcsc_tlv = (PCSC_TLV_STRUCTURE*)reader->features.buf;
	while ((void*)pcsc_tlv - (void*)reader->features.buf < reader->features.buf_len) {
		if (pcsc_tlv->tag == feature && pcsc_tlv->length == 4) {
			*control_code = ntohl(pcsc_tlv->value);
			return 0;
		}
		++pcsc_tlv;
	}

	return 1;
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
		fprintf(stderr, "SCardReleaseContext() failed; result=0x%x [%s]\n", (unsigned int)result, pcsc_stringify_error(result));
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

bool pcsc_reader_has_feature(pcsc_reader_ctx_t reader_ctx, unsigned int feature)
{
	struct pcsc_reader_t* reader;
	const PCSC_TLV_STRUCTURE* pcsc_tlv;

	if (!reader_ctx) {
		return NULL;
	}
	reader = reader_ctx;

	if (!reader->features.buf_len) {
		return false;
	}

	pcsc_tlv = (PCSC_TLV_STRUCTURE*)reader->features.buf;
	while ((void*)pcsc_tlv - (void*)reader->features.buf < reader->features.buf_len) {
		if (pcsc_tlv->tag == feature) {
			return true;
		}
		++pcsc_tlv;
	}

	return false;
}

int pcsc_reader_get_property(
	pcsc_reader_ctx_t reader_ctx,
	unsigned int property,
	void* value,
	size_t* value_len
)
{
	struct pcsc_reader_t* reader;
	const uint8_t* tlv;
	size_t tlv_len;

	if (!reader_ctx || !value || !value_len) {
		return -1;
	}
	reader = reader_ctx;

	// Retrieve from GET_TLV_PROPERTIES
	// See PC/SC Part 10 Rev 2.02.09, 2.6.14
	tlv = reader->features.properties;
	tlv_len = reader->features.properties_len;
	while (tlv_len >= 2) { // 1-byte tag and 1-byte length
		if (tlv[0] == property) {
			if (tlv[1] > tlv_len - 2) {
				*value_len = 0;
				return -2;
			}
			if (tlv[1] > *value_len) {
				*value_len = 0;
				return -3;
			}

			memcpy(value, tlv + 2, tlv[1]);
			*value_len = tlv[1];
			return 0;
		}

		tlv += (2 + tlv[1]);
		tlv_len -= (2 + tlv[1]);
	}

	// Otherwise, retrieve from IFD_PIN_PROPERTIES
	// See PC/SC Part 10 Rev 2.02.09, 2.5.5
	if (reader->features.pin_properties_len) {
		if (property == PCSC_PROPERTY_wLcdLayout) {
			if (*value_len < sizeof(reader->features.pin_properties.wLcdLayout)) {
				*value_len = 0;
				return -4;
			}

			memcpy(
				value,
				&reader->features.pin_properties.wLcdLayout,
				sizeof(reader->features.pin_properties.wLcdLayout)
			);
			*value_len = sizeof(reader->features.pin_properties.wLcdLayout);
			return 0;
		}
	}

	// Otherwise, retrieve from IFD_DISPLAY_PROPERTIES
	// See PC/SC Part 10 Rev 2.02.09, 2.5.6
	if (reader->features.display_properties_len) {
		if (property == PCSC_PROPERTY_wLcdMaxCharacters) {
			if (*value_len < sizeof(reader->features.display_properties.wLcdMaxCharacters)) {
				*value_len = 0;
				return -5;
			}

			memcpy(
				value,
				&reader->features.display_properties.wLcdMaxCharacters,
				sizeof(reader->features.display_properties.wLcdMaxCharacters)
			);
			*value_len = sizeof(reader->features.display_properties.wLcdMaxCharacters);
			return 0;
		}

		if (property == PCSC_PROPERTY_wLcdMaxLines) {
			if (*value_len < sizeof(reader->features.display_properties.wLcdMaxLines)) {
				*value_len = 0;
				return -6;
			}

			memcpy(
				value,
				&reader->features.display_properties.wLcdMaxLines,
				sizeof(reader->features.display_properties.wLcdMaxLines)
			);
			*value_len = sizeof(reader->features.display_properties.wLcdMaxLines);
			return 0;
		}
	}

	// Property not found
	*value_len = 0;
	return 1;
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
		fprintf(stderr, "SCardGetStatusChange() failed; result=0x%x [%s]\n", (unsigned int)result, pcsc_stringify_error(result));
		return -1;
	}

	*state = reader_state.dwEventState;

	return 0;
}

int pcsc_wait_for_card(pcsc_ctx_t ctx, unsigned long timeout_ms, size_t* idx)
{
	struct pcsc_t* pcsc;
	LONG result;

	if (!ctx || !idx) {
		return -1;
	}
	pcsc = ctx;

	// Prepare reader states for card detection
	for (size_t i = 0; i < pcsc->reader_count; ++i) {
		if (*idx == PCSC_READER_ANY || *idx == i) {
			pcsc->reader_states[i].dwCurrentState = SCARD_STATE_EMPTY;
		} else {
			pcsc->reader_states[i].dwCurrentState = SCARD_STATE_IGNORE;
		}
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
		fprintf(stderr, "SCardGetStatusChange() failed; result=0x%x [%s]\n", (unsigned int)result, pcsc_stringify_error(result));
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

static int pcsc_reader_internal_get_uid(pcsc_reader_ctx_t reader_ctx, uint8_t* uid, size_t* uid_len)
{
	int r;
	uint8_t rx_buf[12]; // ISO 14443 Type A triple size UID + status bytes
	size_t rx_len = sizeof(rx_buf);
	uint8_t SW1;
	uint8_t SW2;

	// PC/SC GET DATA command for requesting contactless UID
	// See PC/SC Part 3 Rev 2.01.09, 3.2.2.1.3
	static const uint8_t pcsc_get_uid_capdu[] = { 0xFF, 0xCA, 0x00, 0x00, 0x00 };

	r = pcsc_reader_trx(reader_ctx, pcsc_get_uid_capdu, sizeof(pcsc_get_uid_capdu), rx_buf, &rx_len);
	if (r) {
		return r;
	}
	if (rx_len < 2) {
		// Invalid respones length
		return -5;
	}

	// Extract status bytes
	SW1 = *((uint8_t*)(rx_buf + rx_len - 2));
	SW2 = *((uint8_t*)(rx_buf + rx_len - 1));

	// Remove status bytes
	rx_len -= 2;

	// Process warning/error status bytes
	// See PC/SC Part 3 Rev 2.01.09, 3.2.2.1.3, table 3-9
	if (SW1 == 0x6A && SW2 == 0x81) {
		// SW1-SW2 is 6A81 (Function not supported)
		// Card is ISO 7816 contact
		return 1;
	}

	if (SW1 != 0x90 || SW2 != 0x00) {
		// SW1-SW2 is not 9000 (Normal)
		// Unknown failure
		return -6;
	}


	if (*uid_len < rx_len) {
		// Output buffer too small
		return -7;
	}
	memcpy(uid, rx_buf, rx_len);
	*uid_len = rx_len;

	return 0;
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
		fprintf(stderr, "SCardConnect() failed; result=0x%x [%s]\n", (unsigned int)result, pcsc_stringify_error(result));
		return -1;
	}

	state = 0;
	reader->atr_len = sizeof(reader->atr);
	result = SCardStatus(reader->card, NULL, NULL, &state, &reader->protocol, reader->atr, &reader->atr_len);
	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardStatus() failed; result=0x%x [%s]\n", (unsigned int)result, pcsc_stringify_error(result));
		pcsc_reader_disconnect(reader);
		return -1;
	}

	// Determine whether PC/SC ATR may be for contactless card
	// See PC/SC Part 3 Rev 2.01.09, 3.1.3.2.3.1
	if (reader->atr_len > 4 &&
		reader->atr[0] == 0x3B &&
		(reader->atr[1] & 0x80) == 0x80 &&
		reader->atr[2] == 0x80 &&
		reader->atr[3] == 0x01 &&
		reader->atr_len == 5 + (reader->atr[1] & 0x0F)
	) {
		int r;

		// Determine whether it is a contactless card by requesting the
		// contactless UID
		reader->uid_len = sizeof(reader->uid);
		r = pcsc_reader_internal_get_uid(
			reader_ctx,
			reader->uid,
			&reader->uid_len
		);
		if (r < 0) {
			// Error during UID retrieval; assume card error
			return r;
		} else if (r > 0) {
			// Function not supported; assume contact card
			reader->type = PCSC_CARD_TYPE_CONTACT;
		} else {
			// UID retrieved; assume contactless card
			reader->type = PCSC_CARD_TYPE_CONTACTLESS;
		}
	} else {
		reader->type = PCSC_CARD_TYPE_CONTACT;
	}

	return reader->type;
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
		fprintf(stderr, "SCardDisconnect() failed; result=0x%x [%s]\n", (unsigned int)result, pcsc_stringify_error(result));
		return -1;
	}

	// Clear card attributes
	reader->card = 0;
	reader->protocol = SCARD_PROTOCOL_UNDEFINED;
	memset(reader->atr, 0, sizeof(reader->atr));
	reader->atr_len = 0;
	reader->type = PCSC_CARD_TYPE_UNKNOWN;

	return 0;
}

int pcsc_reader_get_atr(pcsc_reader_ctx_t reader_ctx, void* atr, size_t* atr_len)
{
	struct pcsc_reader_t* reader;

	if (!reader_ctx || !atr || !atr_len) {
		return -1;
	}
	reader = reader_ctx;

	if (!reader->atr_len) {
		return 1;
	}

	if (reader->type != PCSC_CARD_TYPE_CONTACT) {
		return 2;
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

		default:
			result = SCARD_F_INTERNAL_ERROR;
			break;
	}

	if (result != SCARD_S_SUCCESS) {
		fprintf(stderr, "SCardTransmit() failed; result=0x%x [%s]\n", (unsigned int)result, pcsc_stringify_error(result));
		return -1;
	}

	*rx_buf_len = rx_len;

	return 0;
}
