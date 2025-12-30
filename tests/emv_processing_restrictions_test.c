/**
 * @file emv_processing_restrictions_test.c
 * @brief Unit tests for EMV Processing Restrictions
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

	uint8_t tvr[5];
};

static const struct test_t test[] = {
	{
		.name = "No restrictions",

		.config_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x13, 0x13 }, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			// Additional Terminal Capabilities:
			// - Transaction Type Capability: Goods, Services, Cashback, Cash
			{ {{ EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, 5, (uint8_t[]){
				EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH | EMV_ADDL_TERM_CAPS_TXN_TYPE_GOODS | EMV_ADDL_TERM_CAPS_TXN_TYPE_SERVICES | EMV_ADDL_TERM_CAPS_TXN_TYPE_CASHBACK,
				0x00, 0x00, 0x00, 0x00
			}, 0 }}, NULL },
			// Terminal Country Code: Netherlands
			{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Transaction Type: Goods and Services
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F08_APPLICATION_VERSION_NUMBER, 2, (uint8_t[]){ 0x13, 0x13 }, 0 }}, NULL },
			// Application Usage Control: All usages
			{ {{ EMV_TAG_9F07_APPLICATION_USAGE_CONTROL, 2, (uint8_t[]){
				EMV_AUC_DOMESTIC_CASH | EMV_AUC_INTERNATIONAL_CASH | EMV_AUC_DOMESTIC_GOODS | EMV_AUC_INTERNATIONAL_GOODS | EMV_AUC_DOMESTIC_SERVICES | EMV_AUC_INTERNATIONAL_SERVICES |
				EMV_AUC_ATM | EMV_AUC_NON_ATM,
				EMV_AUC_DOMESTIC_CASHBACK | EMV_AUC_INTERNATIONAL_CASHBACK
			}, 0 }}, NULL },
			// Issuer Country Code: Netherlands
			{ {{ EMV_TAG_5F28_ISSUER_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
			{ {{ EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ EMV_TAG_5F25_APPLICATION_EFFECTIVE_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
	},

	{
		.name = "Application version differs",

		.config_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x13, 0x13 }, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			// Additional Terminal Capabilities:
			// - Transaction Type Capability: Goods, Services, Cashback, Cash
			{ {{ EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, 5, (uint8_t[]){
				EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH | EMV_ADDL_TERM_CAPS_TXN_TYPE_GOODS | EMV_ADDL_TERM_CAPS_TXN_TYPE_SERVICES | EMV_ADDL_TERM_CAPS_TXN_TYPE_CASHBACK,
				0x00, 0x00, 0x00, 0x00
			}, 0 }}, NULL },
			// Terminal Country Code: Netherlands
			{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Transaction Type: Goods and Services
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F08_APPLICATION_VERSION_NUMBER, 2, (uint8_t[]){ 0x13, 0x10 }, 0 }}, NULL },
			{ {{ EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.tvr = { 0x00, EMV_TVR_APPLICATION_VERSIONS_DIFFERENT, 0x00, 0x00, 0x00 },
	},

	{
		.name = "ATM not allowed",

		.config_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x13, 0x13 }, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_FINANCIAL_INSTITUTION | EMV_TERM_TYPE_ENV_UNATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			// Additional Terminal Capabilities:
			// - Transaction Type Capability: Goods, Services, Cashback, Cash
			{ {{ EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, 5, (uint8_t[]){
				EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH | EMV_ADDL_TERM_CAPS_TXN_TYPE_GOODS | EMV_ADDL_TERM_CAPS_TXN_TYPE_SERVICES | EMV_ADDL_TERM_CAPS_TXN_TYPE_CASHBACK,
				0x00, 0x00, 0x00, 0x00
			}, 0 }}, NULL },
			// Terminal Country Code: Netherlands
			{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Transaction Type: Goods and Services
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// Application Usage Control: All usages
			{ {{ EMV_TAG_9F07_APPLICATION_USAGE_CONTROL, 2, (uint8_t[]){
				EMV_AUC_DOMESTIC_CASH | EMV_AUC_INTERNATIONAL_CASH | EMV_AUC_DOMESTIC_GOODS | EMV_AUC_INTERNATIONAL_GOODS | EMV_AUC_DOMESTIC_SERVICES | EMV_AUC_INTERNATIONAL_SERVICES |
				EMV_AUC_NON_ATM,
				EMV_AUC_DOMESTIC_CASHBACK | EMV_AUC_INTERNATIONAL_CASHBACK
			}, 0 }}, NULL },
			{ {{ EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.tvr = { 0x00, EMV_TVR_SERVICE_NOT_ALLOWED, 0x00, 0x00, 0x00 },
	},

	{
		.name = "Non-ATM (because no cash) not allowed",

		.config_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x13, 0x13 }, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			// Additional Terminal Capabilities:
			// - Transaction Type Capability: Goods, Services, Cashback, Cash
			{ {{ EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, 5, (uint8_t[]){
				EMV_ADDL_TERM_CAPS_TXN_TYPE_GOODS | EMV_ADDL_TERM_CAPS_TXN_TYPE_SERVICES | EMV_ADDL_TERM_CAPS_TXN_TYPE_CASHBACK,
				0x00, 0x00, 0x00, 0x00
			}, 0 }}, NULL },
			// Terminal Country Code: Netherlands
			{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Transaction Type: Goods and Services
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F08_APPLICATION_VERSION_NUMBER, 2, (uint8_t[]){ 0x13, 0x13 }, 0 }}, NULL },
			// Application Usage Control: All usages
			{ {{ EMV_TAG_9F07_APPLICATION_USAGE_CONTROL, 2, (uint8_t[]){
				EMV_AUC_DOMESTIC_CASH | EMV_AUC_INTERNATIONAL_CASH | EMV_AUC_DOMESTIC_GOODS | EMV_AUC_INTERNATIONAL_GOODS | EMV_AUC_DOMESTIC_SERVICES | EMV_AUC_INTERNATIONAL_SERVICES |
				EMV_AUC_ATM,
				EMV_AUC_DOMESTIC_CASHBACK | EMV_AUC_INTERNATIONAL_CASHBACK
			}, 0 }}, NULL },
			// Issuer Country Code: Netherlands
			{ {{ EMV_TAG_5F28_ISSUER_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
			{ {{ EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ EMV_TAG_5F25_APPLICATION_EFFECTIVE_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.tvr = { 0x00, EMV_TVR_SERVICE_NOT_ALLOWED, 0x00, 0x00, 0x00 },
	},

	{
		.name = "Domestic goods&services not allowed",

		.config_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x13, 0x13 }, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			// Additional Terminal Capabilities:
			// - Transaction Type Capability: Goods, Services, Cashback, Cash
			{ {{ EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, 5, (uint8_t[]){
				EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH | EMV_ADDL_TERM_CAPS_TXN_TYPE_GOODS | EMV_ADDL_TERM_CAPS_TXN_TYPE_SERVICES | EMV_ADDL_TERM_CAPS_TXN_TYPE_CASHBACK,
				0x00, 0x00, 0x00, 0x00
			}, 0 }}, NULL },
			// Terminal Country Code: Netherlands
			{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Transaction Type: Goods and Services
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F08_APPLICATION_VERSION_NUMBER, 2, (uint8_t[]){ 0x13, 0x13 }, 0 }}, NULL },
			// Application Usage Control: All usages
			{ {{ EMV_TAG_9F07_APPLICATION_USAGE_CONTROL, 2, (uint8_t[]){
				EMV_AUC_DOMESTIC_CASH | EMV_AUC_INTERNATIONAL_CASH | 	EMV_AUC_INTERNATIONAL_GOODS | EMV_AUC_INTERNATIONAL_SERVICES |
				EMV_AUC_ATM | EMV_AUC_NON_ATM,
				EMV_AUC_DOMESTIC_CASHBACK | EMV_AUC_INTERNATIONAL_CASHBACK
			}, 0 }}, NULL },
			// Issuer Country Code: Netherlands
			{ {{ EMV_TAG_5F28_ISSUER_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
			{ {{ EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ EMV_TAG_5F25_APPLICATION_EFFECTIVE_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.tvr = { 0x00, EMV_TVR_SERVICE_NOT_ALLOWED, 0x00, 0x00, 0x00 },
	},

	{
		.name = "International cashback not allowed",

		.config_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x13, 0x13 }, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			// Additional Terminal Capabilities:
			// - Transaction Type Capability: Goods, Services, Cashback, Cash
			{ {{ EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, 5, (uint8_t[]){
				EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH | EMV_ADDL_TERM_CAPS_TXN_TYPE_GOODS | EMV_ADDL_TERM_CAPS_TXN_TYPE_SERVICES | EMV_ADDL_TERM_CAPS_TXN_TYPE_CASHBACK,
				0x00, 0x00, 0x00, 0x00
			}, 0 }}, NULL },
			// Terminal Country Code: Netherlands
			{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Transaction Type: Goods and Services
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_CASHBACK }, 0 }}, NULL },
			{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F08_APPLICATION_VERSION_NUMBER, 2, (uint8_t[]){ 0x13, 0x13 }, 0 }}, NULL },
			// Application Usage Control: All usages
			{ {{ EMV_TAG_9F07_APPLICATION_USAGE_CONTROL, 2, (uint8_t[]){
				EMV_AUC_DOMESTIC_CASH | EMV_AUC_INTERNATIONAL_CASH | EMV_AUC_DOMESTIC_GOODS | EMV_AUC_INTERNATIONAL_GOODS | EMV_AUC_DOMESTIC_SERVICES | EMV_AUC_INTERNATIONAL_SERVICES |
				EMV_AUC_ATM | EMV_AUC_NON_ATM,
				EMV_AUC_DOMESTIC_CASHBACK
			}, 0 }}, NULL },
			// Issuer Country Code: United States
			{ {{ EMV_TAG_5F28_ISSUER_COUNTRY_CODE, 2, (uint8_t[]){ 0x08, 0x40 }, 0 }}, NULL },
			{ {{ EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ EMV_TAG_5F25_APPLICATION_EFFECTIVE_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.tvr = { 0x00, EMV_TVR_SERVICE_NOT_ALLOWED, 0x00, 0x00, 0x00 },
	},

	{
		.name = "Application not yet effective",

		.config_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x13, 0x13 }, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			// Additional Terminal Capabilities:
			// - Transaction Type Capability: Goods, Services, Cashback, Cash
			{ {{ EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, 5, (uint8_t[]){
				EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH | EMV_ADDL_TERM_CAPS_TXN_TYPE_GOODS | EMV_ADDL_TERM_CAPS_TXN_TYPE_SERVICES | EMV_ADDL_TERM_CAPS_TXN_TYPE_CASHBACK,
				0x00, 0x00, 0x00, 0x00
			}, 0 }}, NULL },
			// Terminal Country Code: Netherlands
			{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Transaction Type: Goods and Services
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x10 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ EMV_TAG_5F25_APPLICATION_EFFECTIVE_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.tvr = { 0x00, EMV_TVR_APPLICATION_NOT_EFFECTIVE, 0x00, 0x00, 0x00 },
	},

	{
		.name = "Application is expired",

		.config_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x13, 0x13 }, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			// Additional Terminal Capabilities:
			// - Transaction Type Capability: Goods, Services, Cashback, Cash
			{ {{ EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, 5, (uint8_t[]){
				EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH | EMV_ADDL_TERM_CAPS_TXN_TYPE_GOODS | EMV_ADDL_TERM_CAPS_TXN_TYPE_SERVICES | EMV_ADDL_TERM_CAPS_TXN_TYPE_CASHBACK,
				0x00, 0x00, 0x00, 0x00
			}, 0 }}, NULL },
			// Terminal Country Code: Netherlands
			{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Transaction Type: Goods and Services
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x12 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ EMV_TAG_5F25_APPLICATION_EFFECTIVE_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.tvr = { 0x00, EMV_TVR_APPLICATION_EXPIRED, 0x00, 0x00, 0x00 },
	},

	{
		.name = "All restrictions",

		.config_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL, 2, (uint8_t[]){ 0x13, 0x13 }, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			// Additional Terminal Capabilities:
			// - Transaction Type Capability: Goods, Services, Cashback, Cash
			{ {{ EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES, 5, (uint8_t[]){
				EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH | EMV_ADDL_TERM_CAPS_TXN_TYPE_GOODS | EMV_ADDL_TERM_CAPS_TXN_TYPE_SERVICES | EMV_ADDL_TERM_CAPS_TXN_TYPE_CASHBACK,
				0x00, 0x00, 0x00, 0x00
			}, 0 }}, NULL },
			// Terminal Country Code: Netherlands
			{ {{ EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			// Transaction Type: Goods and Services
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_CASH }, 0 }}, NULL },
			{ {{ EMV_TAG_9A_TRANSACTION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x11 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9F08_APPLICATION_VERSION_NUMBER, 2, (uint8_t[]){ 0x13, 0x14 }, 0 }}, NULL },
			// Application Usage Control: All usages
			{ {{ EMV_TAG_9F07_APPLICATION_USAGE_CONTROL, 2, (uint8_t[]){
				EMV_AUC_INTERNATIONAL_CASH | EMV_AUC_DOMESTIC_GOODS | EMV_AUC_INTERNATIONAL_GOODS | EMV_AUC_DOMESTIC_SERVICES | EMV_AUC_INTERNATIONAL_SERVICES |
				EMV_AUC_ATM | EMV_AUC_NON_ATM,
				EMV_AUC_DOMESTIC_CASHBACK | EMV_AUC_INTERNATIONAL_CASHBACK
			}, 0 }}, NULL },
			// Issuer Country Code: Netherlands
			{ {{ EMV_TAG_5F28_ISSUER_COUNTRY_CODE, 2, (uint8_t[]){ 0x05, 0x28 }, 0 }}, NULL },
			{ {{ EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x10 }, 0 }}, NULL },
			{ {{ EMV_TAG_5F25_APPLICATION_EFFECTIVE_DATE, 3, (uint8_t[]){ 0x25, 0x05, 0x12 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.tvr = { 0x00, EMV_TVR_APPLICATION_VERSIONS_DIFFERENT | EMV_TVR_APPLICATION_EXPIRED | EMV_TVR_APPLICATION_NOT_EFFECTIVE | EMV_TVR_SERVICE_NOT_ALLOWED, 0x00, 0x00, 0x00 },
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
	struct emv_ttl_t ttl = { { 0, NULL, NULL } };
	struct emv_ctx_t emv;

	r = emv_debug_init(
		EMV_DEBUG_SOURCE_ALL,
		EMV_DEBUG_LEVEL_CARD,
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
		emv.tvr = emv_tlv_list_find(&emv.terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS);

		// Test processing restrictions...
		r = emv_processing_restrictions(&emv);
		if (r) {
			fprintf(stderr, "emv_processing_restrictions() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
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
