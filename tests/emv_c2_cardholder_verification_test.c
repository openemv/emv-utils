/**
 * @file emv_c2_cardholder_verification_test.c
 * @brief Unit tests for EMV contactless kernel C-2 Cardholder Verification
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
#include "emv_c2.h"
#include "emv_tlv.h"
#include "emv_tags.h"
#include "emv_fields.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

// For debug output
#include "emv_debug.h"
#include "print_helpers.h"

struct test_t {
	const char* name;

	struct emv_tlv_t* config_data;
	struct emv_tlv_t* icc_data;

	uint32_t txn_amount;

	int result;
	enum emv_cvm_state_t cvm_state;
	uint8_t cvm_caps;
	uint8_t cvmres[3];
	uint8_t tvr[5];
	enum emv_cvm_t cvm;
};

static const struct test_t test[] = {
	{
		.name = "CDCVM above Reader CVM Required Limit",

		.config_data = (struct emv_tlv_t[]){
			// CVM Capability - CVM Required: Online PIN, Signature
			{ {{ EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED, 1, (uint8_t[]){
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE
			}, 0 }}, NULL },
			// CVM Capability - No CVM Required: No CVM
			{ {{ EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_NO_CVM }, 0 }}, NULL },
			// Kernel Configuration: CDCVM supported
			{ {{ EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION, 1, (uint8_t[]){ EMV_C2_KERNEL_CONFIG_CDCVM_SUPPORTED }, 0 }}, NULL },
			// Reader CVM Required Limit: 50.00
			{ {{ EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x50, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// AIP: Cardholder verification and CDCVM supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED | EMV_AIP_ODCV_SUPPORTED, EMV_AIP_EMV_MODE_SUPPORTED }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), if terminal supports the CVM; No CVM required, if terminal supports the CVM
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_CVM_SUPPORTED
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_amount = 10000, // 100.00

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvm_caps = EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE,
		.cvmres = { EMV_C2_CVM_CDCVM_PERFORMED, 0x00, EMV_CVM_RESULT_SUCCESSFUL },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.cvm = EMV_CVM_CDCVM,
	},

	{
		.name = "CDCVM at Reader CVM Required Limit",

		.config_data = (struct emv_tlv_t[]){
			// CVM Capability - CVM Required: Online PIN, Signature
			{ {{ EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED, 1, (uint8_t[]){
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE
			}, 0 }}, NULL },
			// CVM Capability - No CVM Required: No CVM
			{ {{ EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_NO_CVM }, 0 }}, NULL },
			// Kernel Configuration: CDCVM supported
			{ {{ EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION, 1, (uint8_t[]){ EMV_C2_KERNEL_CONFIG_CDCVM_SUPPORTED }, 0 }}, NULL },
			// Reader CVM Required Limit: 50.00
			{ {{ EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x50, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// AIP: Cardholder verification and CDCVM supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED | EMV_AIP_ODCV_SUPPORTED, EMV_AIP_EMV_MODE_SUPPORTED }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), if terminal supports the CVM; No CVM required, if terminal supports the CVM
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_CVM_SUPPORTED
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_amount = 5000, // 50.00

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvm_caps = EMV_TERM_CAPS_CVM_NO_CVM,
		.cvmres = { EMV_CVM_NOT_PERFORMED, 0x00, EMV_CVM_RESULT_SUCCESSFUL },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.cvm = EMV_CVM_NO_CVM_PERFORMED,
	},

	{
		.name = "Cardholder verification not supported",

		.config_data = (struct emv_tlv_t[]){
			// CVM Capability - CVM Required: Online PIN, Signature
			{ {{ EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED, 1, (uint8_t[]){
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE
			}, 0 }}, NULL },
			// CVM Capability - No CVM Required: No CVM
			{ {{ EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_NO_CVM }, 0 }}, NULL },
			// Kernel Configuration: CDCVM supported
			{ {{ EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION, 1, (uint8_t[]){ EMV_C2_KERNEL_CONFIG_CDCVM_SUPPORTED }, 0 }}, NULL },
			// Reader CVM Required Limit: 50.00
			{ {{ EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x50, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// AIP: Neither cardholder verification nor CDCVM supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CDA_SUPPORTED, EMV_AIP_EMV_MODE_SUPPORTED }, 0 }}, NULL },
			// CVM List: No CVM required, always
			{ {{ EMV_TAG_8E_CVM_LIST, 10, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_amount = 10000, // 100.00

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvm_caps = EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE,
		.cvmres = { EMV_CVM_NOT_PERFORMED, 0x00, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.cvm = EMV_CVM_NO_CVM_PERFORMED,
	},

	{
		.name = "CVM List not present",

		.config_data = (struct emv_tlv_t[]){
			// CVM Capability - CVM Required: Online PIN, Signature
			{ {{ EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED, 1, (uint8_t[]){
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE
			}, 0 }}, NULL },
			// CVM Capability - No CVM Required: No CVM
			{ {{ EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_NO_CVM }, 0 }}, NULL },
			// Kernel Configuration: CDCVM supported
			{ {{ EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION, 1, (uint8_t[]){ EMV_C2_KERNEL_CONFIG_CDCVM_SUPPORTED }, 0 }}, NULL },
			// Reader CVM Required Limit: 50.00
			{ {{ EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x50, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// AIP: Cardholder verification supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, EMV_AIP_EMV_MODE_SUPPORTED }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_amount = 10000, // 100.00

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvm_caps = EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE,
		.cvmres = { EMV_CVM_NOT_PERFORMED, 0x00, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { EMV_TVR_ICC_DATA_MISSING, 0x00, 0x00, 0x00, 0x00 },
		.cvm = EMV_CVM_NO_CVM_PERFORMED,
	},

	{
		.name = "CVM List with odd length",

		.config_data = (struct emv_tlv_t[]){
			// CVM Capability - CVM Required: Online PIN, Signature
			{ {{ EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED, 1, (uint8_t[]){
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE
			}, 0 }}, NULL },
			// CVM Capability - No CVM Required: No CVM
			{ {{ EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_NO_CVM }, 0 }}, NULL },
			// Kernel Configuration: CDCVM supported
			{ {{ EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION, 1, (uint8_t[]){ EMV_C2_KERNEL_CONFIG_CDCVM_SUPPORTED }, 0 }}, NULL },
			// Reader CVM Required Limit: 50.00
			{ {{ EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x50, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// AIP: Cardholder verification supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, EMV_AIP_EMV_MODE_SUPPORTED }, 0 }}, NULL },
			{ {{ EMV_TAG_8E_CVM_LIST, 11, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS, 0x1E
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_amount = 10000, // 100.00

		.result = EMV_OUTCOME_END_APPLICATION_TRY_ANOTHER_CARD,
		.cvm_state = EMV_CVM_STATE_NONE,
		.cvm_caps = EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE,
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.cvm = EMV_CVM_UNKNOWN,
	},

	{
		.name = "Online PIN without CDCVM support",

		.config_data = (struct emv_tlv_t[]){
			// CVM Capability - CVM Required: Online PIN, Signature
			{ {{ EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED, 1, (uint8_t[]){
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE
			}, 0 }}, NULL },
			// CVM Capability - No CVM Required: No CVM
			{ {{ EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_NO_CVM }, 0 }}, NULL },
			// Kernel Configuration: CDCVM not supported
			{ {{ EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION, 1, (uint8_t[]){ 0x00 }, 0 }}, NULL },
			// Reader CVM Required Limit: 50.00
			{ {{ EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x50, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// AIP: Cardholder verification and CDCVM supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED | EMV_AIP_ODCV_SUPPORTED, EMV_AIP_EMV_MODE_SUPPORTED }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), if terminal supports the CVM; No CVM required, if terminal supports the CVM
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_CVM_SUPPORTED
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_amount = 10000, // 100.00

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvm_caps = EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE,
		.cvmres = { EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, EMV_TVR_ONLINE_CVM_CAPTURED, 0x00, 0x00 },
		.cvm = EMV_CVM_ONLINE_PIN,
	},

	{
		.name = "No CVM at Reader CVM Required Limit",

		.config_data = (struct emv_tlv_t[]){
			// CVM Capability - CVM Required: Online PIN, Signature
			{ {{ EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED, 1, (uint8_t[]){
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE
			}, 0 }}, NULL },
			// CVM Capability - No CVM Required: No CVM
			{ {{ EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_NO_CVM }, 0 }}, NULL },
			// Kernel Configuration: CDCVM supported
			{ {{ EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION, 1, (uint8_t[]){ EMV_C2_KERNEL_CONFIG_CDCVM_SUPPORTED }, 0 }}, NULL },
			// Reader CVM Required Limit: 50.00
			{ {{ EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x50, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// AIP: Cardholder verification supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, EMV_AIP_EMV_MODE_SUPPORTED }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), if terminal supports the CVM; No CVM required, if terminal supports the CVM
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_CVM_SUPPORTED
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_amount = 5000, // 50.00

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvm_caps = EMV_TERM_CAPS_CVM_NO_CVM,
		.cvmres = { EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_CVM_SUPPORTED, EMV_CVM_RESULT_SUCCESSFUL },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.cvm = EMV_CVM_NO_CVM_REQUIRED,
	},

	{
		.name = "Online PIN not supported, then signature",

		.config_data = (struct emv_tlv_t[]){
			// CVM Capability - CVM Required: Signature
			{ {{ EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_SIGNATURE }, 0 }}, NULL },
			// CVM Capability - No CVM Required: No CVM
			{ {{ EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_NO_CVM }, 0 }}, NULL },
			// Kernel Configuration: CDCVM supported
			{ {{ EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION, 1, (uint8_t[]){ EMV_C2_KERNEL_CONFIG_CDCVM_SUPPORTED }, 0 }}, NULL },
			// Reader CVM Required Limit: 50.00
			{ {{ EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x50, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// AIP: Cardholder verification supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, EMV_AIP_EMV_MODE_SUPPORTED }, 0 }}, NULL },
			// CVM List: Online PIN (apply next), always; Signature, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_amount = 10000, // 100.00

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvm_caps = EMV_TERM_CAPS_CVM_SIGNATURE,
		.cvmres = { EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.cvm = EMV_CVM_SIGNATURE,
	},

	{
		.name = "Fail CVM processing, then signature",

		.config_data = (struct emv_tlv_t[]){
			// CVM Capability - CVM Required: Online PIN, Signature
			{ {{ EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED, 1, (uint8_t[]){
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE
			}, 0 }}, NULL },
			// CVM Capability - No CVM Required: No CVM
			{ {{ EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_NO_CVM }, 0 }}, NULL },
			// Kernel Configuration: CDCVM supported
			{ {{ EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION, 1, (uint8_t[]){ EMV_C2_KERNEL_CONFIG_CDCVM_SUPPORTED }, 0 }}, NULL },
			// Reader CVM Required Limit: 50.00
			{ {{ EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x50, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// AIP: Cardholder verification supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, EMV_AIP_EMV_MODE_SUPPORTED }, 0 }}, NULL },
			// CVM List: Fail CVM processing (apply next), always; Signature, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_FAIL, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_amount = 10000, // 100.00

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvm_caps = EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE,
		.cvmres = { EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_UNKNOWN },
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.cvm = EMV_CVM_SIGNATURE,
	},

	{
		.name = "Fail CVM processing as last CV Rule",

		.config_data = (struct emv_tlv_t[]){
			// CVM Capability - CVM Required: Online PIN, Signature
			{ {{ EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED, 1, (uint8_t[]){
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE
			}, 0 }}, NULL },
			// CVM Capability - No CVM Required: No CVM
			{ {{ EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_NO_CVM }, 0 }}, NULL },
			// Kernel Configuration: CDCVM supported
			{ {{ EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION, 1, (uint8_t[]){ EMV_C2_KERNEL_CONFIG_CDCVM_SUPPORTED }, 0 }}, NULL },
			// Reader CVM Required Limit: 50.00
			{ {{ EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x50, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// AIP: Cardholder verification supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, EMV_AIP_EMV_MODE_SUPPORTED }, 0 }}, NULL },
			// CVM List: Fail CVM processing (apply next), always
			{ {{ EMV_TAG_8E_CVM_LIST, 10, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_FAIL, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_amount = 10000, // 100.00

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvm_caps = EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE,
		.cvmres = { EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_FAIL, EMV_CV_RULE_COND_ALWAYS, EMV_CVM_RESULT_FAILED },
		.tvr = { 0x00, 0x00, EMV_TVR_CV_PROCESSING_FAILED, 0x00, 0x00 },
		.cvm = EMV_CVM_FAILED,
	},

	{
		.name = "Offline PIN in CVM Capability",

		.config_data = (struct emv_tlv_t[]){
			// CVM Capability - CVM Required: Plaintext offline PIN, No CVM
			{ {{ EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED, 1, (uint8_t[]){
				EMV_TERM_CAPS_CVM_PLAINTEXT_PIN_OFFLINE | EMV_TERM_CAPS_CVM_NO_CVM
			}, 0 }}, NULL },
			// CVM Capability - No CVM Required: No CVM
			{ {{ EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_NO_CVM }, 0 }}, NULL },
			// Kernel Configuration: CDCVM supported
			{ {{ EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION, 1, (uint8_t[]){ EMV_C2_KERNEL_CONFIG_CDCVM_SUPPORTED }, 0 }}, NULL },
			// Reader CVM Required Limit: 50.00
			{ {{ EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x50, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// AIP: Cardholder verification supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, EMV_AIP_EMV_MODE_SUPPORTED }, 0 }}, NULL },
			// CVM List: Plaintext offline PIN (apply next), always; No CVM required, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_amount = 10000, // 100.00

		.result = EMV_ERROR_INVALID_CONFIG,
		.cvm_state = EMV_CVM_STATE_NONE,
		.cvm_caps = 0x00,
		.tvr = { 0x00, 0x00, 0x00, 0x00, 0x00 },
		.cvm = EMV_CVM_UNKNOWN,
	},

	{
		.name = "No CVM Condition satisfied",

		.config_data = (struct emv_tlv_t[]){
			// CVM Capability - CVM Required: Online PIN, Signature
			{ {{ EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED, 1, (uint8_t[]){
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE
			}, 0 }}, NULL },
			// CVM Capability - No CVM Required: No CVM
			{ {{ EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_NO_CVM }, 0 }}, NULL },
			// Kernel Configuration: CDCVM supported
			{ {{ EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION, 1, (uint8_t[]){ EMV_C2_KERNEL_CONFIG_CDCVM_SUPPORTED }, 0 }}, NULL },
			// Reader CVM Required Limit: 50.00
			{ {{ EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x50, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// AIP: Cardholder verification supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, EMV_AIP_EMV_MODE_SUPPORTED }, 0 }}, NULL },
			// CVM List: Signature (apply next), if terminal supports the CVM; No CVM required, condition 0x80
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_CVM_SUPPORTED,
				EMV_CV_RULE_NO_CVM, 0x80
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_amount = 5000, // 50.00

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvm_caps = EMV_TERM_CAPS_CVM_NO_CVM,
		.cvmres = { EMV_CVM_NOT_PERFORMED, 0x00, EMV_CVM_RESULT_FAILED },
		.tvr = { 0x00, 0x00, EMV_TVR_CV_PROCESSING_FAILED, 0x00, 0x00 },
		.cvm = EMV_CVM_FAILED,
	},

	{
		.name = "Unrecognised CVM without apply next",

		.config_data = (struct emv_tlv_t[]){
			// CVM Capability - CVM Required: Online PIN, Signature
			{ {{ EMV_C2_TAG_DF8118_CVM_CAPABILITY_CVM_REQUIRED, 1, (uint8_t[]){
				EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE
			}, 0 }}, NULL },
			// CVM Capability - No CVM Required: No CVM
			{ {{ EMV_C2_TAG_DF8119_CVM_CAPABILITY_NO_CVM_REQUIRED, 1, (uint8_t[]){ EMV_TERM_CAPS_CVM_NO_CVM }, 0 }}, NULL },
			// Kernel Configuration: CDCVM supported
			{ {{ EMV_C2_TAG_DF811B_KERNEL_CONFIGURATION, 1, (uint8_t[]){ EMV_C2_KERNEL_CONFIG_CDCVM_SUPPORTED }, 0 }}, NULL },
			// Reader CVM Required Limit: 50.00
			{ {{ EMV_C2_TAG_DF8126_READER_CVM_REQUIRED_LIMIT, 6, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x50, 0x00 }, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.icc_data = (struct emv_tlv_t[]){
			// AIP: Cardholder verification supported
			{ {{ EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE, 2, (uint8_t[]){ EMV_AIP_CV_SUPPORTED, EMV_AIP_EMV_MODE_SUPPORTED }, 0 }}, NULL },
			// CVM List: Issuer CVM 0x30, always; Signature, always
			{ {{ EMV_TAG_8E_CVM_LIST, 12, (uint8_t[]){
				0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				0x30, EMV_CV_RULE_COND_ALWAYS,
				EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_ALWAYS
			}, 0 }}, NULL },
			{ {{ 0 }} },
		},

		.txn_amount = 10000, // 100.00

		.result = 0,
		.cvm_state = EMV_CVM_STATE_COMPLETE,
		.cvm_caps = EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE | EMV_TERM_CAPS_CVM_SIGNATURE,
		.cvmres = { EMV_CVM_NOT_PERFORMED, 0x00, EMV_CVM_RESULT_FAILED },
		.tvr = { 0x00, 0x00, EMV_TVR_CV_PROCESSING_FAILED | EMV_TVR_CVM_UNRECOGNISED, 0x00, 0x00 },
		.cvm = EMV_CVM_FAILED,
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

int main(void)
{
	int r;
	struct emv_ctx_t emv;
	uint8_t amount[6];
	const struct emv_tlv_t* term_caps;
	const struct emv_tlv_t* cvmres;
	enum emv_cvm_t cvm;

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
		r = append_tlv_list(test[i].config_data, &emv.config.data);
		if (r) {
			fprintf(stderr, "append_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.config.data);
		r = append_tlv_list(test[i].icc_data, &emv.icc);
		if (r) {
			fprintf(stderr, "append_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.icc);

		// Terminal data created by emv_c2_initiate_kernel_processing()
		r = append_tlv_list(
			(struct emv_tlv_t[]){
				// Kernel Identifier - Terminal: Kernel C-2
				{ {{ EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL, 8, (uint8_t[]){ 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL },
				// Terminal Capabilities without CVM Capability
				{ {{ EMV_TAG_9F33_TERMINAL_CAPABILITIES, 3, (uint8_t[]){
					EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS,
					0x00,
					EMV_TERM_CAPS_SECURITY_CDA
				}, 0 }}, NULL },
				{ {{ EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS, 5, (uint8_t[]){ 0x00, 0x00, 0x00, 0x00, 0x00 }, 0 }}, NULL },
				{ {{ EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION, 2, (uint8_t[]){ 0x00, 0x00 }, 0 }}, NULL },
				{ {{ 0 }} },
			},
			&emv.terminal
		);
		if (r) {
			fprintf(stderr, "append_tlv_list() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		emv.kernel_id = emv_tlv_list_find_const(&emv.terminal, EMV_TAG_96_KERNEL_IDENTIFIER_TERMINAL);
		emv.tvr = emv_tlv_list_find_const(&emv.terminal, EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS);
		emv.tsi = emv_tlv_list_find_const(&emv.terminal, EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION);
		emv.aip = emv_tlv_list_find_const(&emv.icc, EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE);

		// Populate Amount, Authorised (Numeric) and cache transaction amount
		// without calling emv_ctx_prepare_txn()
		if (!emv_uint_to_format_n(test[i].txn_amount, amount, sizeof(amount))) {
			fprintf(stderr, "emv_uint_to_format_n() failed\n");
			r = 1;
			goto exit;
		}
		r = emv_tlv_list_push(&emv.params, EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC, sizeof(amount), amount, 0);
		if (r) {
			fprintf(stderr, "emv_tlv_list_push() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		print_emv_tlv_list(&emv.params);
		emv.txn_amount = test[i].txn_amount;

		// Test cardholder verification twice to confirm that repeating it
		// does not change the outcome
		for (unsigned int j = 0; j < 2; ++j) {
			r = emv_c2_cardholder_verification(&emv);
			if (r != test[i].result) {
				fprintf(stderr, "emv_c2_cardholder_verification() returned %d; expected %d\n", r, test[i].result);
				r = 1;
				goto exit;
			}
		}
		print_emv_tlv_list(&emv.terminal);

		// Validate CVM state
		if (emv.cvm.state != test[i].cvm_state) {
			fprintf(stderr, "Incorrect CVM state %d; expected %d\n", emv.cvm.state, test[i].cvm_state);
			r = 1;
			goto exit;
		}

		// Validate CVM Capability
		term_caps = emv_tlv_list_find_const(&emv.terminal, EMV_TAG_9F33_TERMINAL_CAPABILITIES);
		if (!term_caps || term_caps->length != 3) {
			fprintf(stderr, "Terminal Capabilities not found or invalid\n");
			r = 1;
			goto exit;
		}
		if (term_caps->value[1] != test[i].cvm_caps) {
			fprintf(stderr, "Incorrect CVM Capability 0x%02X; expected 0x%02X\n", term_caps->value[1], test[i].cvm_caps);
			r = 1;
			goto exit;
		}

		// Validate CVM Results
		cvmres = emv_tlv_list_find_const(&emv.terminal, EMV_TAG_9F34_CVM_RESULTS);
		if (test[i].result) {
			if (cvmres) {
				fprintf(stderr, "CVM Results unexpectedly present\n");
				r = 1;
				goto exit;
			}
		} else {
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

		// Kernel C-2 does not use TSI
		if (emv.tsi->value[0] || emv.tsi->value[1]) {
			fprintf(stderr, "TSI unexpectedly updated\n");
			print_buf("TSI", emv.tsi->value, emv.tsi->length);
			r = 1;
			goto exit;
		}

		// Validate CVM
		r = emv_cvm_get(&emv, &cvm);
		if (r) {
			fprintf(stderr, "emv_cvm_get() failed; r=%d\n", r);
			r = 1;
			goto exit;
		}
		if (cvm != test[i].cvm) {
			fprintf(stderr, "Incorrect CVM %d; expected %d\n", cvm, test[i].cvm);
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
