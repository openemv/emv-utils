/**
 * @file emv_ep_select_application_test.c
 * @brief Unit tests for EMV Entry Point application selection
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
#include "emv_cardreader_emul.h"
#include "emv_ttl.h"
#include "emv_tlv.h"
#include "emv_app.h"
#include "emv_config.h"
#include "emv_tags.h"
#include "emv_fields.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

// Reuse source data for all tests
static const struct emv_tlv_t test_param_data[] = {
	{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x07, 0x5B, 0xCD, 0x15 }, 0 }}, NULL }, // Numeric 123456789
	{ {{ EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, 6, (uint8_t[]){ 0x00, 0x01, 0x23, 0x45, 0x67, 0x89 }, 0 }}, NULL }, // Binary 0x75BCD15
};

static const uint8_t test_kernel_id_c2[] = { 0x02, 0x00, 0x00 };
static const uint8_t test_kernel_id_c3[] = { 0x03, 0x00, 0x00 };
static const uint8_t test_kernel_id_domestic[] = { 0x83, 0x05, 0x28 };
static const uint8_t test_kernel_id_rfu[] = { 0x43, 0x00, 0x00 };

// Proximity Payment System Environment (PPSE) test data for the FCI parsing
// tests that follow
static const struct xpdu_t test_ppse_mastercard[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT 2PAY.SYS.DDF01
		204, (uint8_t[]){
			0x6F, 0x81, 0xC7, 0x84, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59,
			0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x81, 0xB4, 0xBF, 0x0C,
			0x81, 0xB0,
			0x61, 0x14, 0x4F, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x01,
			0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x31, 0x87, 0x01, 0x01,
			0x61, 0x14, 0x4F, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x02,
			0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x32, 0x87, 0x01, 0x02,
			0x61, 0x14, 0x4F, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x03,
			0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x33, 0x87, 0x01, 0x03,
			0x61, 0x14, 0x4F, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x04,
			0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x34, 0x87, 0x01, 0x04,
			0x61, 0x14, 0x4F, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x05,
			0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x35, 0x87, 0x01, 0x05,
			0x61, 0x14, 0x4F, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x06,
			0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x36, 0x87, 0x01, 0x06,
			0x61, 0x14, 0x4F, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x07,
			0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x37, 0x87, 0x01, 0x07,
			0x61, 0x14, 0x4F, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x08,
			0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x38, 0x87, 0x01, 0x08,
			0x90, 0x00,
		}, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_select_app1[] = {
	{
		14, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x01, 0x00 }, // SELECT A000000004101001
		2, (uint8_t[]){ 0x6A, 0x82 }, // File or application not found
	},
	{ 0 }
};

static const struct xpdu_t test_select_app2[] = {
	{
		14, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x02, 0x00 }, // SELECT A000000004101002
		2, (uint8_t[]){ 0x6A, 0x81 }, // Function not supported
	},
	{ 0 }
};

static const struct xpdu_t test_select_app3[] = {
	{
		14, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x03, 0x00 }, // SELECT A000000004101003
		2, (uint8_t[]){ 0x62, 0x83 }, // Selected file deactivated
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x00 }, // GET RESPONSE
		2, (uint8_t[]){ 0x6C, 0x15 }, // 21 bytes available
	},
	{
		5, (uint8_t[]){ 0x00, 0xC0, 0x00, 0x00, 0x15 }, // GET RESPONSE Le=21
		23, (uint8_t[]){ 0x6F, 0x13, 0x84, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x03, 0xA5, 0x07, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x33, 0x90, 0x00 }, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_select_app4[] = {
	{
		14, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x04, 0x00 }, // SELECT A000000004101004
		23, (uint8_t[]){ 0x6F, 0x13, 0x85, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x04, 0xA5, 0x07, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x34, 0x90, 0x00 }, // Invalid FCI
	},
	{ 0 }
};

static const struct xpdu_t test_select_app5[] = {
	{
		14, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x05, 0x00 }, // SELECT A000000004101005
		23, (uint8_t[]){ 0x6F, 0x13, 0x84, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x09, 0xA5, 0x07, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x35, 0x90, 0x00 }, // FCI for A000000004101009
	},
	{ 0 }
};

static const struct xpdu_t test_select_app6[] = {
	{
		14, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x06, 0x00 }, // SELECT A000000004101006
		1, (uint8_t[]){ 0x00 }, // Invalid response
	},
	{ 0 }
};

static const struct xpdu_t test_select_app7[] = {
	{
		14, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x07, 0x00 }, // SELECT A000000004101007
		23, (uint8_t[]){ 0x6F, 0x13, 0x84, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x07, 0xA5, 0x07, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x37, 0x90, 0x00 }, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_select_app8[] = {
	{
		14, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x08, 0x00 }, // SELECT A000000004101008
		2, (uint8_t[]){ 0x6A, 0x82 }, // File or application not found
	},
	{ 0 }
};

// Proximity Payment System Environment (PPSE) test data for the kernel ID
// processing tests that follow
static const struct xpdu_t test_ppse_kernel_id[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT 2PAY.SYS.DDF01
		104, (uint8_t[]){
			0x6F, 0x64, 0x84, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53,
			0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x52, 0xBF, 0x0C, 0x4F,
			// International kernel ID of less than 3 bytes
			0x61, 0x17, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x50,
			0x05, 0x41, 0x50, 0x50, 0x20, 0x31, 0x87, 0x01, 0x01, 0x9F, 0x2A, 0x01,
			0x02,
			// Domestic EMVCo kernel ID with Extended Kernel ID
			0x61, 0x19, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x09, 0x12, 0x34, 0x56, 0x50,
			0x05, 0x41, 0x50, 0x50, 0x20, 0x32, 0x87, 0x01, 0x02, 0x9F, 0x2A, 0x03,
			0x83, 0x05, 0x28,
			// RFU kernel ID for which the Extended Kernel ID is ignored
			0x61, 0x19, 0x4F, 0x07, 0xA0, 0x00, 0x00, 0x09, 0x12, 0x34, 0x56, 0x50,
			0x05, 0x41, 0x50, 0x50, 0x20, 0x33, 0x87, 0x01, 0x03, 0x9F, 0x2A, 0x03,
			0x43, 0x05, 0x28,
			0x90, 0x00,
		}, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_select_kernel_id1[] = {
	{
		13, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x00 }, // SELECT A0000000041010
		22, (uint8_t[]){ 0x6F, 0x12, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0xA5, 0x07, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x31, 0x90, 0x00 }, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_select_kernel_id2[] = {
	{
		13, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x09, 0x12, 0x34, 0x56, 0x00 }, // SELECT A0000009123456
		22, (uint8_t[]){ 0x6F, 0x12, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x09, 0x12, 0x34, 0x56, 0xA5, 0x07, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x32, 0x90, 0x00 }, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_select_kernel_id3[] = {
	{
		13, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x09, 0x12, 0x34, 0x56, 0x00 }, // SELECT A0000009123456
		22, (uint8_t[]){ 0x6F, 0x12, 0x84, 0x07, 0xA0, 0x00, 0x00, 0x09, 0x12, 0x34, 0x56, 0xA5, 0x07, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x33, 0x90, 0x00 }, // FCI
	},
	{ 0 }
};

// Proximity Payment System Environment (PPSE) test data for the Visa PDOL
// validation tests that follow
static const struct xpdu_t test_ppse_visa[] = {
	{
		20, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59, 0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0x00 }, // SELECT 2PAY.SYS.DDF01
		136, (uint8_t[]){
			0x6F, 0x81, 0x83, 0x84, 0x0E, 0x32, 0x50, 0x41, 0x59, 0x2E, 0x53, 0x59,
			0x53, 0x2E, 0x44, 0x44, 0x46, 0x30, 0x31, 0xA5, 0x71, 0xBF, 0x0C, 0x6E,
			0x61, 0x14, 0x4F, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x01,
			0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x31, 0x87, 0x01, 0x01,
			0x61, 0x14, 0x4F, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x02,
			0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x32, 0x87, 0x01, 0x02,
			0x61, 0x14, 0x4F, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x03,
			0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x33, 0x87, 0x01, 0x03,
			0x61, 0x14, 0x4F, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x04,
			0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x34, 0x87, 0x01, 0x04,
			0x61, 0x14, 0x4F, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x05,
			0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x35, 0x87, 0x01, 0x05,
			0x90, 0x00,
		}, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_select_visa1[] = {
	{
		14, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x01, 0x00 }, // SELECT A000000003101001
		23, (uint8_t[]){
			0x6F, 0x13, 0x84, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x01,
			0xA5, 0x07, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x31,
			0x90, 0x00,
		}, // FCI without PDOL
	},
	{ 0 }
};

static const struct xpdu_t test_select_visa2[] = {
	{
		14, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x02, 0x00 }, // SELECT A000000003101002
		32, (uint8_t[]){
			0x6F, 0x1C, 0x84, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x02,
			0xA5, 0x10, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x32,
			// PDOL without Terminal Transaction Qualifiers (9F66)
			0x9F, 0x38, 0x06, 0x9F, 0x02, 0x06, 0x9F, 0x03, 0x06,
			0x90, 0x00,
		}, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_select_visa3[] = {
	{
		14, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x03, 0x00 }, // SELECT A000000003101003
		28, (uint8_t[]){
			0x6F, 0x18, 0x84, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x03,
			0xA5, 0x0C, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x33,
			// Malformed PDOL
			0x9F, 0x38, 0x02, 0x9F, 0x66,
			0x90, 0x00,
		}, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_select_visa4[] = {
	{
		14, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x04, 0x00 }, // SELECT A000000003101004
		32, (uint8_t[]){
			0x6F, 0x1C, 0x84, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x04,
			0xA5, 0x10, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x34,
			// PDOL with Terminal Transaction Qualifiers (9F66)
			0x9F, 0x38, 0x06, 0x9F, 0x66, 0x04, 0x9F, 0x02, 0x06,
			0x90, 0x00,
		}, // FCI
	},
	{ 0 }
};

static const struct xpdu_t test_select_visa5[] = {
	{
		14, (uint8_t[]){ 0x00, 0xA4, 0x04, 0x00, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x05, 0x00 }, // SELECT A000000003101005
		23, (uint8_t[]){
			0x6F, 0x13, 0x84, 0x08, 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10, 0x05,
			0xA5, 0x07, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x35,
			0x90, 0x00,
		}, // FCI without PDOL
	},
	{ 0 }
};

static const uint8_t test_dir_entry[] = {
	0x4F, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10, 0x50, 0x05, 0x41, 0x50, 0x50, 0x20, 0x31, 0x87, 0x01, 0x01,
};
static const struct emv_config_app_t test_config_app = { // Combination for kernel C-2
	.aid = { 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10 },
	.aid_len = 7,
	.asi = EMV_ASI_PARTIAL_MATCH,
};
static const struct emv_tlv_t test_config_app_invalid_data[] = { // Invalid Kernel Identifier - Terminal (96) length
	{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 3, (uint8_t[]){ 0x02, 0x00, 0x00 }, 0 }}, NULL },
};

static int populate_tlv_list(
	const struct emv_tlv_t* tlv_array,
	size_t tlv_array_count,
	struct emv_tlv_list_t* list
)
{
	int r;

	emv_tlv_list_clear(list);
	for (size_t i = 0; i < tlv_array_count; ++i) {
		r = emv_tlv_list_push(list, tlv_array[i].tag, tlv_array[i].length, tlv_array[i].value, 0);
		if (r) {
			return r;
		}
	}

	return 0;
}

static bool verify_app_list_numbers(
	const struct emv_app_list_t* app_list,
	const unsigned int* numbers,
	size_t numbers_len
)
{
	struct emv_app_t* app;
	size_t i;

	i = 0;
	for (app = app_list->front; app != NULL; app = app->next) {
		if (i >= numbers_len) {
			return false;
		}

		// Use application display name to validate sorted app order
		char tmp[] = "APP x";
		tmp[4] = '0' + numbers[i];
		if (strcmp(tmp, app->display_name) != 0) {
			return false;
		}

		++i;
	}

	if (i != numbers_len) {
		return false;
	}

	return true;
}

static int build_candidate_list(
	struct emv_ctx_t* ctx,
	struct emv_cardreader_emul_ctx_t* emul_ctx,
	const struct xpdu_t* xpdu_list,
	struct emv_ep_app_list_t* ep_list,
	struct emv_app_list_t* app_list
)
{
	int r;

	// Silence debugging logs for rebuilding candidate application list
	r = emv_debug_init(
		EMV_DEBUG_SOURCE_NONE,
		EMV_DEBUG_LEVEL_NONE,
		NULL
	);
	if (r) {
		fprintf(stderr, "Failed to initialise EMV debugging\n");
		return r;
	}

	emul_ctx->xpdu_list = xpdu_list;
	emul_ctx->xpdu_current = NULL;
	emv_app_list_clear(app_list);
	r = emv_ep_build_candidate_list(ctx, ep_list, app_list);
	if (r) {
		fprintf(stderr, "emv_ep_build_candidate_list() failed; r=%d\n", r);
		return r;
	}

	// Restore debugging logs
	r = emv_debug_init(
		EMV_DEBUG_SOURCE_ALL,
		EMV_DEBUG_LEVEL_CARD,
		&print_emv_debug
	);
	if (r) {
		fprintf(stderr, "Failed to initialise EMV debugging\n");
		return r;
	}

	return 0;
}

int main(void)
{
	int r;
	struct emv_cardreader_emul_ctx_t emul_ctx;
	struct emv_ttl_t ttl;
	struct emv_ctx_t emv;
	struct emv_tlv_list_t app_data = EMV_TLV_LIST_INIT;
	struct emv_ep_app_list_t ep_list = EMV_EP_APP_LIST_INIT;
	struct emv_app_list_t app_list = EMV_APP_LIST_INIT;
	struct emv_config_app_t config_app;
	struct emv_app_t* app;
	uint8_t kernel_id[3];

	memset(&ttl, 0, sizeof(ttl));
	ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
	ttl.cardreader.ctx = &emul_ctx;
	ttl.cardreader.trx = &emv_cardreader_emul;
	ttl.contactless = true;

	r = emv_ctx_init(&emv, &ttl);
	if (r) {
		fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	// Populate transaction parameters
	r = populate_tlv_list(test_param_data, sizeof(test_param_data) / sizeof(test_param_data[0]), &emv.params);
	if (r) {
		fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	// Supported applications
	r = emv_tlv_list_push(&app_data, EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 ); // Kernel C-2
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_config_app_create(&emv, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x04, 0x10, 0x10 }, 7, EMV_ASI_PARTIAL_MATCH, &app_data, NULL); // Mastercard
	if (r) {
		fprintf(stderr, "emv_config_app_create() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	r = emv_tlv_list_push(&app_data, EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 ); // Kernel C-3
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_config_app_create(&emv, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x00, 0x03, 0x10, 0x10 }, 7, EMV_ASI_PARTIAL_MATCH, &app_data, NULL); // Visa Credit/Debit
	if (r) {
		fprintf(stderr, "emv_config_app_create() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	r = emv_tlv_list_push(&app_data, EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x83, 0x05, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 ); // Domestic EMVCo kernel
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_config_app_create(&emv, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x09, 0x12, 0x34, 0x56 }, 7, EMV_ASI_PARTIAL_MATCH, &app_data, NULL); // Domestic
	if (r) {
		fprintf(stderr, "emv_config_app_create() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	r = emv_tlv_list_push(&app_data, EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x43, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 ); // RFU kernel
	if (r) {
		fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}
	r = emv_config_app_create(&emv, (uint8_t[]){ 0xA0, 0x00, 0x00, 0x09, 0x12, 0x34, 0x56 }, 7, EMV_ASI_PARTIAL_MATCH, &app_data, NULL); // Domestic
	if (r) {
		fprintf(stderr, "emv_config_app_create() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	r = emv_debug_init(
		EMV_DEBUG_SOURCE_ALL,
		EMV_DEBUG_LEVEL_CARD,
		&print_emv_debug
	);
	if (r) {
		printf("Failed to initialise EMV debugging\n");
		r = 1;
		goto exit;
	}

	r = emv_ep_preprocess(&emv.config, 3000, &ep_list);
	if (r) {
		fprintf(stderr, "emv_ep_preprocess() failed; r=%d\n", r);
		r = 1;
		goto exit;
	}

	printf("\nTesting invalid parameters...\n");
	emul_ctx.xpdu_list = NULL;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(NULL, &app_list, kernel_id);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	r = emv_ep_select_application(&emv, NULL, kernel_id);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current) {
		fprintf(stderr, "Card interaction while there should have been none\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting Entry Point for non-contactless card...\n");
	emul_ctx.xpdu_list = NULL;
	emul_ctx.xpdu_current = NULL;
	ttl.contactless = false;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	ttl.contactless = true;
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current) {
		fprintf(stderr, "Card interaction while there should have been none\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting empty candidate application list...\n");
	emul_ctx.xpdu_list = NULL;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_ERROR_INVALID_PARAMETER) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current) {
		fprintf(stderr, "Card interaction while there should have been none\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	r = build_candidate_list(&emv, &emul_ctx, test_ppse_mastercard, &ep_list, &app_list);
	if (r) {
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 1, 2, 3, 4, 5, 6, 7, 8 }, 8)) {
		fprintf(stderr, "Invalid candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}

	printf("\nTesting application not found...\n");
	emul_ctx.xpdu_list = test_select_app1;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_OUTCOME_SELECT_NEXT) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv.selected_app != NULL) {
		fprintf(stderr, "emv_ep_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 2, 3, 4, 5, 6, 7, 8 }, 7)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting card blocked or SELECT not supported...\n");
	emul_ctx.xpdu_list = test_select_app2;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_OUTCOME_SELECT_NEXT) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv.selected_app != NULL) {
		fprintf(stderr, "emv_ep_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 3, 4, 5, 6, 7, 8 }, 6)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting application blocked...\n");
	emul_ctx.xpdu_list = test_select_app3;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_OUTCOME_SELECT_NEXT) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv.selected_app != NULL) {
		fprintf(stderr, "emv_ep_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 4, 5, 6, 7, 8 }, 5)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting invalid application FCI...\n");
	emul_ctx.xpdu_list = test_select_app4;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_OUTCOME_SELECT_NEXT) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv.selected_app != NULL) {
		fprintf(stderr, "emv_ep_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 5, 6, 7, 8 }, 4)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting application DF Name mismatch...\n");
	emul_ctx.xpdu_list = test_select_app5;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_OUTCOME_SELECT_NEXT) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv.selected_app != NULL) {
		fprintf(stderr, "emv_ep_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 6, 7, 8 }, 3)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting card error during application selection...\n");
	emul_ctx.xpdu_list = test_select_app6;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_OUTCOME_CARD_ERROR) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv.selected_app != NULL) {
		fprintf(stderr, "emv_ep_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 7, 8 }, 2)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting successful application selection...\n");
	emul_ctx.xpdu_list = test_select_app7;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!emv.selected_app) {
		fprintf(stderr, "emv_ep_select_application() failed to populate selected_app\n");
		r = 1;
		goto exit;
	}
	print_emv_app(emv.selected_app);
	if (emv.selected_app->config != emv_config_app_find_supported(&emv.config, emv.selected_app)) {
		fprintf(stderr, "Incorrect application configuration for selected application\n");
		r = 1;
		goto exit;
	}
	if (memcmp(kernel_id, test_kernel_id_c2, sizeof(kernel_id)) != 0) {
		fprintf(stderr, "Incorrect selected kernel ID\n");
		print_buf("Kernel ID", kernel_id, sizeof(kernel_id));
		print_buf("Expected", test_kernel_id_c2, sizeof(test_kernel_id_c2));
		r = 1;
		goto exit;
	}
	if (emv_tlv_list_find_const(&emv.selected_app->tlv_list, EMV_TAG_9F2A_KERNEL_IDENTIFIER)) {
		fprintf(stderr, "Kernel Identifier (9F2A) unexpectedly found\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 8 }, 1)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting application selection failure for last candidate...\n");
	emul_ctx.xpdu_list = test_select_app8;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	// Previously selected application must have been freed
	if (emv.selected_app != NULL) {
		fprintf(stderr, "emv_ep_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	if (!emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Candidate application list unexpectedly NOT empty\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	r = build_candidate_list(&emv, &emul_ctx, test_ppse_kernel_id, &ep_list, &app_list);
	if (r) {
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 1, 2, 3 }, 3)) {
		fprintf(stderr, "Invalid candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}

	printf("\nTesting selected kernel ID for international kernel ID...\n");
	emul_ctx.xpdu_list = test_select_kernel_id1;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!emv.selected_app) {
		fprintf(stderr, "emv_ep_select_application() failed to populate selected_app\n");
		r = 1;
		goto exit;
	}
	print_emv_app(emv.selected_app);
	if (memcmp(kernel_id, test_kernel_id_c2, sizeof(kernel_id)) != 0) {
		fprintf(stderr, "Incorrect selected kernel ID\n");
		print_buf("Kernel ID", kernel_id, sizeof(kernel_id));
		print_buf("Expected", test_kernel_id_c2, sizeof(test_kernel_id_c2));
		r = 1;
		goto exit;
	}
	// Kernel Identifier (9F2A) is only available in the PPSE directory entry
	// and must therefore be copied to the selected application
	if (!emv_tlv_list_find_const(&emv.selected_app->tlv_list, EMV_TAG_9F2A_KERNEL_IDENTIFIER)) {
		fprintf(stderr, "Failed to find Kernel Identifier (9F2A)\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting selected kernel ID for domestic kernel ID...\n");
	emul_ctx.xpdu_list = test_select_kernel_id2;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!emv.selected_app) {
		fprintf(stderr, "emv_ep_select_application() failed to populate selected_app\n");
		r = 1;
		goto exit;
	}
	print_emv_app(emv.selected_app);
	if (memcmp(kernel_id, test_kernel_id_domestic, sizeof(kernel_id)) != 0) {
		fprintf(stderr, "Incorrect selected kernel ID\n");
		print_buf("Kernel ID", kernel_id, sizeof(kernel_id));
		print_buf("Expected", test_kernel_id_domestic, sizeof(test_kernel_id_domestic));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting selected kernel ID for RFU kernel ID...\n");
	emul_ctx.xpdu_list = test_select_kernel_id3;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!emv.selected_app) {
		fprintf(stderr, "emv_ep_select_application() failed to populate selected_app\n");
		r = 1;
		goto exit;
	}
	print_emv_app(emv.selected_app);
	if (memcmp(kernel_id, test_kernel_id_rfu, sizeof(kernel_id)) != 0) {
		fprintf(stderr, "Incorrect selected kernel ID\n");
		print_buf("Kernel ID", kernel_id, sizeof(kernel_id));
		print_buf("Expected", test_kernel_id_rfu, sizeof(test_kernel_id_rfu));
		r = 1;
		goto exit;
	}
	printf("Success\n");

	r = build_candidate_list(&emv, &emul_ctx, test_ppse_visa, &ep_list, &app_list);
	if (r) {
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 1, 2, 3, 4, 5 }, 5)) {
		fprintf(stderr, "Invalid candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}

	printf("\nTesting Visa kernel 3 without PDOL...\n");
	emul_ctx.xpdu_list = test_select_visa1;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_OUTCOME_SELECT_NEXT) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 2, 3, 4, 5 }, 4)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting Visa kernel 3 with PDOL that has no TTQ...\n");
	emul_ctx.xpdu_list = test_select_visa2;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_OUTCOME_SELECT_NEXT) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 3, 4, 5 }, 3)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting Visa kernel 3 with invalid PDOL...\n");
	emul_ctx.xpdu_list = test_select_visa3;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_OUTCOME_SELECT_NEXT) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 4, 5 }, 2)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting Visa kernel 3 with PDOL that has TTQ...\n");
	emul_ctx.xpdu_list = test_select_visa4;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!emv.selected_app) {
		fprintf(stderr, "emv_ep_select_application() failed to populate selected_app\n");
		r = 1;
		goto exit;
	}
	print_emv_app(emv.selected_app);
	if (memcmp(kernel_id, test_kernel_id_c3, sizeof(kernel_id)) != 0) {
		fprintf(stderr, "Incorrect selected kernel ID\n");
		print_buf("Kernel ID", kernel_id, sizeof(kernel_id));
		print_buf("Expected", test_kernel_id_c3, sizeof(test_kernel_id_c3));
		r = 1;
		goto exit;
	}
	if (!verify_app_list_numbers(&app_list, (unsigned int[]){ 5 }, 1)) {
		fprintf(stderr, "Invalid remaining candidate application list\n");
		for (struct emv_app_t* a = app_list.front; a != NULL; a = a->next) {
			print_emv_app(a);
		}
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting Visa kernel 3 without PDOL for last candidate...\n");
	emul_ctx.xpdu_list = test_select_visa5;
	emul_ctx.xpdu_current = NULL;
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (!emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Candidate application list unexpectedly NOT empty\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting candidate application without configuration...\n");
	emul_ctx.xpdu_list = NULL;
	emul_ctx.xpdu_current = NULL;
	emv_app_list_clear(&app_list);
	app = emv_app_create_from_ppse_dir_entry(test_dir_entry, sizeof(test_dir_entry));
	if (!app) {
		fprintf(stderr, "emv_app_create_from_ppse_dir_entry() failed; app=%p\n", app);
		r = 1;
		goto exit;
	}
	r = emv_app_list_push(&app_list, app);
	if (r) {
		fprintf(stderr, "emv_app_list_push() failed; r=%d\n", r);
		emv_app_free(app);
		r = 1;
		goto exit;
	}
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current) {
		fprintf(stderr, "Card interaction while there should have been none\n");
		r = 1;
		goto exit;
	}
	if (!emv_app_list_is_empty(&app_list)) {
		fprintf(stderr, "Candidate application list unexpectedly NOT empty\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting candidate application without Kernel Identifier - Terminal (96)...\n");
	emul_ctx.xpdu_list = test_select_kernel_id1;
	emul_ctx.xpdu_current = NULL;
	emv_app_list_clear(&app_list);
	app = emv_app_create_from_ppse_dir_entry(test_dir_entry, sizeof(test_dir_entry));
	if (!app) {
		fprintf(stderr, "emv_app_create_from_ppse_dir_entry() failed; app=%p\n", app);
		r = 1;
		goto exit;
	}
	config_app = test_config_app;
	app->config = &config_app;
	r = emv_app_list_push(&app_list, app);
	if (r) {
		fprintf(stderr, "emv_app_list_push() failed; r=%d\n", r);
		emv_app_free(app);
		r = 1;
		goto exit;
	}
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_ERROR_INVALID_CONFIG) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv.selected_app != NULL) {
		fprintf(stderr, "emv_ep_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	printf("Success\n");

	printf("\nTesting candidate application with invalid Kernel Identifier - Terminal (96) length...\n");
	emul_ctx.xpdu_list = test_select_kernel_id1;
	emul_ctx.xpdu_current = NULL;
	emv_app_list_clear(&app_list);
	app = emv_app_create_from_ppse_dir_entry(test_dir_entry, sizeof(test_dir_entry));
	if (!app) {
		fprintf(stderr, "emv_app_create_from_ppse_dir_entry() failed; app=%p\n", app);
		r = 1;
		goto exit;
	}
	config_app = test_config_app;
	r = populate_tlv_list(test_config_app_invalid_data, sizeof(test_config_app_invalid_data) / sizeof(test_config_app_invalid_data[0]), &config_app.data);
	if (r) {
		fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
		emv_app_free(app);
		r = 1;
		goto exit;
	}
	app->config = &config_app;
	r = emv_app_list_push(&app_list, app);
	if (r) {
		fprintf(stderr, "emv_app_list_push() failed; r=%d\n", r);
		emv_app_free(app);
		r = 1;
		goto exit;
	}
	r = emv_ep_select_application(&emv, &app_list, kernel_id);
	if (r != EMV_ERROR_INVALID_CONFIG) {
		fprintf(stderr, "Unexpected emv_ep_select_application() result; error %d: %s\n", r, r < 0 ? emv_error_get_string(r) : emv_outcome_get_string(r));
		r = 1;
		goto exit;
	}
	if (emul_ctx.xpdu_current->c_xpdu_len != 0) {
		fprintf(stderr, "Incomplete card interaction\n");
		r = 1;
		goto exit;
	}
	if (emv.selected_app != NULL) {
		fprintf(stderr, "emv_ep_select_application() failed to zero selected_app\n");
		r = 1;
		goto exit;
	}
	emv_tlv_list_clear(&config_app.data);
	printf("Success\n");

	// Success
	r = 0;
	goto exit;

exit:
	emv_ctx_clear(&emv);
	emv_ep_app_list_clear(&ep_list);
	emv_app_list_clear(&app_list);

	return r;
}
