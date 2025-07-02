/**
 * @file emv_terminal_risk_management_test.c
 * @brief Unit tests for EMV Terminal Risk Management
 *
 * Copyright 2025 Leon Lynch
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

#include "emv.h"
#include "emv_cardreader_emul.h"
#include "emv_tlv.h"
#include "emv_tags.h"
#include "emv_fields.h"
#include "emv_ttl.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

struct test_t {
	const char* name;

	struct emv_tlv_t* config_data;
	struct emv_tlv_t* params_data;
	struct emv_tlv_t* icc_data;

	const struct emv_txn_log_entry_t* txn_log;
	size_t txn_log_cnt;

	const struct xpdu_t* xpdu_list;

	uint8_t tvr[5];
	uint8_t tsi[2];
};

static const struct test_t test[] = {
	{
		.name = "No risk found",

		.config_data = (struct emv_tlv_t[]){
			// Floor limit: 100.00
			{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 50.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x13, 0x88 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// PAN: 5413330089020011
			{ {{ EMV_TAG_5A_APPLICATION_PAN, 8, (uint8_t[]){ 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x11 }, 0 }}, NULL },
			// Lower Consecutive Offline Limit: 18
			{ {{ EMV_TAG_9F14_LOWER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x12 }, 0 }}, NULL },
			// Upper Consecutive Offline Limit: 52
			{ {{ EMV_TAG_9F23_UPPER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x34 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_log = (const struct emv_txn_log_entry_t[]){
			{
				{ 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x11, 0xFF, 0xFF },
				0x01,
				{ 0x25, 0x07, 0x01 },
				0x9999, // 39321
			},
			{
				{ 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x11, 0xFF, 0xFF },
				0x01,
				{ 0x25, 0x07, 0x02 },
				0x1234, // 4660
			},
			{
				{ 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x01, 0x19, 0xFF, 0xFF },
				0x01,
				{ 0x25, 0x07, 0x03 },
				0x9999, // 39321
			},
		},
		.txn_log_cnt = 3,

		.xpdu_list = (const struct xpdu_t[]){
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x36, 0x00 }, // GET DATA [9F36]
				7, (uint8_t[]){ 0x9F, 0x36, 0x02, 0x04, 0xD2, 0x90, 0x00 }, // 9F36 is 1234
			},
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x13, 0x00 }, // GET DATA [9F13]
				7, (uint8_t[]){ 0x9F, 0x13, 0x02, 0x04, 0xD1, 0x90, 0x00 }, // 9F13 is 1233
			},
			{ 0 }
		},

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},

	{
		.name = "Floor limit exceeded without transaction log",

		.config_data = (struct emv_tlv_t[]){
			// Floor limit: 100.00
			{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 150.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x3A, 0x98 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// PAN: 5413330089020011
			{ {{ EMV_TAG_5A_APPLICATION_PAN, 8, (uint8_t[]){ 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x11 }, 0 }}, NULL },
			// No velocity checking
			{ {{ 0 }} },
		},

		.xpdu_list = NULL,

		.tvr = { 0x00, 0x00, 0x00, EMV_TVR_TXN_FLOOR_LIMIT_EXCEEDED, 0x00 },
		.tsi = { EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},

	{
		.name = "Floor limit exceeded regardless of transaction log",

		.config_data = (struct emv_tlv_t[]){
			// Floor limit: 100.00
			{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 150.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x3A, 0x98 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// PAN: 5413330089020011
			{ {{ EMV_TAG_5A_APPLICATION_PAN, 8, (uint8_t[]){ 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x11 }, 0 }}, NULL },
			// No velocity checking
			{ {{ 0 }} },
		},

		.txn_log = (const struct emv_txn_log_entry_t[]){
			{
				{ 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x11, 0xFF, 0xFF },
				0x01,
				{ 0x25, 0x07, 0x01 },
				1,
			},
			{
				{ 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x11, 0xFF, 0xFF },
				0x01,
				{ 0x25, 0x07, 0x02 },
				1,
			},
			{
				{ 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x01, 0x19, 0xFF, 0xFF },
				0x01,
				{ 0x25, 0x07, 0x03 },
				1,
			},
		},
		.txn_log_cnt = 3,

		.xpdu_list = NULL,

		.tvr = { 0x00, 0x00, 0x00, EMV_TVR_TXN_FLOOR_LIMIT_EXCEEDED, 0x00 },
		.tsi = { EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},

	{
		.name = "Floor limit exceeded due to transaction log",

		.config_data = (struct emv_tlv_t[]){
			// Floor limit: 100.00
			{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 50.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x13, 0x88 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// PAN: 5413330089020011
			{ {{ EMV_TAG_5A_APPLICATION_PAN, 8, (uint8_t[]){ 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x11 }, 0 }}, NULL },
			// No velocity checking
			{ {{ 0 }} },
		},

		.txn_log = (const struct emv_txn_log_entry_t[]){
			{
				{ 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x11, 0xFF, 0xFF },
				0x01,
				{ 0x25, 0x07, 0x01 },
				0x9999, // 39321
			},
			{
				{ 0x54, 0x13, 0x33, 0x00, 0x89, 0x02, 0x00, 0x12, 0xFF, 0xFF },
				0x01,
				{ 0x25, 0x07, 0x02 },
				0x1234, // 4660
			},
			{
				{ 0x47, 0x61, 0x73, 0x90, 0x01, 0x01, 0x01, 0x19, 0xFF, 0xFF },
				0x01,
				{ 0x25, 0x07, 0x03 },
				0x1234, // 4660
			},
		},
		.txn_log_cnt = 3,

		.xpdu_list = NULL,

		.tvr = { 0x00, 0x00, 0x00, EMV_TVR_TXN_FLOOR_LIMIT_EXCEEDED, 0x00 },
		.tsi = { EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},

	{
		.name = "Velocity checking not possible",

		.config_data = (struct emv_tlv_t[]){
			// Floor limit: 100.00
			{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 50.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x13, 0x88 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// Lower Consecutive Offline Limit: 18
			{ {{ EMV_TAG_9F14_LOWER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x12 }, 0 }}, NULL },
			// Upper Consecutive Offline Limit: 52
			{ {{ EMV_TAG_9F23_UPPER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x34 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.xpdu_list = (const struct xpdu_t[]){
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x36, 0x00 }, // GET DATA [9F36]
				2, (uint8_t[]){ 0x6A, 0x81 }, // Function not supported
			},
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x13, 0x00 }, // GET DATA [9F13]
				2, (uint8_t[]){ 0x6A, 0x81 }, // Function not supported
			},
			{ 0 }
		},

		.tvr = { 0x00, 0x00, 0x00, EMV_TVR_LOWER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED | EMV_TVR_UPPER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED, 0x00 },
		.tsi = { EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},

	{
		.name = "Lower Consecutive Offline Limit matched",

		.config_data = (struct emv_tlv_t[]){
			// Floor limit: 100.00
			{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 50.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x13, 0x88 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// Lower Consecutive Offline Limit: 18
			{ {{ EMV_TAG_9F14_LOWER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x12 }, 0 }}, NULL },
			// Upper Consecutive Offline Limit: 52
			{ {{ EMV_TAG_9F23_UPPER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x34 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.xpdu_list = (const struct xpdu_t[]){
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x36, 0x00 }, // GET DATA [9F36]
				7, (uint8_t[]){ 0x9F, 0x36, 0x02, 0x04, 0xD2, 0x90, 0x00 }, // 9F36 is 1234
			},
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x13, 0x00 }, // GET DATA [9F13]
				7, (uint8_t[]){ 0x9F, 0x13, 0x02, 0x04, 0xC0, 0x90, 0x00 }, // 9F13 is 1216
			},
			{ 0 }
		},

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},

	{
		.name = "Lower Consecutive Offline Limit exceeded",

		.config_data = (struct emv_tlv_t[]){
			// Floor limit: 100.00
			{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 50.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x13, 0x88 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// Lower Consecutive Offline Limit: 18
			{ {{ EMV_TAG_9F14_LOWER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x12 }, 0 }}, NULL },
			// Upper Consecutive Offline Limit: 52
			{ {{ EMV_TAG_9F23_UPPER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x34 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.xpdu_list = (const struct xpdu_t[]){
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x36, 0x00 }, // GET DATA [9F36]
				7, (uint8_t[]){ 0x9F, 0x36, 0x02, 0x04, 0xD2, 0x90, 0x00 }, // 9F36 is 1234
			},
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x13, 0x00 }, // GET DATA [9F13]
				7, (uint8_t[]){ 0x9F, 0x13, 0x02, 0x04, 0xBF, 0x90, 0x00 }, // 9F13 is 1215
			},
			{ 0 }
		},

		.tvr = { 0x00, 0x00, 0x00, EMV_TVR_LOWER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED, 0x00 },
		.tsi = { EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},

	{
		.name = "Upper Consecutive Offline Limit matched",

		.config_data = (struct emv_tlv_t[]){
			// Floor limit: 100.00
			{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 50.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x13, 0x88 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// Lower Consecutive Offline Limit: 18
			{ {{ EMV_TAG_9F14_LOWER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x12 }, 0 }}, NULL },
			// Upper Consecutive Offline Limit: 52
			{ {{ EMV_TAG_9F23_UPPER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x34 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.xpdu_list = (const struct xpdu_t[]){
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x36, 0x00 }, // GET DATA [9F36]
				7, (uint8_t[]){ 0x9F, 0x36, 0x02, 0x04, 0xD2, 0x90, 0x00 }, // 9F36 is 1234
			},
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x13, 0x00 }, // GET DATA [9F13]
				7, (uint8_t[]){ 0x9F, 0x13, 0x02, 0x04, 0x9E, 0x90, 0x00 }, // 9F13 is 1182
			},
			{ 0 }
		},

		.tvr = { 0x00, 0x00, 0x00, EMV_TVR_LOWER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED, 0x00 },
		.tsi = { EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},

	{
		.name = "Upper Consecutive Offline Limit exceeded",

		.config_data = (struct emv_tlv_t[]){
			// Floor limit: 100.00
			{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 50.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x13, 0x88 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// Lower Consecutive Offline Limit: 18
			{ {{ EMV_TAG_9F14_LOWER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x12 }, 0 }}, NULL },
			// Upper Consecutive Offline Limit: 52
			{ {{ EMV_TAG_9F23_UPPER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x34 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.xpdu_list = (const struct xpdu_t[]){
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x36, 0x00 }, // GET DATA [9F36]
				7, (uint8_t[]){ 0x9F, 0x36, 0x02, 0x04, 0xD2, 0x90, 0x00 }, // 9F36 is 1234
			},
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x13, 0x00 }, // GET DATA [9F13]
				7, (uint8_t[]){ 0x9F, 0x13, 0x02, 0x04, 0x9D, 0x90, 0x00 }, // 9F13 is 1181
			},
			{ 0 }
		},

		.tvr = { 0x00, 0x00, 0x00, EMV_TVR_LOWER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED | EMV_TVR_UPPER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED, 0x00 },
		.tsi = { EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},

	{
		.name = "New card",

		.config_data = (struct emv_tlv_t[]){
			// Floor limit: 100.00
			{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 50.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x13, 0x88 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// Lower Consecutive Offline Limit: 18
			{ {{ EMV_TAG_9F14_LOWER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x12 }, 0 }}, NULL },
			// Upper Consecutive Offline Limit: 52
			{ {{ EMV_TAG_9F23_UPPER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x34 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.xpdu_list = (const struct xpdu_t[]){
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x36, 0x00 }, // GET DATA [9F36]
				7, (uint8_t[]){ 0x9F, 0x36, 0x02, 0x00, 0x05, 0x90, 0x00 }, // 9F36 is 5
			},
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x13, 0x00 }, // GET DATA [9F13]
				7, (uint8_t[]){ 0x9F, 0x13, 0x02, 0x00, 0x00, 0x90, 0x00 }, // 9F13 is 0
			},
			{ 0 }
		},

		.tvr = { 0x00, EMV_TVR_NEW_CARD, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},

	{
		.name = "New card with no ATC",

		.config_data = (struct emv_tlv_t[]){
			// Floor limit: 100.00
			{ {{ EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Amount, Authorised (Binary): 50.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x13, 0x88 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// Lower Consecutive Offline Limit: 18
			{ {{ EMV_TAG_9F14_LOWER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x12 }, 0 }}, NULL },
			// Upper Consecutive Offline Limit: 52
			{ {{ EMV_TAG_9F23_UPPER_CONSECUTIVE_OFFLINE_LIMIT, 1, (uint8_t[]){ 0x34 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.xpdu_list = (const struct xpdu_t[]){
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x36, 0x00 }, // GET DATA [9F36]
				2, (uint8_t[]){ 0x6A, 0x81 }, // Function not supported
			},
			{
				5, (uint8_t[]){ 0x80, 0xCA, 0x9F, 0x13, 0x00 }, // GET DATA [9F13]
				7, (uint8_t[]){ 0x9F, 0x13, 0x02, 0x00, 0x00, 0x90, 0x00 }, // 9F13 is 0
			},
			{ 0 }
		},

		.tvr = { 0x00, EMV_TVR_NEW_CARD, 0x00, EMV_TVR_LOWER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED | EMV_TVR_UPPER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED, 0x00 },
		.tsi = { EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED, 0x00 },
	},
};

static int populate_tlv_list(
	const struct emv_tlv_t* tlv_array,
	struct emv_tlv_list_t* list
)
{
	int r;

	emv_tlv_list_clear(list);
	while (tlv_array && tlv_array->tag) {
		r = emv_tlv_list_push(list, tlv_array->tag, tlv_array->length, tlv_array->value, 0);
		if (r) {
			return r;
		}

		++tlv_array;
	}
	return 0;
}

int main(void)
{
	int r;
	struct emv_cardreader_emul_ctx_t emul_ctx;
	struct emv_ttl_t ttl;
	struct emv_ctx_t emv;

	ttl.cardreader.mode = EMV_CARDREADER_MODE_APDU;
	ttl.cardreader.ctx = &emul_ctx;
	ttl.cardreader.trx = &emv_cardreader_emul;

	r = emv_debug_init(
		EMV_DEBUG_SOURCE_ALL,
		EMV_DEBUG_CARD,
		&print_emv_debug
	);
	if (r) {
		printf("Failed to initialise EMV debugging\n");
		return 1;
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
		r = populate_tlv_list(
			test[i].config_data,
			&emv.config
		);
		if (r) {
			fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.config);
		r = populate_tlv_list(
			test[i].params_data,
			&emv.params
		);
		if (r) {
			fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.params);
		r = populate_tlv_list(
			test[i].icc_data,
			&emv.icc
		);
		if (r) {
			fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.icc);
		r = emv_tlv_list_push(&emv.terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS, 5, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x00 }, 0);
		if (r) {
			fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		r = emv_tlv_list_push(&emv.terminal, EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION, 2, (uint8_t[]){ 0x00, 0x00 }, 0);
		if (r) {
			fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		emv.tvr = emv_tlv_list_find(&emv.terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS);
		emv.tsi = emv_tlv_list_find(&emv.terminal, EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION);

		// Prepare card emulation for current test
		emul_ctx.xpdu_list = test[i].xpdu_list;
		emul_ctx.xpdu_current = NULL;

		// Test terminal risk management...
		r = emv_terminal_risk_management(
			&emv,
			test[i].txn_log,
			test[i].txn_log_cnt
		);
		if (r) {
			fprintf(stderr, "emv_terminal_risk_management() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		if (test[i].xpdu_list && emul_ctx.xpdu_current->c_xpdu_len != 0) {
			fprintf(stderr, "Incomplete card interaction\n");
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.icc);
		print_emv_tlv_list(&emv.terminal);

		// Validate TVR
		if (emv.tvr->length != sizeof(test[i].tvr) ||
			memcmp(emv.tvr->value, test[i].tvr, sizeof(test[i].tvr)) != 0
		) {
			fprintf(stderr, "Incorrect TVR\n");
			print_buf("TVR", emv.tvr->value, emv.tvr->length);
			print_buf("Expected", test[i].tvr, sizeof(test[i].tvr));
			r = 1;
			goto exit;
		}

		// Validate TSI
		if (emv.tsi->length != sizeof(test[i].tsi) ||
			memcmp(emv.tsi->value, test[i].tsi, sizeof(test[i].tsi)) != 0
		) {
			fprintf(stderr, "Incorrect TSI\n");
			print_buf("TSI", emv.tsi->value, emv.tsi->length);
			print_buf("Expected", test[i].tsi, sizeof(test[i].tsi));
			r = 1;
			goto exit;
		}

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
	emv_ctx_clear(&emv);

	return r;
}
