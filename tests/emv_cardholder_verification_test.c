/**
 * @file emv_cardholder_verification_test.c
 * @brief Unit tests for EMV Cardholder Verification
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

#include "emv.h"
#include "emv_tlv.h"
#include "emv_tags.h"
#include "emv_fields.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

struct pin_entry_t {
	enum emv_pin_entry_result_t result; // PIN entry result to indicate
	int r; // Expected return value of emv_pin_entry_result()
};

struct test_t {
	const char* name;

	struct emv_tlv_t* config_data;
	struct emv_tlv_t* params_data;
	struct emv_tlv_t* icc_data;

	// PIN entry results to indicate, in order, whenever PIN entry is required
	const struct pin_entry_t* pin_entry;
	size_t pin_entry_count;

	int result;
	enum emv_cvm_state_t cvm_state;
	uint8_t cvmres[3];
	uint8_t tvr[5];
	uint8_t tsi[2];
};

static const struct test_t test[] = {
	{
		.name = "Cardholder verification not supported",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_SDA_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: No CVM required, always
			{ {{ EMV_TAG_8E_CVM_LIST, 10, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CVM_NOT_PERFORMED, 0x00, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "CVM List not present",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CVM_NOT_PERFORMED, 0x00, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { EMV_TVR_ICC_DATA_MISSING, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "CVM List without CV Rules",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ EMV_TAG_8E_CVM_LIST, 8, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CVM_NOT_PERFORMED, 0x00, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { EMV_TVR_ICC_DATA_MISSING, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "CVM List with odd length",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			{ {{ EMV_TAG_8E_CVM_LIST, 11, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS, 0x1E
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = EMV_OUTCOME_CARD_ERROR,
		.cvm_state = EMV_CVM_STATE_NONE,
		.cvmres = { EMV_CVM_NOT_PERFORMED, 0x00, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "Signature",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Signature, if terminal supports the CVM
			{ {{ EMV_TAG_8E_CVM_LIST, 10, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_CVM_SUPPORTED
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_CVM_SUPPORTED, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "No CVM required",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: No CVM required, always
			{ {{ EMV_TAG_8E_CVM_LIST, 10, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_SUCCESSFUL },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Fail CVM processing",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Fail CVM processing (apply next), always; No CVM required, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_FAIL, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_FAIL, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_FAILED },
		.tvr = { 0x00, 0x00, EMV_TVR_CV_PROCESSING_FAILED, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Unrecognised CVM, then No CVM required",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Facial biometric verified offline (apply next), always; No CVM required, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_OFFLINE_BIOMETRIC_FACIAL, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_SUCCESSFUL },
		.tvr = { 0x00, 0x00, EMV_TVR_CVM_UNRECOGNISED, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Unrecognised CVM without apply next",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Issuer CVM 0x30, always; No CVM required, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x30, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CVM_NOT_PERFORMED, 0x00, EMV_CVM_RESULT_FAILED },
		.tvr = { 0x00, 0x00, EMV_TVR_CV_PROCESSING_FAILED | EMV_TVR_CVM_UNRECOGNISED, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "CVM List exhausted",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Finger biometric verified online (apply next), always
			{ {{ EMV_TAG_8E_CVM_LIST, 10, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_BIOMETRIC_FINGER, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CVM_NOT_PERFORMED, 0x00, EMV_CVM_RESULT_FAILED },
		.tvr = { 0x00, 0x00, EMV_TVR_CV_PROCESSING_FAILED | EMV_TVR_CVM_UNRECOGNISED, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Online PIN not supported, then signature",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), always; Signature, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, EMV_TVR_PIN_PAD_FAILED, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Online PIN",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), if terminal supports the CVM; Signature, if terminal supports the CVM
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_CVM_SUPPORTED
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_PIN_ENTRY,
		.cvmres = { EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "Offline plaintext PIN",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Plaintext offline PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_PLAINTEXT_PIN_OFFLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), if terminal supports the CVM; Plaintext offline PIN (apply next), if terminal supports the CVM; Signature, if terminal supports the CVM
			{ {{ EMV_TAG_8E_CVM_LIST, 14, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT, EMV_CV_RULE_COND_CVM_SUPPORTED,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_CVM_SUPPORTED
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_PIN_ENTRY,
		.cvmres = { EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT, EMV_CV_RULE_COND_CVM_SUPPORTED, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "Offline PIN not supported, then No CVM required",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Enciphered offline PIN (apply next), always; No CVM required, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_OFFLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_SUCCESSFUL },
		.tvr = { 0x00, 0x00, EMV_TVR_PIN_PAD_FAILED, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Offline plaintext PIN and signature not supported, then No CVM required",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Plaintext offline PIN, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_PLAINTEXT_PIN_OFFLINE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Plaintext offline PIN and signature (apply next), always; No CVM required, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT_AND_SIGNATURE, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_SUCCESSFUL },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Unsupported CVM with condition if terminal supports the CVM",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Online PIN, if terminal supports the CVM; No CVM required, if terminal supports the CVM
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_CVM_SUPPORTED
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_CVM_SUPPORTED, EMV_CVM_RESULT_SUCCESSFUL },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "No CVM Condition satisfied",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), if unattended cash; Signature (apply next), if manual cash; No CVM required, if purchase with cashback
			{ {{ EMV_TAG_8E_CVM_LIST, 14, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_UNATTENDED_CASH,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_MANUAL_CASH,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_CASHBACK
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CVM_NOT_PERFORMED, 0x00, EMV_CVM_RESULT_FAILED },
		.tvr = { 0x00, 0x00, EMV_TVR_CV_PROCESSING_FAILED, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Unattended cash",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Financial institution unattended, online only
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_FINANCIAL_INSTITUTION | EMV_TERM_TYPE_ENV_UNATTENDED_ONLINE_ONLY
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_CASH }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Signature (apply next), if manual cash; Online PIN, if unattended cash
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_MANUAL_CASH,
				EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_UNATTENDED_CASH
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_PIN_ENTRY,
		.cvmres = { EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_UNATTENDED_CASH, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
	},

	{
		.name = "Manual cash",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Financial institution attended, online only
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_FINANCIAL_INSTITUTION | EMV_TERM_TYPE_ENV_ATTENDED_ONLINE_ONLY
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_CASH }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), if unattended cash; Signature, if manual cash
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_UNATTENDED_CASH,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_MANUAL_CASH
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_MANUAL_CASH, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Purchase with cashback",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_CASHBACK }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Signature (apply next), if not unattended cash and not manual cash and not purchase with cashback; No CVM required, if purchase with cashback
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_NOT_CASH_OR_CASHBACK,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_CASHBACK
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_CASHBACK, EMV_CVM_RESULT_SUCCESSFUL },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Not cash or cashback",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Signature (apply next), if purchase with cashback; No CVM required, if not unattended cash and not manual cash and not purchase with cashback
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_CASHBACK,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_NOT_CASH_OR_CASHBACK
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_NOT_CASH_OR_CASHBACK, EMV_CVM_RESULT_SUCCESSFUL },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Amount under X",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// Application Currency Code: EUR
			{ {{ EMV_TAG_9F42_APPLICATION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			// CVM List: X is 200.00; Y is 50.00
			// Signature (apply next), if over X; No CVM required, if under X
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x4E, 0x20, 0x00, 0x00, 0x13, 0x88,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_MORE_THAN_X,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_LESS_THAN_X
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_LESS_THAN_X, EMV_CVM_RESULT_SUCCESSFUL },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Amount over Y",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// Application Currency Code: EUR
			{ {{ EMV_TAG_9F42_APPLICATION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			// CVM List: X is 200.00; Y is 50.00
			// No CVM required (apply next), if under Y; Signature, if over Y
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x4E, 0x20, 0x00, 0x00, 0x13, 0x88,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_LESS_THAN_Y,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_MORE_THAN_Y
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_MORE_THAN_Y, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Application currency differs",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// Application Currency Code: USD
			{ {{ EMV_TAG_9F42_APPLICATION_CURRENCY_CODE, 2, (uint8_t[]){ 0x08, 0x40 }, 0 }}, NULL },
			// CVM List: X is 200.00; Y is 50.00
			// No CVM required (apply next), if under X; Signature, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x4E, 0x20, 0x00, 0x00, 0x13, 0x88,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_LESS_THAN_X,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Application currency not present",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: X is 200.00; Y is 50.00
			// No CVM required (apply next), if under X; Signature, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x4E, 0x20, 0x00, 0x00, 0x13, 0x88,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_LESS_THAN_X,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Unknown CVM Condition",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: No CVM required, condition 0x80; Signature, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_NO_CVM, 0x80,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Online PIN captured",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), if terminal supports the CVM; Signature, if terminal supports the CVM
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_CVM_SUPPORTED
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.pin_entry = (const struct pin_entry_t[]){
			{ EMV_PIN_ENTRY_CAPTURED, 0 },
		},
		.pin_entry_count = 1,

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, EMV_TVR_ONLINE_CVM_CAPTURED, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Online PIN bypassed, then signature",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), if terminal supports the CVM; Signature, if terminal supports the CVM
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_CVM_SUPPORTED
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.pin_entry = (const struct pin_entry_t[]){
			{ EMV_PIN_ENTRY_BYPASSED, 0 },
		},
		.pin_entry_count = 1,

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_CVM_SUPPORTED, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, EMV_TVR_PIN_NOT_ENTERED, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Online PIN pad failed without apply next",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Online PIN, always; Signature, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.pin_entry = (const struct pin_entry_t[]){
			{ EMV_PIN_ENTRY_PIN_PAD_FAILED, 0 },
		},
		.pin_entry_count = 1,

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_FAILED },
		.tvr = { 0x00, 0x00, EMV_TVR_CV_PROCESSING_FAILED | EMV_TVR_PIN_PAD_FAILED, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Online PIN bypassed and no subsequent CVM performed",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), always; Facial biometric verified offline (apply next), always; Signature, if manual cash
			{ {{ EMV_TAG_8E_CVM_LIST, 14, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_OFFLINE_BIOMETRIC_FACIAL, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_MANUAL_CASH
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.pin_entry = (const struct pin_entry_t[]){
			{ EMV_PIN_ENTRY_BYPASSED, 0 },
		},
		.pin_entry_count = 1,

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_FAILED },
		.tvr = { 0x00, 0x00, EMV_TVR_CV_PROCESSING_FAILED | EMV_TVR_CVM_UNRECOGNISED | EMV_TVR_PIN_NOT_ENTERED, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Duplicate online PIN CV Rules bypassed, then signature",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), always; Online PIN (apply next), always; Signature, always
			{ {{ EMV_TAG_8E_CVM_LIST, 14, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.pin_entry = (const struct pin_entry_t[]){
			{ EMV_PIN_ENTRY_BYPASSED, 0 },
			{ EMV_PIN_ENTRY_BYPASSED, 0 },
		},
		.pin_entry_count = 2,

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, EMV_TVR_PIN_NOT_ENTERED, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Offline PIN captured, then online PIN captured",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Plaintext offline PIN, Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_PLAINTEXT_PIN_OFFLINE | EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Plaintext offline PIN (apply next), if terminal supports the CVM; Online PIN (apply next), if terminal supports the CVM; Signature, if terminal supports the CVM
			{ {{ EMV_TAG_8E_CVM_LIST, 14, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT, EMV_CV_RULE_COND_CVM_SUPPORTED,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_CVM_SUPPORTED
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.pin_entry = (const struct pin_entry_t[]){
			// Offline PIN verification is not supported and therefore
			// indicates that PIN pad is not present or not working
			{ EMV_PIN_ENTRY_CAPTURED, 0 },
			{ EMV_PIN_ENTRY_CAPTURED, 0 },
		},
		.pin_entry_count = 2,

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvmres = { EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, EMV_TVR_PIN_PAD_FAILED | EMV_TVR_ONLINE_CVM_CAPTURED, 0x00, 0x00 },
		.tsi = { EMV_TSI_CV_PERFORMED, 0x00 },
	},

	{
		.name = "Invalid PIN entry result",

		.config_data = (struct emv_tlv_t[]){
			// Terminal Capabilities: Online PIN, Signature, No CVM
			{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
				EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE | EMV_TERM_CAPS_CVM_NO_CVM,
				EMV_TERM_CAPS_SECURITY_CDA
			}, 0 }}, NULL },
			// Terminal Type: Merchant attended, offline with online capability
			{ {{ EMV_TAG_9F35_TERMINAL_TYPE, 1, (uint8_t[]){
				EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT | EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.params_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_9C_TRANSACTION_TYPE, 1, (uint8_t[]){ EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES }, 0 }}, NULL },
			// Amount, Authorised (Binary): 100.00
			{ {{ EMV_TAG_81_AMOUNT_AUTHORISED_BINARY, 4, (uint8_t[]){ 0x00, 0x00, 0x27, 0x10 }, 0 }}, NULL },
			// Transaction Currency Code: EUR
			{ {{ EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE, 2, (uint8_t[]){ 0x09, 0x78 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, 0x00 }, 0 }}, NULL },
			// CVM List: Online PIN, always
			{ {{ EMV_TAG_8E_CVM_LIST, 10, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.pin_entry = (const struct pin_entry_t[]){
			{ 0, EMV_ERROR_INVALID_PARAMETER },
		},
		.pin_entry_count = 1,

		.result = 0,
		.cvm_state = EMV_CVM_STATE_PIN_ENTRY,
		.cvmres = { EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.tsi = { 0x00, 0x00 },
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
	struct emv_ctx_t emv;
	size_t pin_entry_idx;
	const struct emv_tlv_t* cvmres;

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
		r = emv_ctx_init(&emv, NULL);
		if (r) {
			fprintf(stderr, "emv_ctx_init() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		r = populate_tlv_list(
			test[i].config_data,
			&emv.config.data
		);
		if (r) {
			fprintf(stderr, "populate_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.config.data);
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
		emv.aip = emv_tlv_list_find(&emv.icc, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);

		// Indicating PIN entry before cardholder verification must fail
		r = emv_pin_entry_result(&emv, EMV_PIN_ENTRY_CAPTURED);
		if (r != EMV_ERROR_INVALID_PARAMETER) {
			fprintf(stderr, "emv_pin_entry_result() returned %d before cardholder verification\n", r);
			r = 1;
			goto exit;
		}

		// Test cardholder verification and indicate the next PIN entry
		// result whenever PIN entry is required
		pin_entry_idx = 0;
		while (true) {
			const struct pin_entry_t* pin_entry;

			r = emv_cardholder_verification(&emv);
			if (r != test[i].result) {
				fprintf(stderr, "emv_cardholder_verification() returned %d; expected %d\n", r, test[i].result);
				r = 1;
				goto exit;
			}
			if (emv.cvm.state != EMV_CVM_STATE_PIN_ENTRY ||
				pin_entry_idx >= test[i].pin_entry_count
			) {
				break;
			}

			pin_entry = &test[i].pin_entry[pin_entry_idx++];
			r = emv_pin_entry_result(&emv, pin_entry->result);
			if (r != pin_entry->r) {
				fprintf(stderr, "emv_pin_entry_result() returned %d; expected %d\n", r, pin_entry->r);
				r = 1;
				goto exit;
			}
			if (r) {
				// Failure must not change the CVM state
				if (emv.cvm.state != EMV_CVM_STATE_PIN_ENTRY) {
					fprintf(stderr, "Incorrect CVM state %d after failed PIN entry result\n", emv.cvm.state);
					r = 1;
					goto exit;
				}
			} else {
				// Only emv_cardholder_verification() may complete cardholder
				// verification
				if (emv.cvm.state != EMV_CVM_STATE_CVM_PERFORMED) {
					fprintf(stderr, "Incorrect CVM state %d after PIN entry result\n", emv.cvm.state);
					r = 1;
					goto exit;
				}
				if (emv.tsi->value[0] & EMV_TSI_CV_PERFORMED) {
					fprintf(stderr, "TSI indicates cardholder verification performed after PIN entry result\n");
					r = 1;
					goto exit;
				}
			}
		}
		if (pin_entry_idx != test[i].pin_entry_count) {
			fprintf(stderr, "Only %zu of %zu PIN entry results used\n", pin_entry_idx, test[i].pin_entry_count);
			r = 1;
			goto exit;
		}

		// Repeat cardholder verification to confirm that it does not change
		// the outcome
		r = emv_cardholder_verification(&emv);
		if (r != test[i].result) {
			fprintf(stderr, "emv_cardholder_verification() returned %d; expected %d\n", r, test[i].result);
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.terminal);

		// Indicating PIN entry when not required must fail
		if (emv.cvm.state != EMV_CVM_STATE_PIN_ENTRY) {
			r = emv_pin_entry_result(&emv, EMV_PIN_ENTRY_CAPTURED);
			if (r != EMV_ERROR_INVALID_PARAMETER) {
				fprintf(stderr, "emv_pin_entry_result() returned %d when PIN entry not required\n", r);
				r = 1;
				goto exit;
			}
		}

		// Validate CVM state
		if (emv.cvm.state != test[i].cvm_state) {
			fprintf(stderr, "Incorrect CVM state %d; expected %d\n", emv.cvm.state, test[i].cvm_state);
			r = 1;
			goto exit;
		}

		// Validate CVM Results
		cvmres = emv_tlv_list_find_const(&emv.terminal, EMV_TAG_9F34_CVM_RESULTS);
		if (!cvmres) {
			fprintf(stderr, "CVM Results not found\n");
			r = 1;
			goto exit;
		}
		if (cvmres->length != sizeof(test[i].cvmres) ||
			memcmp(cvmres->value, test[i].cvmres, sizeof(test[i].cvmres)) != 0
		) {
			fprintf(stderr, "Incorrect CVM Results\n");
			print_buf("CVM Results", cvmres->value, cvmres->length);
			print_buf("Expected", test[i].cvmres, sizeof(test[i].cvmres));
			r = 1;
			goto exit;
		}

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
