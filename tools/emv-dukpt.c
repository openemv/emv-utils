/**
 * @file emv-dukpt.c
 * @brief Simple EMV encryption tool using DUKPT
 *
 * Copyright 2026 Leon Lynch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "emv.h"
#include "emv_tags.h"
#include "iso8825_ber.h"
#include "crypto_mem.h"
#include "pinblock.h"

#include <dukpt_tdes.h>
#include <dukpt_aes.h>

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <argp.h>

#define EMV_DUKPT_MAX_KEY_LEN (DUKPT_AES_KEY_LEN(AES256))
#define EMV_DUKPT_MAX_BLOCK_LEN (DUKPT_AES_BLOCK_LEN)
#define EMV_DUKPT_MAX_PINBLOCK_LEN (DUKPT_AES_PINBLOCK_LEN)
#define EMV_DUKPT_MAX_BER_DEPTH (16)

// Forward declarations
struct emv_dukpt_ctx_t;
struct emv_dukpt_pin_data_t;
struct emv_dukpt_envelope_t;

// Helper functions
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state);
static int parse_hex(const char* hex, void* buf, size_t* buf_len);
static error_t parse_hex_arg(struct argp_state* state, const char* name, const char* arg, uint8_t** buf, size_t* buf_len);
static const struct emv_dukpt_algorithm_t* lookup_algorithm(int mode, int key_type);
static const struct emv_dukpt_algorithm_t* lookup_algorithm_by_oid(const struct iso8825_oid_t* oid);
static int prepare_dukpt_ctx(const char* key_name, const char* ksn_name, const char* key_type_name, bool key_type_valid, enum dukpt_aes_key_type_t key_type, struct emv_dukpt_ctx_t* ctx);
static int validate_ber(const void* buf, size_t buf_len, unsigned int depth);
static int validate_pan(const uint8_t* pan, size_t pan_len);
static int process_pin_data(uint8_t* buf, size_t buf_len, unsigned int depth, struct emv_dukpt_pin_data_t* pin_data);
static int build_plaintext(const void* buf, size_t buf_len, size_t block_len, uint8_t** plaintext, size_t* plaintext_len);
static int derive_txn_key(const struct emv_dukpt_ctx_t* ctx, void* txn_key);
static int encrypt_tdes(const struct emv_dukpt_ctx_t* ctx, const void* plaintext, size_t plaintext_len, void* ciphertext, void* authenticator);
static int encrypt_aes(const struct emv_dukpt_ctx_t* ctx, const void* plaintext, size_t plaintext_len, void* ciphertext, void* authenticator);
static int encrypt_pin_tdes(const struct emv_dukpt_ctx_t* ctx, const uint8_t* pin, size_t pin_len, const uint8_t* pan, size_t pan_len, void* ciphertext);
static int encrypt_pin_aes(const struct emv_dukpt_ctx_t* ctx, const uint8_t* pin, size_t pin_len, const uint8_t* pan, size_t pan_len, void* ciphertext);
static int build_envelope(const struct emv_dukpt_ctx_t* ctx, const void* ciphertext, size_t ciphertext_len, const void* authenticator, uint8_t** envelope, size_t* envelope_len);
static int encrypt_pin_data(const struct emv_dukpt_ctx_t* ctx, const struct emv_dukpt_pin_data_t* pin_data, uint8_t** envelope, size_t* envelope_len);
static int encrypt_emv_data(const struct emv_dukpt_ctx_t* ctx, const struct emv_dukpt_ctx_t* pin_ctx, uint8_t* buf, size_t buf_len, uint8_t** envelope, size_t* envelope_len, uint8_t** pin_envelope, size_t* pin_envelope_len);
static int parse_envelope(const void* buf, size_t buf_len, struct emv_dukpt_envelope_t* envelope);
static int validate_envelope(const struct emv_dukpt_envelope_t* envelope, const char* key_name, size_t key_len);
static int decrypt_tdes(const struct emv_dukpt_ctx_t* ctx, const struct emv_dukpt_envelope_t* envelope, void* plaintext);
static int decrypt_aes(const struct emv_dukpt_ctx_t* ctx, const struct emv_dukpt_envelope_t* envelope, void* plaintext);
static int extract_plaintext(const void* plaintext, size_t plaintext_len, size_t* data_len);
static int decrypt_emv_data(const struct emv_dukpt_ctx_t* ctx, const struct emv_dukpt_envelope_t* envelope, uint8_t** buf, size_t* buf_len);
static void print_hex(const void* buf, size_t buf_len);

// argp option keys
enum emv_dukpt_param_t {
	EMV_DUKPT_PARAM_MODE = -255, // Negative value to avoid short options
	EMV_DUKPT_PARAM_KEY_TYPE,
	EMV_DUKPT_PARAM_BDK,
	EMV_DUKPT_PARAM_IK,
	EMV_DUKPT_PARAM_KSN,
	EMV_DUKPT_PARAM_PIN_BDK,
	EMV_DUKPT_PARAM_PIN_IK,
	EMV_DUKPT_PARAM_PIN_KSN,
	EMV_DUKPT_PARAM_PIN_KEY_TYPE,
	EMV_DUKPT_ENCRYPT_REQUEST,
	EMV_DUKPT_DECRYPT_RESPONSE,
	EMV_DUKPT_VERSION,
};

// argp option structure
static struct argp_option argp_options[] = {
	{ NULL, 0, NULL, 0, "Derivation mode:", 1 },
	{ "mode", EMV_DUKPT_PARAM_MODE, "TDES|AES", 0, "Derivation mode. Default is TDES." },

	{ NULL, 0, NULL, 0, "Key options:", 2 },
	{ "key-type", EMV_DUKPT_PARAM_KEY_TYPE, "AES128|AES192|AES256", 0, "Working key type. Only applies to AES derivation mode. Defaults to length of BDK or IK." },
	{ "bdk", EMV_DUKPT_PARAM_BDK, "KEY", 0, "Base Derivation Key (BDK)" },
	{ "ik", EMV_DUKPT_PARAM_IK, "KEY", 0, "Initial Key (IK)" },
	{ "ksn", EMV_DUKPT_PARAM_KSN, "HEX", 0, "Key Serial Number (KSN)" },

	{ NULL, 0, NULL, 0, "Key options for PIN encryption:", 3 },
	{ "pin-bdk", EMV_DUKPT_PARAM_PIN_BDK, "KEY", 0, "Override Base Derivation Key (BDK) for PIN encryption." },
	{ "pin-ik", EMV_DUKPT_PARAM_PIN_IK, "KEY", 0, "Override Initial Key (IK) for PIN encryption." },
	{ "pin-ksn", EMV_DUKPT_PARAM_PIN_KSN, "HEX", 0, "Override Key Serial Number (KSN) for PIN encryption." },
	{ "pin-key-type", EMV_DUKPT_PARAM_PIN_KEY_TYPE, "AES128|AES192|AES256", 0, "Override Working key type for PIN encryption. Only applies to AES derivation mode." },

	{ NULL, 0, NULL, 0, "Actions:", 4 },
	{ "encrypt-request", EMV_DUKPT_ENCRYPT_REQUEST, "DATA", 0, "Encrypt EMV transaction request data and output as ASN.1 envelope, with optional second envelope for PIN encryption. Requires either BDK or IK, as well as KSN." },
	{ "decrypt-response", EMV_DUKPT_DECRYPT_RESPONSE, "DATA", 0, "Decrypt ASN.1 envelope and output EMV transaction response data. Requires either BDK or IK. Derivation mode, working key type and KSN are optional and must match the envelope." },

	{ "version", EMV_DUKPT_VERSION, NULL, 0, "Display emv-utils and dukpt versions" },

	{ 0 },
};

// argp configuration
static struct argp argp_config = {
	argp_options,
	argp_parser_helper,
	NULL,
	"Encrypt/decrypt EMV data using DUKPT"
	"\v" // Print remaining text after options
	"Use derivation mode TDES for ANSI X9.24-1:2009 TDES DUKPT.\n"
	"Use derivation mode AES for ANSI X9.24-3:2017 AES DUKPT.\n\n"
	"KEY and HEX argument values are strings of hex digits representing binary data.\n"
	"DATA argument values are strings of hex digits representing BER encoded data.\n\n"
	"The envelope is an ASN.1 sequence containing the OID of the DUKPT "
	"transaction key, KSN, encrypted EMV data, and authenticator.\n\n"
	"Transaction PIN Data (field 99) is always replaced by ASN.1 NULL (field "
	"05) of the same length before the EMV data is encrypted. If field 99 is "
	"in ISO 9564-1:2017 PIN block format 2 and the EMV data also contains a "
	"valid Application PAN (field 5A), the PIN is encrypted using "
	"ISO 9564-1:2017 PIN block format 0 for TDES or format 4 for AES, and "
	"output as a second envelope containing the OID of the DUKPT transaction "
	"key, KSN, and encrypted PIN block. PIN encryption uses the same BDK or "
	"IK, KSN and working key type as the EMV data, unless the PIN encryption "
	"options are specified.",
};

// Derivation mode
enum emv_dukpt_mode_t {
	EMV_DUKPT_MODE_TDES = 1,
	EMV_DUKPT_MODE_AES,
};
static enum emv_dukpt_mode_t mode = EMV_DUKPT_MODE_TDES;
static bool mode_valid = false;

// AES working key
static enum dukpt_aes_key_type_t key_type = -1;
static bool key_type_valid = false;

// Inputs
static uint8_t* bdk = NULL;
static size_t bdk_len = 0;
static uint8_t* ik = NULL;
static size_t ik_len = 0;
static uint8_t* ksn = NULL;
static size_t ksn_len = 0;
static uint8_t* input_data = NULL;
static size_t input_data_len = 0;

// PIN encryption
static uint8_t* pin_bdk = NULL;
static size_t pin_bdk_len = 0;
static uint8_t* pin_ik = NULL;
static size_t pin_ik_len = 0;
static uint8_t* pin_ksn = NULL;
static size_t pin_ksn_len = 0;
static enum dukpt_aes_key_type_t pin_key_type = -1;
static bool pin_key_type_valid = false;

// Actions
enum emv_dukpt_action_t {
	EMV_DUKPT_ACTION_NONE,
	EMV_DUKPT_ACTION_ENCRYPT_REQUEST,
	EMV_DUKPT_ACTION_DECRYPT_RESPONSE,
};
static enum emv_dukpt_action_t action = EMV_DUKPT_ACTION_NONE;

// DUKPT algorithms identified by the envelope
struct emv_dukpt_algorithm_t {
	struct iso8825_oid_t oid;
	enum emv_dukpt_mode_t mode;
	enum dukpt_aes_key_type_t key_type; // Only applies to AES derivation mode
	size_t key_len;
	size_t block_len;
};
static const struct emv_dukpt_algorithm_t algorithm_list[] = {
	{ ASN1_OID(dukpt_tdes), EMV_DUKPT_MODE_TDES, 0, DUKPT_TDES_KEY_LEN, DUKPT_TDES_BLOCK_LEN },
	{ ASN1_OID(dukpt_aes128), EMV_DUKPT_MODE_AES, DUKPT_AES_KEY_TYPE_AES128, DUKPT_AES_KEY_LEN(AES128), DUKPT_AES_BLOCK_LEN },
	{ ASN1_OID(dukpt_aes192), EMV_DUKPT_MODE_AES, DUKPT_AES_KEY_TYPE_AES192, DUKPT_AES_KEY_LEN(AES192), DUKPT_AES_BLOCK_LEN },
	{ ASN1_OID(dukpt_aes256), EMV_DUKPT_MODE_AES, DUKPT_AES_KEY_TYPE_AES256, DUKPT_AES_KEY_LEN(AES256), DUKPT_AES_BLOCK_LEN },
};

// DUKPT parameters
struct emv_dukpt_ctx_t {
	const uint8_t* bdk; // Either BDK or IK
	size_t bdk_len;
	const uint8_t* ik; // Either BDK or IK
	size_t ik_len;
	const uint8_t* ksn;
	size_t ksn_len;
	const struct emv_dukpt_algorithm_t* algorithm;
};

// Transaction PIN Data and Application PAN found in EMV data
struct emv_dukpt_pin_data_t {
	unsigned int pinblock_count;
	uint8_t pinblock[PINBLOCK_SIZE];
	size_t pinblock_len;
	uint8_t pin[PINBLOCK_PIN_LENGTH_MAX];
	size_t pin_len;
	unsigned int pan_count;
	const uint8_t* pan;
	size_t pan_len;
};

// Decoded envelope fields
struct emv_dukpt_envelope_t {
	const struct emv_dukpt_algorithm_t* algorithm;
	const uint8_t* ksn;
	size_t ksn_len;
	const uint8_t* ciphertext;
	size_t ciphertext_len;
	const uint8_t* authenticator;
	size_t authenticator_len;
};

// argp parser helper function
static error_t argp_parser_helper(int key, char* arg, struct argp_state* state)
{
	switch (key) {
		case EMV_DUKPT_PARAM_MODE: {
			if (strcasecmp(arg, "TDES") == 0) {
				mode = EMV_DUKPT_MODE_TDES;
			} else if (strcasecmp(arg, "AES") == 0) {
				mode = EMV_DUKPT_MODE_AES;
			} else {
				argp_error(state, "Derivation mode (--mode) argument must be TDES or AES");
				return EINVAL;
			}
			mode_valid = true;
			return 0;
		}

		case EMV_DUKPT_PARAM_KEY_TYPE: {
			if (strcasecmp(arg, "AES128") == 0) {
				key_type = DUKPT_AES_KEY_TYPE_AES128;
			} else if (strcasecmp(arg, "AES192") == 0) {
				key_type = DUKPT_AES_KEY_TYPE_AES192;
			} else if (strcasecmp(arg, "AES256") == 0) {
				key_type = DUKPT_AES_KEY_TYPE_AES256;
			} else {
				argp_error(state, "Working key type (--key-type) argument must be AES128, AES192 or AES256");
				return EINVAL;
			}
			key_type_valid = true;
			return 0;
		}

		case EMV_DUKPT_PARAM_BDK:
			return parse_hex_arg(state, "Base Derivation Key (--bdk)", arg, &bdk, &bdk_len);

		case EMV_DUKPT_PARAM_IK:
			return parse_hex_arg(state, "Initial Key (--ik)", arg, &ik, &ik_len);

		case EMV_DUKPT_PARAM_KSN:
			return parse_hex_arg(state, "Key Serial Number (--ksn)", arg, &ksn, &ksn_len);

		case EMV_DUKPT_PARAM_PIN_BDK:
			return parse_hex_arg(state, "PIN Base Derivation Key (--pin-bdk)", arg, &pin_bdk, &pin_bdk_len);

		case EMV_DUKPT_PARAM_PIN_IK:
			return parse_hex_arg(state, "PIN Initial Key (--pin-ik)", arg, &pin_ik, &pin_ik_len);

		case EMV_DUKPT_PARAM_PIN_KSN:
			return parse_hex_arg(state, "PIN Key Serial Number (--pin-ksn)", arg, &pin_ksn, &pin_ksn_len);

		case EMV_DUKPT_PARAM_PIN_KEY_TYPE: {
			if (strcasecmp(arg, "AES128") == 0) {
				pin_key_type = DUKPT_AES_KEY_TYPE_AES128;
			} else if (strcasecmp(arg, "AES192") == 0) {
				pin_key_type = DUKPT_AES_KEY_TYPE_AES192;
			} else if (strcasecmp(arg, "AES256") == 0) {
				pin_key_type = DUKPT_AES_KEY_TYPE_AES256;
			} else {
				argp_error(state, "PIN working key type (--pin-key-type) argument must be AES128, AES192 or AES256");
				return EINVAL;
			}
			pin_key_type_valid = true;
			return 0;
		}

		case EMV_DUKPT_ENCRYPT_REQUEST:
			if (action != EMV_DUKPT_ACTION_NONE) {
				argp_error(state, "Only one action may be specified");
				return EINVAL;
			}
			action = EMV_DUKPT_ACTION_ENCRYPT_REQUEST;
			return parse_hex_arg(state, "EMV data (--encrypt-request)", arg, &input_data, &input_data_len);

		case EMV_DUKPT_DECRYPT_RESPONSE:
			if (action != EMV_DUKPT_ACTION_NONE) {
				argp_error(state, "Only one action may be specified");
				return EINVAL;
			}
			action = EMV_DUKPT_ACTION_DECRYPT_RESPONSE;
			return parse_hex_arg(state, "Envelope (--decrypt-response)", arg, &input_data, &input_data_len);

		case EMV_DUKPT_VERSION: {
			const char* version;
			const char* tdes_version;
			const char* aes_version;

			version = emv_lib_version_string();
			if (version) {
				printf("emv-utils %s\n", version);
			} else {
				printf("emv-utils Unknown\n");
			}

			// The dukpt project provides libdukpt_tdes and libdukpt_aes which
			// report the same project version. Report the dukpt project
			// version if the library versions match. Otherwise report the
			// library versions separately.
			tdes_version = dukpt_tdes_lib_version_string();
			aes_version = dukpt_aes_lib_version_string();
			if (tdes_version && aes_version &&
				strcmp(tdes_version, aes_version) == 0
			) {
				printf("dukpt %s\n", tdes_version);
			} else {
				printf("libdukpt_tdes %s\n", tdes_version ? tdes_version : "Unknown");
				printf("libdukpt_aes %s\n", aes_version ? aes_version : "Unknown");
			}

			exit(EXIT_SUCCESS);
			return 0;
		}

		default:
			return ARGP_ERR_UNKNOWN;
	}
}

// Hex parser helper function
static int parse_hex(const char* hex, void* buf, size_t* buf_len)
{
	size_t max_buf_len;

	if (!buf_len) {
		return -1;
	}
	max_buf_len = *buf_len;
	*buf_len = 0;

	while (*hex && max_buf_len) {
		uint8_t* ptr = buf;
		char str[3];
		unsigned int str_idx = 0;

		// Find next two valid hex digits
		while (*hex && str_idx < 2) {
			// Skip spaces
			if (isspace((unsigned char)*hex)) {
				++hex;
				continue;
			}
			// Only allow hex digits
			if (!isxdigit((unsigned char)*hex)) {
				return -2;
			}

			str[str_idx++] = *hex;
			++hex;
		}
		if (!str_idx) {
			// No hex digits left
			continue;
		}
		if (str_idx != 2) {
			// Uneven number of hex digits
			return 1;
		}
		str[2] = 0;

		*ptr = strtoul(str, NULL, 16);
		++buf;
		++*buf_len;
		--max_buf_len;
	}

	return 0;
}

// Hex parser helper function for arguments
static error_t parse_hex_arg(
	struct argp_state* state,
	const char* name,
	const char* arg,
	uint8_t** buf,
	size_t* buf_len
)
{
	int r;

	// Discard previous instance of the same option
	if (*buf) {
		crypto_cleanse(*buf, *buf_len);
		free(*buf);
		*buf = NULL;
		*buf_len = 0;
	}

	// Ensure that the buffer has enough space for odd length hex strings
	*buf_len = strlen(arg) / 2 + 1;
	*buf = malloc(*buf_len);
	if (!*buf) {
		argp_failure(state, 1, ENOMEM, "%s", name);
		return ENOMEM;
	}

	r = parse_hex(arg, *buf, buf_len);
	if (r < 0) {
		argp_error(state, "%s argument must consist of hex digits", name);
		return EINVAL;
	}
	if (r > 0) {
		argp_error(state, "%s argument must have even number of hex digits", name);
		return EINVAL;
	}

	return 0;
}

// DUKPT algorithm lookup helper function
static const struct emv_dukpt_algorithm_t* lookup_algorithm(int mode, int key_type)
{
	for (size_t i = 0; i < sizeof(algorithm_list) / sizeof(algorithm_list[0]); ++i) {
		if (algorithm_list[i].mode != mode) {
			continue;
		}
		if (mode == EMV_DUKPT_MODE_AES &&
			algorithm_list[i].key_type != key_type
		) {
			continue;
		}

		return &algorithm_list[i];
	}

	return NULL;
}

// DUKPT algorithm lookup helper function for envelope OID
static const struct emv_dukpt_algorithm_t* lookup_algorithm_by_oid(const struct iso8825_oid_t* oid)
{
	for (size_t i = 0; i < sizeof(algorithm_list) / sizeof(algorithm_list[0]); ++i) {
		if (iso8825_ber_oid_is_equal(&algorithm_list[i].oid, oid)) {
			return &algorithm_list[i];
		}
	}

	return NULL;
}

// Encryption context setup helper function
static int prepare_dukpt_ctx(
	const char* key_name,
	const char* ksn_name,
	const char* key_type_name,
	bool key_type_valid,
	enum dukpt_aes_key_type_t key_type,
	struct emv_dukpt_ctx_t* ctx
)
{
	size_t key_len;

	if (!key_name || !ksn_name || !key_type_name || !ctx) {
		return -1;
	}
	if ((!ctx->bdk && !ctx->ik) || (ctx->bdk && ctx->ik)) {
		return -2;
	}
	key_len = ctx->bdk ? ctx->bdk_len : ctx->ik_len;

	if (!ctx->ksn) {
		fprintf(stderr, "%s is required\n", ksn_name);
		return 1;
	}

	if (mode == EMV_DUKPT_MODE_TDES) {
		if (key_type_valid) {
			fprintf(stderr, "%s only applies to AES derivation mode\n", key_type_name);
			return 2;
		}
		if (key_len != DUKPT_TDES_KEY_LEN) {
			fprintf(stderr, "%s must be %u bytes for TDES derivation mode\n", key_name, DUKPT_TDES_KEY_LEN);
			return 3;
		}
		if (ctx->ksn_len != DUKPT_TDES_KSN_LEN) {
			fprintf(stderr, "%s must be %u bytes for TDES derivation mode\n", ksn_name, DUKPT_TDES_KSN_LEN);
			return 4;
		}
		if (!dukpt_tdes_ksn_is_valid(ctx->ksn)) {
			fprintf(stderr, "%s transaction counter is invalid\n", ksn_name);
			return 5;
		}
	} else if (mode == EMV_DUKPT_MODE_AES) {
		if (key_len != DUKPT_AES_KEY_LEN(AES128) &&
			key_len != DUKPT_AES_KEY_LEN(AES192) &&
			key_len != DUKPT_AES_KEY_LEN(AES256)
		) {
			fprintf(stderr, "%s must be %u, %u or %u bytes for AES derivation mode\n",
				key_name,
				DUKPT_AES_KEY_LEN(AES128),
				DUKPT_AES_KEY_LEN(AES192),
				DUKPT_AES_KEY_LEN(AES256)
			);
			return 3;
		}
		if (ctx->ksn_len != DUKPT_AES_KSN_LEN) {
			fprintf(stderr, "%s must be %u bytes for AES derivation mode\n", ksn_name, DUKPT_AES_KSN_LEN);
			return 4;
		}
		if (!dukpt_aes_ksn_is_valid(ctx->ksn)) {
			fprintf(stderr, "%s transaction counter is invalid\n", ksn_name);
			return 5;
		}

		if (!key_type_valid) {
			// Working key type defaults to the length of the BDK or IK
			if (key_len == DUKPT_AES_KEY_LEN(AES128)) {
				key_type = DUKPT_AES_KEY_TYPE_AES128;
			} else if (key_len == DUKPT_AES_KEY_LEN(AES192)) {
				key_type = DUKPT_AES_KEY_TYPE_AES192;
			} else {
				key_type = DUKPT_AES_KEY_TYPE_AES256;
			}
		}
	} else {
		fprintf(stderr, "Unknown derivation mode\n");
		return -3;
	}

	ctx->algorithm = lookup_algorithm(mode, key_type);
	if (!ctx->algorithm) {
		fprintf(stderr, "Unsupported DUKPT algorithm\n");
		return -4;
	}
	if (ctx->algorithm->key_len > key_len) {
		fprintf(stderr, "%s must not be longer than %s\n", key_type_name, key_name);
		return 6;
	}

	return 0;
}

// BER validation helper function
static int validate_ber(const void* buf, size_t buf_len, unsigned int depth)
{
	int r;
	struct iso8825_ber_itr_t itr;
	struct iso8825_tlv_t tlv;

	if (depth > EMV_DUKPT_MAX_BER_DEPTH) {
		// Too many nested constructed fields
		return 1;
	}

	r = iso8825_ber_itr_init(buf, buf_len, &itr);
	if (r) {
		return -1;
	}

	while ((r = iso8825_ber_itr_next(&itr, &tlv)) > 0) {
		// EMV disallows indefinite length form and iso8825_ber_decode()
		// excludes the end-of-contents octets resulting in them being parsed
		// as zero tags. Therefore reject zero tags.
		// See EMV 4.4 Book 3, Annex B2
		if (tlv.tag == ASN1_EOC) {
			return 2;
		}

		if (iso8825_ber_is_constructed(&tlv)) {
			r = validate_ber(tlv.value, tlv.length, depth + 1);
			if (r) {
				return r;
			}
		}
	}
	if (r < 0) {
		// Invalid BER encoded data
		return 3;
	}

	return 0;
}

// Application PAN validation helper function
static int validate_pan(const uint8_t* pan, size_t pan_len)
{
	size_t digit_count = 0;
	bool padding_found = false;

	if (!pan || !pan_len) {
		return -1;
	}

	// EMV format "cn" consists of left justified digits padded with trailing
	// 'F's
	for (size_t i = 0; i < pan_len * 2; ++i) {
		uint8_t digit;

		if ((i & 0x1) == 0) {
			// Most significant nibble
			digit = pan[i >> 1] >> 4;
		} else {
			// Least significant nibble
			digit = pan[i >> 1] & 0xF;
		}

		if (digit == 0xF) {
			padding_found = true;
			continue;
		}
		if (digit > 9 || padding_found) {
			// Invalid digit or digit after padding
			return 1;
		}
		++digit_count;
	}

	// ISO 7812 specifies that PANs are 10 - 19 digits
	if (digit_count < 10 || digit_count > 19) {
		return 2;
	}

	return 0;
}

// PIN data processing helper function
static int process_pin_data(
	uint8_t* buf,
	size_t buf_len,
	unsigned int depth,
	struct emv_dukpt_pin_data_t* pin_data
)
{
	int r;
	struct iso8825_ber_itr_t itr;
	struct iso8825_tlv_t tlv;

	if (!buf || !pin_data) {
		return -1;
	}

	if (depth > EMV_DUKPT_MAX_BER_DEPTH) {
		// Too many nested constructed fields
		return -2;
	}

	r = iso8825_ber_itr_init(buf, buf_len, &itr);
	if (r) {
		return -3;
	}

	while ((r = iso8825_ber_itr_next(&itr, &tlv)) > 0) {
		// Locate the value within the buffer such that it can be modified
		uint8_t* value = buf + (tlv.value - buf);

		if (iso8825_ber_is_constructed(&tlv)) {
			r = process_pin_data(value, tlv.length, depth + 1, pin_data);
			if (r < 0) {
				return r;
			}
			continue;
		}

		if (tlv.tag == EMV_TAG_5A_APPLICATION_PAN) {
			++pin_data->pan_count;
			pin_data->pan = tlv.value;
			pin_data->pan_len = tlv.length;
		}

		if (tlv.tag == EMV_TAG_99_TRANSACTION_PIN_DATA) {
			uint8_t* tag = value - (r - tlv.length);

			++pin_data->pinblock_count;
			if (tlv.length <= sizeof(pin_data->pinblock)) {
				memcpy(pin_data->pinblock, value, tlv.length);
			}
			pin_data->pinblock_len = tlv.length;

			// The PIN must never be encrypted with a non-PIN key. Therefore
			// always replace Transaction PIN Data (field 99) in-place with an
			// ASN.1 NULL of the same length, regardless of its content. Both
			// tags are single octet tags and the length octets are retained.
			*tag = ASN1_NULL;
			crypto_cleanse(value, tlv.length);
		}
	}
	if (r < 0) {
		// Invalid BER encoded data
		return -4;
	}

	if (depth > 0) {
		// Validate only after all nested fields have been processed
		return 0;
	}

	if (!pin_data->pinblock_count) {
		// Transaction PIN Data not present
		return 0;
	}
	if (pin_data->pinblock_count > 1) {
		fprintf(stderr, "EMV data contains more than one Transaction PIN Data (field 99)\n");
		return 1;
	}

	if (pin_data->pinblock_len != PINBLOCK_SIZE) {
		fprintf(stderr, "Transaction PIN Data (field 99) is not ISO 9564-1:2017 PIN block format 2\n");
		return 2;
	}

	r = pinblock_decode_iso9564_format2(
		pin_data->pinblock,
		pin_data->pinblock_len,
		pin_data->pin,
		&pin_data->pin_len
	);
	if (r) {
		fprintf(stderr, "Transaction PIN Data (field 99) is not ISO 9564-1:2017 PIN block format 2\n");
		return 3;
	}
	crypto_cleanse(pin_data->pinblock, sizeof(pin_data->pinblock));

	if (!pin_data->pan_count) {
		fprintf(stderr, "Transaction PIN Data (field 99) requires Application PAN (field 5A)\n");
		return 4;
	}
	if (pin_data->pan_count > 1) {
		fprintf(stderr, "EMV data contains more than one Application PAN (field 5A)\n");
		return 5;
	}
	r = validate_pan(pin_data->pan, pin_data->pan_len);
	if (r) {
		fprintf(stderr, "Application PAN (field 5A) is invalid\n");
		return 6;
	}

	return 0;
}

// Plaintext builder helper function
static int build_plaintext(
	const void* buf,
	size_t buf_len,
	size_t block_len,
	uint8_t** plaintext,
	size_t* plaintext_len
)
{
	int r;
	struct iso8825_tlv_t tlv;
	bool apply_wrap;
	size_t data_len;
	size_t pad_len;

	if (!buf || !buf_len ||
		!block_len || block_len > EMV_DUKPT_MAX_BLOCK_LEN ||
		!plaintext || !plaintext_len
	) {
		return -1;
	}
	*plaintext = NULL;
	*plaintext_len = 0;

	// Determine whether a single EMV Data Template (field 70) already wraps
	// the EMV data
	r = iso8825_ber_decode(buf, buf_len, &tlv);
	if (r <= 0) {
		fprintf(stderr, "iso8825_ber_decode() failed; r=%d\n", r);
		return -2;
	}
	if (r != buf_len ||
		tlv.tag != EMV_TAG_70_DATA_TEMPLATE
	) {
		apply_wrap = true;
	} else {
		apply_wrap = false;
	}

	// Wrap if necessary and determine unpadded data length
	if (apply_wrap) {
		tlv.tag = EMV_TAG_70_DATA_TEMPLATE;
		tlv.length = buf_len;
		tlv.value = buf;
		tlv.flags = ISO8825_BER_CONSTRUCTED;

		r = iso8825_ber_compute_encoded_length(&tlv);
		if (r <= 0) {
			fprintf(stderr, "iso8825_ber_compute_encoded_length() failed; r=%d\n", r);
			return -3;
		}
		data_len = r;
	} else {
		data_len = buf_len;
	}

	// Determine padding length
	if ((data_len % block_len) != 0) {
		pad_len = block_len - (data_len % block_len);
		if (pad_len < 2) {
			// ASN.1 octet string requires at least two bytes for tag and length
			pad_len += block_len;
		}
	} else {
		pad_len = 0;
	}

	// Populate unpadded data
	*plaintext = malloc(data_len + pad_len);
	if (!*plaintext) {
		fprintf(stderr, "malloc() failed\n");
		return -4;
	}
	if (apply_wrap) {
		size_t len = data_len;
		r = iso8825_ber_encode(&tlv, *plaintext, &len);
		if (r) {
			fprintf(stderr, "iso8825_ber_encode() failed; r=%d\n", r);
			r = -5;
			goto error;
		}
		if (len != data_len) {
			fprintf(stderr, "Unexpected encoded length\n");
			r = -6;
			goto error;
		}
	} else {
		memcpy(*plaintext, buf, buf_len);
	}

	// Apply padding if necessary
	if (pad_len) {
		uint8_t pad[EMV_DUKPT_MAX_BLOCK_LEN] = { 0 };
		size_t len = pad_len;

		tlv.tag = ASN1_OCTET_STRING;
		tlv.length = pad_len - 2;
		tlv.value = pad;
		tlv.flags = 0;

		r = iso8825_ber_encode(&tlv, *plaintext + data_len, &len);
		if (r) {
			fprintf(stderr, "iso8825_ber_encode() failed; r=%d\n", r);
			r = -7;
			goto error;
		}
		if (len != pad_len) {
			fprintf(stderr, "Unexpected padding length\n");
			r = -8;
			goto error;
		}
	}
	*plaintext_len = data_len + pad_len;

	// Success
	return 0;

error:
	if (*plaintext) {
		crypto_cleanse(*plaintext, data_len + pad_len);
		free(*plaintext);
		*plaintext = NULL;
	}
	*plaintext_len = 0;
	return r;
}

// DUKPT transaction key derivation helper function
static int derive_txn_key(const struct emv_dukpt_ctx_t* ctx, void* txn_key)
{
	int r;
	uint8_t derived_ik[EMV_DUKPT_MAX_KEY_LEN];
	const uint8_t* key;
	size_t key_len;

	if (ctx->bdk) {
		// Derive Initial Key (IK) from Base Derivation Key (BDK) using the
		// Key Serial Number (KSN)
		if (ctx->algorithm->mode == EMV_DUKPT_MODE_TDES) {
			r = dukpt_tdes_derive_ik(ctx->bdk, ctx->ksn, derived_ik);
		} else {
			r = dukpt_aes_derive_ik(ctx->bdk, ctx->bdk_len, ctx->ksn, derived_ik);
		}
		if (r) {
			fprintf(stderr, "Failed to derive Initial Key (IK); r=%d\n", r);
			r = -1;
			goto exit;
		}
		key = derived_ik;
		key_len = ctx->bdk_len;
	} else {
		key = ctx->ik;
		key_len = ctx->ik_len;
	}

	if (ctx->algorithm->mode == EMV_DUKPT_MODE_TDES) {
		r = dukpt_tdes_derive_txn_key(key, ctx->ksn, txn_key);
	} else {
		r = dukpt_aes_derive_txn_key(key, key_len, ctx->ksn, txn_key);
	}
	if (r) {
		fprintf(stderr, "Failed to derive transaction key; r=%d\n", r);
		r = -2;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	crypto_cleanse(derived_ik, sizeof(derived_ik));
	return r;
}

// TDES DUKPT request encryption helper function
static int encrypt_tdes(
	const struct emv_dukpt_ctx_t* ctx,
	const void* plaintext,
	size_t plaintext_len,
	void* ciphertext,
	void* authenticator
)
{
	int r;
	uint8_t txn_key[DUKPT_TDES_KEY_LEN];

	r = derive_txn_key(ctx, txn_key);
	if (r) {
		goto exit;
	}

	// Use CMAC authenticator as IV because ANSI X9.19 Retail MAC is too short
	r = dukpt_tdes_generate_request_cmac(
		txn_key,
		plaintext,
		plaintext_len,
		authenticator
	);
	if (r) {
		fprintf(stderr, "dukpt_tdes_generate_request_cmac() failed; r=%d\n", r);
		r = -3;
		goto exit;
	}

	r = dukpt_tdes_encrypt_request(
		txn_key,
		authenticator,
		plaintext,
		plaintext_len,
		ciphertext
	);
	if (r) {
		fprintf(stderr, "dukpt_tdes_encrypt_request() failed; r=%d\n", r);
		r = -4;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	crypto_cleanse(txn_key, sizeof(txn_key));
	return r;
}

// AES DUKPT request encryption helper function
static int encrypt_aes(
	const struct emv_dukpt_ctx_t* ctx,
	const void* plaintext,
	size_t plaintext_len,
	void* ciphertext,
	void* authenticator
)
{
	int r;
	uint8_t txn_key[EMV_DUKPT_MAX_KEY_LEN];
	size_t txn_key_len = ctx->bdk ? ctx->bdk_len : ctx->ik_len;

	r = derive_txn_key(ctx, txn_key);
	if (r) {
		goto exit;
	}

	// Use CMAC authenticator as IV because HMAC-SHA256 is too long
	r = dukpt_aes_generate_request_cmac(
		txn_key,
		txn_key_len,
		ctx->ksn,
		ctx->algorithm->key_type,
		plaintext,
		plaintext_len,
		authenticator
	);
	if (r) {
		fprintf(stderr, "dukpt_aes_generate_request_cmac() failed; r=%d\n", r);
		r = -3;
		goto exit;
	}

	r = dukpt_aes_encrypt_request(
		txn_key,
		txn_key_len,
		ctx->ksn,
		ctx->algorithm->key_type,
		authenticator,
		plaintext,
		plaintext_len,
		ciphertext
	);
	if (r) {
		fprintf(stderr, "dukpt_aes_encrypt_request() failed; r=%d\n", r);
		r = -4;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	crypto_cleanse(txn_key, sizeof(txn_key));
	return r;
}

// TDES DUKPT PIN encryption helper function
static int encrypt_pin_tdes(
	const struct emv_dukpt_ctx_t* ctx,
	const uint8_t* pin,
	size_t pin_len,
	const uint8_t* pan,
	size_t pan_len,
	void* ciphertext
)
{
	int r;
	uint8_t txn_key[DUKPT_TDES_KEY_LEN];

	r = derive_txn_key(ctx, txn_key);
	if (r) {
		goto exit;
	}

	// Use ISO 9564-1:2017 PIN block format 0 for TDES DUKPT
	// See ISO 9564-1:2017 9.3.2
	r = dukpt_tdes_encrypt_pin(
		txn_key,
		PINBLOCK_ISO9564_FORMAT_0,
		pin,
		pin_len,
		pan,
		pan_len,
		ciphertext
	);
	if (r) {
		fprintf(stderr, "dukpt_tdes_encrypt_pin() failed; r=%d\n", r);
		r = -3;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	crypto_cleanse(txn_key, sizeof(txn_key));
	return r;
}

// AES DUKPT PIN encryption helper function
static int encrypt_pin_aes(
	const struct emv_dukpt_ctx_t* ctx,
	const uint8_t* pin,
	size_t pin_len,
	const uint8_t* pan,
	size_t pan_len,
	void* ciphertext
)
{
	int r;
	uint8_t txn_key[EMV_DUKPT_MAX_KEY_LEN];
	size_t txn_key_len = ctx->bdk ? ctx->bdk_len : ctx->ik_len;

	r = derive_txn_key(ctx, txn_key);
	if (r) {
		goto exit;
	}

	// Use ISO 9564-1:2017 PIN block format 4 for AES DUKPT
	// See ISO 9564-1:2017 9.4.2
	r = dukpt_aes_encrypt_pin(
		txn_key,
		txn_key_len,
		ctx->ksn,
		ctx->algorithm->key_type,
		pin,
		pin_len,
		pan,
		pan_len,
		ciphertext
	);
	if (r) {
		fprintf(stderr, "dukpt_aes_encrypt_pin() failed; r=%d\n", r);
		r = -3;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	crypto_cleanse(txn_key, sizeof(txn_key));
	return r;
}

// Envelope builder helper function
static int build_envelope(
	const struct emv_dukpt_ctx_t* ctx,
	const void* ciphertext,
	size_t ciphertext_len,
	const void* authenticator,
	uint8_t** envelope,
	size_t* envelope_len
)
{
	int r;
	uint8_t oid[16];
	size_t oid_len = sizeof(oid);
	size_t content_len = 0;
	uint8_t* content = NULL;
	size_t offset = 0;
	struct iso8825_tlv_t tlv;
	size_t field_count;
	size_t encoded_len = 0;

	if (!ctx ||
		!ciphertext || !ciphertext_len ||
		!envelope || !envelope_len
	) {
		return -1;
	}
	*envelope = NULL;
	*envelope_len = 0;

	r = iso8825_ber_oid_encode(&ctx->algorithm->oid, oid, &oid_len);
	if (r) {
		fprintf(stderr, "iso8825_ber_oid_encode() failed; r=%d\n", r);
		r = -2;
		goto error;
	}

	const struct iso8825_tlv_t fields[] = {
		{ ASN1_OBJECT_IDENTIFIER, oid_len, oid },
		{ ASN1_OCTET_STRING, ctx->ksn_len, ctx->ksn },
		{ ASN1_OCTET_STRING, ciphertext_len, ciphertext },
		{ ASN1_OCTET_STRING, ctx->algorithm->block_len, authenticator },
	};

	// Authenticator is optional and omitted if not provided
	field_count = sizeof(fields) / sizeof(fields[0]);
	if (!authenticator) {
		--field_count;
	}

	for (size_t i = 0; i < field_count; ++i) {
		r = iso8825_ber_compute_encoded_length(&fields[i]);
		if (r <= 0) {
			fprintf(stderr, "iso8825_ber_compute_encoded_length() failed; r=%d\n", r);
			r = -3;
			goto error;
		}
		content_len += r;
	}

	content = malloc(content_len);
	if (!content) {
		fprintf(stderr, "malloc() failed\n");
		r = -4;
		goto error;
	}

	for (size_t i = 0; i < field_count; ++i) {
		size_t len = content_len - offset;

		r = iso8825_ber_encode(&fields[i], content + offset, &len);
		if (r) {
			fprintf(stderr, "iso8825_ber_encode() failed; r=%d\n", r);
			r = -5;
			goto error;
		}
		offset += len;
	}

	// Wrap the fields in a SEQUENCE
	tlv.tag = ISO8825_BER_CONSTRUCTED | ASN1_SEQUENCE;
	tlv.length = content_len;
	tlv.value = content;
	tlv.flags = ISO8825_BER_CONSTRUCTED;

	r = iso8825_ber_compute_encoded_length(&tlv);
	if (r <= 0) {
		fprintf(stderr, "iso8825_ber_compute_encoded_length() failed; r=%d\n", r);
		r = -6;
		goto error;
	}
	encoded_len = r;

	*envelope = malloc(encoded_len);
	if (!*envelope) {
		fprintf(stderr, "malloc() failed\n");
		r = -7;
		goto error;
	}

	*envelope_len = encoded_len;
	r = iso8825_ber_encode(&tlv, *envelope, envelope_len);
	if (r) {
		fprintf(stderr, "iso8825_ber_encode() failed; r=%d\n", r);
		r = -8;
		goto error;
	}

	// Success
	r = 0;
	goto exit;

error:
	if (*envelope) {
		crypto_cleanse(*envelope, encoded_len);
		free(*envelope);
		*envelope = NULL;
	}
	*envelope_len = 0;

exit:
	if (content) {
		crypto_cleanse(content, content_len);
		free(content);
	}
	return r;
}

// Transaction PIN Data encryption helper function
static int encrypt_pin_data(
	const struct emv_dukpt_ctx_t* ctx,
	const struct emv_dukpt_pin_data_t* pin_data,
	uint8_t** envelope,
	size_t* envelope_len
)
{
	int r;
	uint8_t ciphertext[EMV_DUKPT_MAX_PINBLOCK_LEN];
	size_t ciphertext_len;

	if (!ctx ||
		!pin_data || !pin_data->pin_len ||
		!pin_data->pan || !pin_data->pan_len ||
		!envelope || !envelope_len
	) {
		return -1;
	}

	if (ctx->algorithm->mode == EMV_DUKPT_MODE_TDES) {
		r = encrypt_pin_tdes(ctx, pin_data->pin, pin_data->pin_len, pin_data->pan, pin_data->pan_len, ciphertext);
		ciphertext_len = DUKPT_TDES_PINBLOCK_LEN;
	} else {
		r = encrypt_pin_aes(ctx, pin_data->pin, pin_data->pin_len, pin_data->pan, pin_data->pan_len, ciphertext);
		ciphertext_len = DUKPT_AES_PINBLOCK_LEN;
	}
	if (r) {
		r = -2;
		goto exit;
	}

	// PIN encryption has no IV or authenticator
	r = build_envelope(ctx, ciphertext, ciphertext_len, NULL, envelope, envelope_len);
	if (r) {
		fprintf(stderr, "Failed to build PIN envelope\n");
		r = -3;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	crypto_cleanse(ciphertext, sizeof(ciphertext));
	return r;
}

// EMV transaction request data encryption helper function
static int encrypt_emv_data(
	const struct emv_dukpt_ctx_t* ctx,
	const struct emv_dukpt_ctx_t* pin_ctx,
	uint8_t* buf,
	size_t buf_len,
	uint8_t** envelope,
	size_t* envelope_len,
	uint8_t** pin_envelope,
	size_t* pin_envelope_len
)
{
	int r;
	struct emv_dukpt_pin_data_t pin_data = { 0 };
	uint8_t* plaintext = NULL;
	size_t plaintext_len = 0;
	uint8_t* ciphertext = NULL;
	uint8_t authenticator[EMV_DUKPT_MAX_BLOCK_LEN];

	if (!ctx || !pin_ctx ||
		!buf || !buf_len ||
		!envelope || !envelope_len ||
		!pin_envelope || !pin_envelope_len
	) {
		return -1;
	}
	*pin_envelope = NULL;
	*pin_envelope_len = 0;

	r = validate_ber(buf, buf_len, 0);
	if (r) {
		fprintf(stderr, "EMV data must consist of valid BER encoded data\n");
		return 1;
	}

	// Extract Transaction PIN Data (field 99) and Application PAN (field 5A),
	// and overwrite Transaction PIN Data (field 99) with ASN.1 NULL (field 05)
	r = process_pin_data(buf, buf_len, 0, &pin_data);
	if (r < 0) {
		fprintf(stderr, "Failed to process PIN data fields\n");
		r = -2;
		goto error;
	}
	if (r > 0) {
		// Invalid Transaction PIN Data or Application PAN
		fprintf(stderr, "PIN envelope omitted\n");
	} else if (pin_data.pin_len) {
		r = encrypt_pin_data(pin_ctx, &pin_data, pin_envelope, pin_envelope_len);
		if (r) {
			r = -3;
			goto error;
		}
	}

	r = build_plaintext(buf, buf_len, ctx->algorithm->block_len, &plaintext, &plaintext_len);
	if (r) {
		fprintf(stderr, "Failed to build plaintext\n");
		r = -4;
		goto error;
	}

	ciphertext = malloc(plaintext_len);
	if (!ciphertext) {
		r = -5;
		goto error;
	}

	if (ctx->algorithm->mode == EMV_DUKPT_MODE_TDES) {
		r = encrypt_tdes(ctx, plaintext, plaintext_len, ciphertext, authenticator);
	} else {
		r = encrypt_aes(ctx, plaintext, plaintext_len, ciphertext, authenticator);
	}
	if (r) {
		r = -6;
		goto error;
	}

	r = build_envelope(ctx, ciphertext, plaintext_len, authenticator, envelope, envelope_len);
	if (r) {
		fprintf(stderr, "Failed to build envelope\n");
		r = -7;
		goto error;
	}

	// Success
	r = 0;
	goto exit;

error:
	if (*pin_envelope) {
		crypto_cleanse(*pin_envelope, *pin_envelope_len);
		free(*pin_envelope);
		*pin_envelope = NULL;
		*pin_envelope_len = 0;
	}

exit:
	crypto_cleanse(&pin_data, sizeof(pin_data));
	if (plaintext) {
		crypto_cleanse(plaintext, plaintext_len);
		free(plaintext);
	}
	if (ciphertext) {
		free(ciphertext);
	}
	return r;
}

// Envelope parser helper function
static int parse_envelope(
	const void* buf,
	size_t buf_len,
	struct emv_dukpt_envelope_t* envelope
)
{
	int r;
	struct iso8825_tlv_t tlv;
	struct iso8825_oid_t oid;
	struct iso8825_ber_itr_t itr;
	struct iso8825_tlv_t fields[3];
	const char* mode_str;
	size_t ksn_len;
	size_t block_len;

	if (!buf || !buf_len || !envelope) {
		return -1;
	}
	memset(envelope, 0, sizeof(*envelope));

	r = iso8825_ber_decode(buf, buf_len, &tlv);
	if (r <= 0 || r != buf_len) {
		fprintf(stderr, "Envelope must consist of a single BER encoded field\n");
		return 1;
	}

	r = iso8825_ber_asn1_object_decode(&tlv, &oid);
	if (r <= 0) {
		fprintf(stderr, "Envelope must be an ASN.1 object\n");
		return 2;
	}
	envelope->algorithm = lookup_algorithm_by_oid(&oid);
	if (!envelope->algorithm) {
		fprintf(stderr, "Envelope OID is not a supported DUKPT algorithm\n");
		return 3;
	}

	// Decode KSN, encrypted data and authenticator
	r = iso8825_ber_itr_init(tlv.value + r, tlv.length - r, &itr);
	if (r) {
		fprintf(stderr, "iso8825_ber_itr_init() failed; r=%d\n", r);
		return -2;
	}
	for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
		r = iso8825_ber_itr_next(&itr, &fields[i]);
		if (r <= 0 || fields[i].tag != ASN1_OCTET_STRING) {
			fprintf(stderr, "Envelope must contain KSN, encrypted data and authenticator\n");
			return 4;
		}
	}
	r = iso8825_ber_itr_next(&itr, &tlv);
	if (r != 0) {
		fprintf(stderr, "Envelope must contain KSN, encrypted data and authenticator\n");
		return 5;
	}

	switch (envelope->algorithm->mode) {
		case EMV_DUKPT_MODE_TDES:
			mode_str = "TDES";
			ksn_len = DUKPT_TDES_KSN_LEN;
			break;

		case EMV_DUKPT_MODE_AES:
			mode_str = "AES";
			ksn_len = DUKPT_AES_KSN_LEN;
			break;

		default:
			fprintf(stderr, "Invalid derivation mode %u\n", envelope->algorithm->mode);
			return -3;
	}
	if (fields[0].length != ksn_len) {
		fprintf(stderr, "Envelope KSN must be %zu bytes for %s envelope\n", ksn_len, mode_str);
		return 6;
	}
	if ((envelope->algorithm->mode == EMV_DUKPT_MODE_TDES && !dukpt_tdes_ksn_is_valid(fields[0].value)) ||
		(envelope->algorithm->mode == EMV_DUKPT_MODE_AES && !dukpt_aes_ksn_is_valid(fields[0].value))
	) {
		fprintf(stderr, "Envelope KSN transaction counter is invalid\n");
		return 7;
	}

	block_len = envelope->algorithm->block_len;
	if (!fields[1].length || (fields[1].length % block_len) != 0) {
		fprintf(stderr, "Envelope encrypted data must be a non-zero multiple of %zu bytes for %s envelope\n", block_len, mode_str);
		return 8;
	}
	if (fields[2].length != block_len) {
		fprintf(stderr, "Envelope authenticator must be %zu bytes for %s envelope\n", block_len, mode_str);
		return 9;
	}

	envelope->ksn = fields[0].value;
	envelope->ksn_len = fields[0].length;
	envelope->ciphertext = fields[1].value;
	envelope->ciphertext_len = fields[1].length;
	envelope->authenticator = fields[2].value;
	envelope->authenticator_len = fields[2].length;

	return 0;
}

// Envelope validation helper function
static int validate_envelope(
	const struct emv_dukpt_envelope_t* envelope,
	const char* key_name,
	size_t key_len
)
{
	if (!envelope || !envelope->algorithm || !key_name) {
		return -1;
	}

	// Validate derivation mode, working key type and KSN if specified
	if (mode_valid && mode != envelope->algorithm->mode) {
		fprintf(stderr, "Derivation mode (--mode) does not match envelope\n");
		return 1;
	}
	if (key_type_valid &&
		(envelope->algorithm->mode != EMV_DUKPT_MODE_AES || key_type != envelope->algorithm->key_type)
	) {
		fprintf(stderr, "Working key type (--key-type) does not match envelope\n");
		return 2;
	}
	if (ksn &&
		(ksn_len != envelope->ksn_len || memcmp(ksn, envelope->ksn, ksn_len) != 0)
	) {
		fprintf(stderr, "Key Serial Number (--ksn) does not match envelope\n");
		return 3;
	}

	// Validate BDK or IK length for envelope
	if (envelope->algorithm->mode == EMV_DUKPT_MODE_TDES &&
		key_len != DUKPT_TDES_KEY_LEN
	) {
		fprintf(stderr, "%s must be %u bytes for TDES envelope\n", key_name, DUKPT_TDES_KEY_LEN);
		return 4;
	}
	if (envelope->algorithm->mode == EMV_DUKPT_MODE_AES &&
		key_len != DUKPT_AES_KEY_LEN(AES128) &&
		key_len != DUKPT_AES_KEY_LEN(AES192) &&
		key_len != DUKPT_AES_KEY_LEN(AES256)
	) {
		fprintf(stderr, "%s must be %u, %u or %u bytes for AES envelope\n",
			key_name,
			DUKPT_AES_KEY_LEN(AES128),
			DUKPT_AES_KEY_LEN(AES192),
			DUKPT_AES_KEY_LEN(AES256)
		);
		return 5;
	}
	if (envelope->algorithm->key_len > key_len) {
		fprintf(stderr, "Envelope working key type must not be longer than %s\n", key_name);
		return 6;
	}

	return 0;
}

// TDES DUKPT response decryption helper function
static int decrypt_tdes(
	const struct emv_dukpt_ctx_t* ctx,
	const struct emv_dukpt_envelope_t* envelope,
	void* plaintext
)
{
	int r;
	uint8_t txn_key[DUKPT_TDES_KEY_LEN];

	r = derive_txn_key(ctx, txn_key);
	if (r) {
		goto exit;
	}

	// Use CMAC authenticator as IV
	r = dukpt_tdes_decrypt_response(
		txn_key,
		envelope->authenticator,
		envelope->ciphertext,
		envelope->ciphertext_len,
		plaintext
	);
	if (r) {
		fprintf(stderr, "dukpt_tdes_decrypt_response() failed; r=%d\n", r);
		r = -3;
		goto exit;
	}

	r = dukpt_tdes_verify_response_cmac(
		txn_key,
		plaintext,
		envelope->ciphertext_len,
		envelope->authenticator
	);
	if (r < 0) {
		fprintf(stderr, "dukpt_tdes_verify_response_cmac() failed; r=%d\n", r);
		r = -4;
		goto exit;
	}
	if (r > 0) {
		fprintf(stderr, "Envelope authenticator verification failed\n");
		r = 1;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	crypto_cleanse(txn_key, sizeof(txn_key));
	return r;
}

// AES DUKPT response decryption helper function
static int decrypt_aes(
	const struct emv_dukpt_ctx_t* ctx,
	const struct emv_dukpt_envelope_t* envelope,
	void* plaintext
)
{
	int r;
	uint8_t txn_key[EMV_DUKPT_MAX_KEY_LEN];
	size_t txn_key_len = ctx->bdk ? ctx->bdk_len : ctx->ik_len;

	r = derive_txn_key(ctx, txn_key);
	if (r) {
		goto exit;
	}

	// Use CMAC authenticator as IV
	r = dukpt_aes_decrypt_response(
		txn_key,
		txn_key_len,
		ctx->ksn,
		ctx->algorithm->key_type,
		envelope->authenticator,
		envelope->ciphertext,
		envelope->ciphertext_len,
		plaintext
	);
	if (r) {
		fprintf(stderr, "dukpt_aes_decrypt_response() failed; r=%d\n", r);
		r = -3;
		goto exit;
	}

	r = dukpt_aes_verify_response_cmac(
		txn_key,
		txn_key_len,
		ctx->ksn,
		ctx->algorithm->key_type,
		plaintext,
		envelope->ciphertext_len,
		envelope->authenticator
	);
	if (r < 0) {
		fprintf(stderr, "dukpt_aes_verify_response_cmac() failed; r=%d\n", r);
		r = -4;
		goto exit;
	}
	if (r > 0) {
		fprintf(stderr, "Envelope authenticator verification failed\n");
		r = 1;
		goto exit;
	}

	// Success
	r = 0;
	goto exit;

exit:
	crypto_cleanse(txn_key, sizeof(txn_key));
	return r;
}

// Plaintext extraction helper function
static int extract_plaintext(
	const void* plaintext,
	size_t plaintext_len,
	size_t* data_len
)
{
	int r;
	struct iso8825_ber_itr_t itr;
	struct iso8825_tlv_t tlv;

	if (!plaintext || !plaintext_len || !data_len) {
		return -1;
	}
	*data_len = 0;

	r = validate_ber(plaintext, plaintext_len, 0);
	if (r) {
		fprintf(stderr, "Decrypted data must consist of valid BER encoded data\n");
		return 1;
	}

	r = iso8825_ber_itr_init(plaintext, plaintext_len, &itr);
	if (r) {
		fprintf(stderr, "iso8825_ber_itr_init() failed; r=%d\n", r);
		return -2;
	}

	// EMV Data Template (field 70) must be first
	r = iso8825_ber_itr_next(&itr, &tlv);
	if (r <= 0 || tlv.tag != EMV_TAG_70_DATA_TEMPLATE) {
		fprintf(stderr, "Decrypted data must start with EMV Data Template (field 70)\n");
		return 2;
	}
	*data_len = r;

	// Padding is optional but must be a single zero filled octet string
	r = iso8825_ber_itr_next(&itr, &tlv);
	if (r > 0) {
		if (tlv.tag != ASN1_OCTET_STRING) {
			fprintf(stderr, "Decrypted data contains invalid padding\n");
			return 3;
		}
		for (unsigned int i = 0; i < tlv.length; ++i) {
			if (tlv.value[i]) {
				fprintf(stderr, "Decrypted data contains invalid padding\n");
				return 3;
			}
		}

		r = iso8825_ber_itr_next(&itr, &tlv);
	}
	if (r != 0) {
		fprintf(stderr, "Decrypted data contains invalid padding\n");
		return 3;
	}

	return 0;
}

// EMV transaction response data decryption helper function
static int decrypt_emv_data(
	const struct emv_dukpt_ctx_t* ctx,
	const struct emv_dukpt_envelope_t* envelope,
	uint8_t** buf,
	size_t* buf_len
)
{
	int r;
	uint8_t* plaintext = NULL;
	size_t data_len;

	if (!ctx || !envelope || !buf || !buf_len) {
		return -1;
	}
	*buf = NULL;
	*buf_len = 0;

	plaintext = malloc(envelope->ciphertext_len);
	if (!plaintext) {
		fprintf(stderr, "malloc() failed\n");
		return -2;
	}

	if (ctx->algorithm->mode == EMV_DUKPT_MODE_TDES) {
		r = decrypt_tdes(ctx, envelope, plaintext);
	} else {
		r = decrypt_aes(ctx, envelope, plaintext);
	}
	if (r < 0) {
		r = -3;
		goto exit;
	}
	if (r > 0) {
		// Authenticator verification failed
		r = 1;
		goto exit;
	}

	r = extract_plaintext(plaintext, envelope->ciphertext_len, &data_len);
	if (r < 0) {
		fprintf(stderr, "Failed to extract plaintext\n");
		r = -4;
		goto exit;
	}
	if (r > 0) {
		// Invalid decrypted data
		r = 2;
		goto exit;
	}

	*buf = malloc(data_len);
	if (!*buf) {
		fprintf(stderr, "malloc() failed\n");
		r = -5;
		goto exit;
	}
	memcpy(*buf, plaintext, data_len);
	*buf_len = data_len;

	// Success
	r = 0;
	goto exit;

exit:
	crypto_cleanse(plaintext, envelope->ciphertext_len);
	free(plaintext);
	return r;
}

// Hex output helper function
static void print_hex(const void* buf, size_t buf_len)
{
	const uint8_t* ptr = buf;

	for (size_t i = 0; i < buf_len; ++i) {
		printf("%02X", ptr[i]);
	}
	printf("\n");
}

int main(int argc, char** argv)
{
	int r;
	const char* key_name = NULL;
	size_t key_len = 0;
	struct emv_dukpt_ctx_t ctx = { 0 };
	struct emv_dukpt_ctx_t pin_ctx = { 0 };
	struct emv_dukpt_envelope_t envelope = { 0 };
	uint8_t* output = NULL;
	size_t output_len = 0;
	uint8_t* pin_output = NULL;
	size_t pin_output_len = 0;

	if (argc == 1) {
		// No command line arguments
		argp_help(&argp_config, stdout, ARGP_HELP_STD_HELP, argv[0]);
		return 1;
	}

	r = argp_parse(&argp_config, argc, argv, 0, 0, 0);
	if (r) {
		fprintf(stderr, "Failed to parse command line\n");
		return 1;
	}

	if (action == EMV_DUKPT_ACTION_NONE || !input_data || !input_data_len) {
		fprintf(stderr, "Specify either EMV data to encrypt (--encrypt-request) or envelope to decrypt (--decrypt-response)\n");
		goto usage;
	}

	// Validate BDK/IK options
	if ((!bdk && !ik) || (bdk && ik)) {
		fprintf(stderr, "Specify either Base Derivation Key (--bdk) or Initial Key (--ik)\n");
		goto usage;
	}
	if (bdk) {
		key_name = "Base Derivation Key (--bdk)";
		key_len = bdk_len;
	} else if (ik) {
		key_name = "Initial Key (--ik)";
		key_len = ik_len;
	}
	ctx.bdk = bdk;
	ctx.bdk_len = bdk_len;
	ctx.ik = ik;
	ctx.ik_len = ik_len;

	// Validate encryption options
	if (action == EMV_DUKPT_ACTION_ENCRYPT_REQUEST) {
		const char* pin_key_name = key_name;
		const char* pin_ksn_name = "Key Serial Number (--ksn)";
		const char* pin_key_type_name = "Working key type (--key-type)";
		bool pin_ctx_key_type_valid = key_type_valid;
		enum dukpt_aes_key_type_t pin_ctx_key_type = key_type;

		ctx.ksn = ksn;
		ctx.ksn_len = ksn_len;
		r = prepare_dukpt_ctx(
			key_name,
			"Key Serial Number (--ksn)",
			"Working key type (--key-type)",
			key_type_valid,
			key_type,
			&ctx
		);
		if (r < 0) {
			r = 1;
			goto exit;
		}
		if (r > 0) {
			goto usage;
		}

		// Apply PIN encryption options if available, otherwise default to data
		// encryption options
		if (pin_bdk && pin_ik) {
			fprintf(stderr, "Specify either PIN Base Derivation Key (--pin-bdk) or PIN Initial Key (--pin-ik)\n");
			goto usage;
		}
		if (pin_bdk || pin_ik) {
			// Overwrite PIN keys
			pin_ctx.bdk = pin_bdk;
			pin_ctx.bdk_len = pin_bdk_len;
			pin_ctx.ik = pin_ik;
			pin_ctx.ik_len = pin_ik_len;
			if (pin_bdk) {
				pin_key_name = "PIN Base Derivation Key (--pin-bdk)";
			} else {
				pin_key_name = "PIN Initial Key (--pin-ik)";
			}
			pin_ctx_key_type_valid = false;
		} else {
			pin_ctx.bdk = bdk;
			pin_ctx.bdk_len = bdk_len;
			pin_ctx.ik = ik;
			pin_ctx.ik_len = ik_len;
		}
		if (pin_ksn) {
			// Overwrite PIN KSN
			pin_ctx.ksn = pin_ksn;
			pin_ctx.ksn_len = pin_ksn_len;
			pin_ksn_name = "PIN Key Serial Number (--pin-ksn)";
		} else {
			pin_ctx.ksn = ksn;
			pin_ctx.ksn_len = ksn_len;
		}
		if (pin_key_type_valid) {
			// Overwrite PIN working key type
			pin_key_type_name = "PIN working key type (--pin-key-type)";
			pin_ctx_key_type_valid = true;
			pin_ctx_key_type = pin_key_type;
		}

		r = prepare_dukpt_ctx(
			pin_key_name,
			pin_ksn_name,
			pin_key_type_name,
			pin_ctx_key_type_valid,
			pin_ctx_key_type,
			&pin_ctx
		);
		if (r < 0) {
			r = 1;
			goto exit;
		}
		if (r > 0) {
			goto usage;
		}
	}

	switch (action) {
		case EMV_DUKPT_ACTION_ENCRYPT_REQUEST: {
			r = encrypt_emv_data(&ctx, &pin_ctx, input_data, input_data_len, &output, &output_len, &pin_output, &pin_output_len);
			break;
		}

		case EMV_DUKPT_ACTION_DECRYPT_RESPONSE: {
			if (pin_bdk || pin_ik || pin_ksn || pin_key_type_valid) {
				fprintf(stderr, "PIN encryption options only apply to EMV transaction request encryption (--encrypt-request)\n");
				goto usage;
			}

			// Envelope provides derivation mode, working key type and KSN
			r = parse_envelope(input_data, input_data_len, &envelope);
			if (r) {
				r = 1;
				goto exit;
			}

			r = validate_envelope(&envelope, key_name, key_len);
			if (r) {
				r = 1;
				goto exit;
			}
			ctx.ksn = envelope.ksn;
			ctx.ksn_len = envelope.ksn_len;
			ctx.algorithm = envelope.algorithm;

			r = decrypt_emv_data(&ctx, &envelope, &output, &output_len);
			break;
		}

		default:
			fprintf(stderr, "Unknown action\n");
			r = 1;
			goto exit;
	}
	if (r) {
		r = 1;
		goto exit;
	}
	print_hex(output, output_len);
	if (pin_output) {
		print_hex(pin_output, pin_output_len);
	}

	// Success
	r = 0;
	goto exit;

usage:
	argp_help(&argp_config, stdout, ARGP_HELP_STD_HELP, argv[0]);
	r = 1;
	goto exit;

exit:
	if (output) {
		crypto_cleanse(output, output_len);
		free(output);
	}
	if (pin_output) {
		crypto_cleanse(pin_output, pin_output_len);
		free(pin_output);
	}
	if (bdk) {
		crypto_cleanse(bdk, bdk_len);
		free(bdk);
	}
	if (ik) {
		crypto_cleanse(ik, ik_len);
		free(ik);
	}
	free(ksn);
	if (pin_bdk) {
		crypto_cleanse(pin_bdk, pin_bdk_len);
		free(pin_bdk);
	}
	if (pin_ik) {
		crypto_cleanse(pin_ik, pin_ik_len);
		free(pin_ik);
	}
	free(pin_ksn);
	if (input_data) {
		crypto_cleanse(input_data, input_data_len);
		free(input_data);
	}

	return r;
}
