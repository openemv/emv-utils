/**
 * @file emv_ep_create_common_kernel_data_test.c
 * @brief Unit tests for EMV Entry Point common contactless kernel data
 *
 * Copyright 2026 Leon Lynch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include "emv_ep.h"
#include "emv.h"
#include "emv_ttl.h"
#include "emv_tlv.h"
#include "emv_app.h"
#include "emv_config.h"
#include "emv_tags.h"
#include "emv_fields.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

// Reuse source data for all tests
static const uint8_t test_aid[] = { 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10 };
static const uint8_t test_fci[] = { // Contactless FCI without PDOL
	0x6F, 0x12, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00,
	0x04, 0x10, 0x10, 0xA5, 0x07, 0x50, 0x05, 0x41,
	0x50, 0x50, 0x20, 0x31,
};
static const struct emv_config_app_t test_config_app = {
	.aid = { 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10 },
	.aid_len = 7,
	.asi = EMV_ASI_PARTIAL_MATCH,
};

// Terminal Floor Limit of 10000
static const struct emv_tlv_t test_config_data[] = {
	{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
	{ {{ 0 }} },
};

struct test_t {
	const char* name;

	// Application dependent configuration
	const struct emv_tlv_t* config_app_data;
	bool contactless_floor_limit_enabled;
	unsigned int contactless_floor_limit;
	bool contactless_cvm_required_limit_enabled;
	unsigned int contactless_cvm_required_limit;

	// Application independent configuration
	const struct emv_tlv_t* config_data;

	// Transaction parameters
	const struct emv_tlv_t* params_data;

	// Integrated Circuit Card (ICC) data from the PPSE directory entry
	const struct emv_tlv_t* icc_data;

	int result;

	// Expected terminal data. NULL if not expected.
	const uint8_t* kernel_id;
	const uint8_t* ttq;
};

static const struct test_t test[] = {
	{
		.name = "Application without Terminal Transaction Qualifiers (9F66)",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-2
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 3000
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.kernel_id = (uint8_t[]){ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	},

	{
		.name = "Kernel Identifier (9F2A) not found",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x02, 0x11, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		// Extended Kernel ID is not present and must be zeroed
		.kernel_id = (uint8_t[]){ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	},

	{
		.name = "International Kernel Identifier (9F2A)",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x02, 0x11, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F2A_KERNEL_IDENTIFIER, 1, (uint8_t[]){ 0x02 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		// Extended Kernel ID is not present and must be zeroed
		.kernel_id = (uint8_t[]){ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	},

	{
		.name = "RFU Kernel Identifier (9F2A)",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x43, 0x11, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F2A_KERNEL_IDENTIFIER, 3, (uint8_t[]){ 0x43, 0x05, 0x28 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		// Extended Kernel ID is ignored for RFU kernel identifier type
		.kernel_id = (uint8_t[]){ 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	},

	{
		.name = "Domestic Kernel Identifier (9F2A) with Extended Kernel ID",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x83, 0x05, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F2A_KERNEL_IDENTIFIER, 3, (uint8_t[]){ 0x83, 0x05, 0x28 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.kernel_id = (uint8_t[]){ 0x83, 0x05, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00 },
	},

	{
		.name = "Domestic Kernel Identifier (9F2A) without Extended Kernel ID",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x83, 0x05, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F2A_KERNEL_IDENTIFIER, 1, (uint8_t[]){ 0x83 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		// Extended Kernel ID is not present and must be zeroed
		.kernel_id = (uint8_t[]){ 0x83, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	},

	{
		.name = "Kernel Identifier - Terminal (96) beyond Extended Kernel ID",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x02, 0x00, 0x00, 0xC3, 0x44, 0x55, 0x66, 0x77 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		// Only the first three bytes are used and the Kernel C-8 reader and
		// transaction support bits are cleared
		.kernel_id = (uint8_t[]){ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
	},

	{
		.name = "Contactless Floor Limit exceeded",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL }, // Online-capable reader
			{ {{ 0 }} },
		},
		.contactless_floor_limit_enabled = true,
		.contactless_floor_limit = 3000,

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 3001
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB9 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.kernel_id = (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.ttq = (uint8_t[]){ 0x37, 0x80, 0x40, 0x00 }, // Online cryptogram required
	},

	{
		.name = "Contactless Floor Limit reached",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL }, // Online-capable reader
			{ {{ 0 }} },
		},
		.contactless_floor_limit_enabled = true,
		.contactless_floor_limit = 3000,

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			// Contactless Floor Limit is only exceeded when the amount is
			// greater than the limit
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.kernel_id = (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.ttq = (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 },
	},

	{
		.name = "Terminal Floor Limit exceeded",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL }, // Online-capable reader
			{ {{ 0 }} },
		},
		// Terminal Floor Limit (9F1B) applies when Contactless Floor Limit is
		// not enabled
		.contactless_floor_limit_enabled = false,
		.contactless_floor_limit = 3000,

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 10001
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.kernel_id = (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.ttq = (uint8_t[]){ 0x37, 0x80, 0x40, 0x00 }, // Online cryptogram required
	},

	{
		.name = "Terminal Floor Limit reached",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL }, // Online-capable reader
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 10000
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.kernel_id = (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.ttq = (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 },
	},

	{
		.name = "Contactless CVM Required Limit reached",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL }, // Online-capable reader
			{ {{ 0 }} },
		},
		.contactless_cvm_required_limit_enabled = true,
		.contactless_cvm_required_limit = 5000,

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			// Contactless CVM Required Limit is exceeded when the amount is
			// equal to the limit
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x13, 0x88 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.kernel_id = (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.ttq = (uint8_t[]){ 0x37, 0x40, 0x40, 0x00 }, // CVM required
	},

	{
		.name = "Contactless CVM Required Limit not reached",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL }, // Online-capable reader
			{ {{ 0 }} },
		},
		.contactless_cvm_required_limit_enabled = true,
		.contactless_cvm_required_limit = 5000,

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 4999
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x13, 0x87 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.kernel_id = (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.ttq = (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 },
	},

	{
		.name = "Contactless Floor Limit and Contactless CVM Required Limit exceeded",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL }, // Online-capable reader
			{ {{ 0 }} },
		},
		.contactless_floor_limit_enabled = true,
		.contactless_floor_limit = 3000,
		.contactless_cvm_required_limit_enabled = true,
		.contactless_cvm_required_limit = 5000,

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 5000
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x13, 0x88 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.kernel_id = (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.ttq = (uint8_t[]){ 0x37, 0xC0, 0x40, 0x00 }, // Online cryptogram and CVM required
	},

	{
		.name = "Configured Terminal Transaction Qualifiers (9F66) bits cleared",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			// Online cryptogram and CVM required bits must be cleared before
			// the limits are applied
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0xC0, 0x40, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},
		.contactless_floor_limit_enabled = true,
		.contactless_floor_limit = 3000,

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 1000
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x03, 0xE8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.kernel_id = (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.ttq = (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 },
	},

	{
		.name = "Contactless Zero Amount for online-capable reader",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL }, // Online-capable reader
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.kernel_id = (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.ttq = (uint8_t[]){ 0x37, 0x80, 0x40, 0x00 }, // Online cryptogram required
	},

	{
		.name = "Contactless Zero Amount for offline-only reader",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x3F, 0x00, 0x40, 0x00 }, 0 }}, NULL }, // Offline-only reader
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.kernel_id = (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
		.ttq = (uint8_t[]){ 0x3F, 0x00, 0x40, 0x00 },
	},

	{
		.name = "Kernel Identifier - Terminal (96) not found",

		.config_app_data = NULL,

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_CONFIG,
	},

	{
		.name = "Invalid Kernel Identifier - Terminal (96) length",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 3, (uint8_t[]){ 0x02, 0x00, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_CONFIG,
	},

	{
		.name = "Invalid Terminal Transaction Qualifiers (9F66) length",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 3, (uint8_t[]){ 0x37, 0x00, 0x40 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_CONFIG,
	},

	{
		.name = "Amount, Authorised - Binary (81) not found",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = NULL,

		.result = EMV_ERROR_INVALID_PARAMETER,
	},

	{
		.name = "Invalid Amount, Authorised - Binary (81) length",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.config_data = test_config_data,

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 3, (uint8_t[]){ 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_PARAMETER,
	},

	{
		.name = "Terminal Floor Limit (9F1B) not found",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},
		// Terminal Floor Limit (9F1B) is mandatory even when the Contactless
		// Floor Limit is enabled
		.contactless_floor_limit_enabled = true,
		.contactless_floor_limit = 3000,

		.config_data = NULL,

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_CONFIG,
	},

	{
		.name = "Invalid Terminal Floor Limit (9F1B) length",

		.config_app_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL }, // Kernel C-3
			{ {{ EMV_TAG_9F66_TTQ, 4, (uint8_t[]){ 0x37, 0x00, 0x40, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.config_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 3, (uint8_t[]){ 0x00, 0x27, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (const struct emv_tlv_t[]){
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x0B, 0xB8 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_ERROR_INVALID_CONFIG,
	},
};

static int append_tlv_list(
	const struct emv_tlv_t* tlv_array,
	struct emv_tlv_list_t* list
)
{
	int r;

	while (tlv_array && tlv_array->tag) {
		r = emv_tlv_list_push(list, tlv_array->tag, tlv_array->length, tlv_array->value, 0);
		if (r) {
			return r;
		}

		++tlv_array;
	}

	return 0;
}

static int populate_tlv_list(
	const struct emv_tlv_t* tlv_array,
	struct emv_tlv_list_t* list
)
{
	emv_tlv_list_clear(list);

	return append_tlv_list(tlv_array, list);
}

static int verify_terminal_data(
	const struct emv_ctx_t* ctx,
	const uint8_t* kernel_id,
	const uint8_t* ttq
)
{
	const struct emv_tlv_t* tlv;

	if (!ctx || !kernel_id) {
		return -1;
	}

	// This is ugly but that's why it's in a helper function
	// See EMV Contactless Book B v2.11, 3.4
	if (emv_tlv_list_is_empty(&ctx->terminal) ||
		ctx->terminal.front->tag != EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL ||
		!ctx->terminal.front->next ||
		ctx->terminal.front->next->tag != EMV_TAG_9F39_POS_ENTRY_MODE ||
		!ctx->terminal.front->next->next ||
		ctx->terminal.front->next->next->tag != EMV_TAG_9F06_AID ||
		!ctx->terminal.front->next->next->next ||
		ctx->terminal.front->next->next->next->tag != EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION ||
		!ctx->terminal.front->next->next->next->next ||
		ctx->terminal.front->next->next->next->next->tag != EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS ||
		!ctx->terminal.front->next->next->next->next->next ||
		ctx->terminal.front->next->next->next->next->next->tag != EMV_TAG_9F37_UNPREDICTABLE_NUMBER
	) {
		fprintf(stderr, "Unexpected terminal data list state\n");
		return 1;
	}
	if (ttq) {
		if (!ctx->terminal.front->next->next->next->next->next->next ||
			ctx->terminal.front->next->next->next->next->next->next->tag != EMV_TAG_9F66_TTQ ||
			ctx->terminal.front->next->next->next->next->next->next->next
		) {
			fprintf(stderr, "Unexpected terminal data list state\n");
			return 1;
		}
	} else {
		if (ctx->terminal.front->next->next->next->next->next->next) {
			fprintf(stderr, "Unexpected terminal data list state\n");
			return 1;
		}
	}

	tlv = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL);
	if (tlv->length != 8 ||
		memcmp(tlv->value, kernel_id, tlv->length) != 0
	) {
		fprintf(stderr, "Incorrect Kernel Identifier - Terminal (96)\n");
		print_buf("Kernel Identifier - Terminal", tlv->value, tlv->length);
		print_buf("Expected", kernel_id, 8);
		return 1;
	}

	tlv = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9F39_POS_ENTRY_MODE);
	if (tlv->length != 1 ||
		tlv->value[0] != EMV_POS_ENTRY_MODE_CONTACTLESS_EMV
	) {
		fprintf(stderr, "Incorrect Point-of-Service (POS) Entry Mode (9F39)\n");
		print_buf("POS Entry Mode", tlv->value, tlv->length);
		return 1;
	}

	tlv = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9F06_AID);
	if (tlv->length != sizeof(test_aid) ||
		memcmp(tlv->value, test_aid, tlv->length) != 0
	) {
		fprintf(stderr, "Incorrect Application Identifier (AID) - terminal (9F06)\n");
		print_buf("AID", tlv->value, tlv->length);
		print_buf("Expected", test_aid, sizeof(test_aid));
		return 1;
	}

	tlv = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION);
	if (tlv->length != 2 ||
		memcmp(tlv->value, (uint8_t[]){ 0x00, 0x00 }, tlv->length) != 0
	) {
		fprintf(stderr, "Incorrect Transaction Status Information (9B)\n");
		print_buf("TSI", tlv->value, tlv->length);
		return 1;
	}

	tlv = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS);
	if (tlv->length != 5 ||
		memcmp(tlv->value, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x00 }, tlv->length) != 0
	) {
		fprintf(stderr, "Incorrect Terminal Verification Results (95)\n");
		print_buf("TVR", tlv->value, tlv->length);
		return 1;
	}

	// Unpredictable Number (9F37) is random and only the length can be
	// validated
	tlv = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9F37_UNPREDICTABLE_NUMBER);
	if (tlv->length != 4) {
		fprintf(stderr, "Incorrect Unpredictable Number (9F37)\n");
		print_buf("Unpredictable Number", tlv->value, tlv->length);
		return 1;
	}

	if (ttq) {
		tlv = emv_tlv_list_find_const(&ctx->terminal, EMV_TAG_9F66_TTQ);
		if (tlv->length != 4 ||
			memcmp(tlv->value, ttq, tlv->length) != 0
		) {
			fprintf(stderr, "Incorrect Terminal Transaction Qualifiers (9F66)\n");
			print_buf("TTQ", tlv->value, tlv->length);
			print_buf("Expected", ttq, 4);
			return 1;
		}
	}

	return 0;
}

int main(void)
{
	int r;
	struct emv_ttl_t ttl;
	struct emv_ctx_t emv;
	struct emv_config_app_t config_app;

	memset(&ttl, 0, sizeof(ttl));
	ttl.contactless = true;

	r = emv_debug_init(
		EMV_DEBUG_SOURCE_ALL,
		EMV_DEBUG_LEVEL_CARD,
		&print_emv_debug
	);
	if (r) {
		printf("Failed to initialise EMV debugging\n");
		return 1;
	}

	// Prepare EMV context for parameter validation
	config_app = test_config_app;
	r = emv_ctx_init(&emv, &ttl);
	if (r) {
		fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	printf("Testing invalid parameters...\n");
	r = emv_ep_create_common_kernel_data(NULL, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_ep_create_common_kernel_data() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	// Without a selected application
	r = emv_ep_create_common_kernel_data(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_ep_create_common_kernel_data() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	printf("Passed!\n\n");

	emv.selected_app = emv_app_create_from_fci(test_fci, sizeof(test_fci));
	if (!emv.selected_app) {
		fprintf(stderr, "emv_app_create_from_fci() failed; selected_app=%p\n", emv.selected_app);
		r = 1;
		goto exit;
	}

	printf("Testing selected application without configuration...\n");
	r = emv_ep_create_common_kernel_data(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	if (r != EMV_ERROR_INTERNAL) {
		fprintf(stderr, "Unexpected emv_ep_create_common_kernel_data() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	printf("Passed!\n\n");

	printf("Testing Entry Point for non-contactless card...\n");
	emv.selected_app->config = &config_app;
	ttl.contactless = false;
	r = emv_ep_create_common_kernel_data(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
	ttl.contactless = true;
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_ep_create_common_kernel_data() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	printf("Passed!\n\n");

	r = emv_ctx_clear(&emv);
	if (r) {
		fprintf(stderr, "emv_ctx_clear() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	for (size_t i = 0; i < sizeof(test) / sizeof(test[0]); ++i) {
		printf("Test %zu (%s)...\n", i + 1, test[i].name);

		// Prepare EMV context for current test
		r = emv_ctx_init(&emv, &ttl);
		if (r) {
			fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		r = populate_tlv_list(test[i].config_data, &emv.config.data);
		if (r) {
			fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		r = populate_tlv_list(test[i].params_data, &emv.params);
		if (r) {
			fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}

		// Prepare application configuration for current test
		config_app = test_config_app;
		r = populate_tlv_list(test[i].config_app_data, &config_app.data);
		if (r) {
			fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		config_app.contactless_floor_limit_enabled = test[i].contactless_floor_limit_enabled;
		config_app.contactless_floor_limit = test[i].contactless_floor_limit;
		config_app.contactless_cvm_required_limit_enabled = test[i].contactless_cvm_required_limit_enabled;
		config_app.contactless_cvm_required_limit = test[i].contactless_cvm_required_limit;

		// Prepare selected application for current test
		emv.selected_app = emv_app_create_from_fci(test_fci, sizeof(test_fci));
		if (!emv.selected_app) {
			fprintf(stderr, "emv_app_create_from_fci() failed; selected_app=%p\n", emv.selected_app);
			r = 1;
			goto exit;
		}
		// Kernel Identifier (9F2A) is only available in the PPSE directory
		// entry and is copied to the selected application by
		// emv_ep_select_application(). The remaining application data
		// originates from the FCI and must be preserved.
		r = append_tlv_list(test[i].icc_data, &emv.selected_app->tlv_list);
		if (r) {
			fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		emv.selected_app->config = &config_app;

		// Test creation of common contactless kernel data...
		r = emv_ep_create_common_kernel_data(&emv, EMV_POS_ENTRY_MODE_CONTACTLESS_EMV);
		if (r != test[i].result) {
			fprintf(stderr, "Unexpected emv_ep_create_common_kernel_data() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
			r = 1;
			goto exit;
		}
		if (test[i].kernel_id) {
			print_emv_tlv_list(&emv.terminal);
			r = verify_terminal_data(&emv, test[i].kernel_id, test[i].ttq);
			if (r) {
				r = 1;
				goto exit;
			}
		}

		emv_tlv_list_clear(&config_app.data);
		r = emv_ctx_clear(&emv);
		if (r) {
			fprintf(stderr, "emv_ctx_clear() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}

		printf("Passed!\n\n");
	}

	// Success
	printf("Success!\n");
	r = 0;
	goto exit;

exit:
	emv_tlv_list_clear(&config_app.data);
	if (emv.selected_app) {
		emv.selected_app->config = NULL;
	}
	emv_ctx_clear(&emv);

	return r;
}
