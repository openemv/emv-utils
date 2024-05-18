/**
 * @file emv_cvmlist_test.c
 * @brief Unit tests for Cardholder Verification Method (CVM) List (field 8E) processing
 *
 * Copyright 2021 Leon Lynch
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

#include "emv_fields.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

const uint8_t test1_cvmlist[] = { 0x00, 0x01, 0x86, 0xA0,  0x00, 0x00, 0x03, 0xE8 }; // Missing CV Rule

const uint8_t test2_cvmlist[] = { 0x00, 0x01, 0x86, 0xA0,  0x00, 0x00, 0x03, 0xE8, 0x1E, 0x03, 0x42 }; // Invalid CV Rule length

const uint8_t test3_cvmlist[] = { 0x00, 0x01, 0x86, 0xA0,  0x00, 0x00, 0x03, 0xE8, 0x1E, 0x03 }; // CV Rule: Signature, if supported
const uint32_t test3_amount_x = 100000;
const uint32_t test3_amount_y = 1000;
const struct emv_cv_rule_t test3_cv_rules[] = {
	{ EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_CVM_SUPPORTED },
};

const uint8_t test4_cvmlist[] = { 0x00, 0x01, 0x86, 0xA0,  0x00, 0x00, 0x03, 0xE8, 0x44, 0x03 }; // CV Rule: Enciphered PIN offline, if supported
const uint32_t test4_amount_x = 100000;
const uint32_t test4_amount_y = 1000;
const struct emv_cv_rule_t test4_cv_rules[] = {
	{ EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_OFFLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED },
};

const uint8_t test5_cvmlist[] = { 0x00, 0x01, 0x86, 0xA0,  0x00, 0x00, 0x03, 0xE8, 0x42, 0x01 }; // CV Rule: Enciphered PIN online, if unattended cash
const uint32_t test5_amount_x = 100000;
const uint32_t test5_amount_y = 1000;
const struct emv_cv_rule_t test5_cv_rules[] = {
	{ EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_UNATTENDED_CASH },
};

const uint8_t test6_cvmlist[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x01, 0x44, 0x03, 0x41, 0x03, 0x5E, 0x03, 0x42, 0x03, 0x1F, 0x03 }; // Multiple CV rules
const uint32_t test6_amount_x = 0;
const uint32_t test6_amount_y = 0;
const struct emv_cv_rule_t test6_cv_rules[] = {
	{ EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_UNATTENDED_CASH },
	{ EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_OFFLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED },
	{ EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT, EMV_CV_RULE_COND_CVM_SUPPORTED},
	{ EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_SIGNATURE, EMV_CV_RULE_COND_CVM_SUPPORTED},
	{ EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL | EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED, EMV_CV_RULE_COND_CVM_SUPPORTED },
	{ EMV_CV_RULE_NO_CVM, EMV_CV_RULE_COND_CVM_SUPPORTED },
};

int main(void)
{
	int r;
	struct emv_cvmlist_amounts_t amounts;
	struct emv_cvmlist_itr_t itr;
	struct emv_cv_rule_t rule;

	printf("\nTest 1: Missing CV Rule\n");
	r = emv_cvmlist_itr_init(test1_cvmlist, sizeof(test1_cvmlist), &amounts, &itr);
	if (r == 0) {
		fprintf(stderr, "emv_cvmlist_itr_init() succeeded for invalid CVM List\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTest 2: Invalid CV Rule length\n");
	r = emv_cvmlist_itr_init(test2_cvmlist, sizeof(test2_cvmlist), &amounts, &itr);
	if (r == 0) {
		fprintf(stderr, "emv_cvmlist_itr_init() succeeded for invalid CVM List\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTest 3: Signature CV Rule\n");
	memset(&amounts, 0, sizeof(amounts));
	memset(&rule, 0, sizeof(rule));
	r = emv_cvmlist_itr_init(test3_cvmlist, sizeof(test3_cvmlist), &amounts, &itr);
	if (r) {
		fprintf(stderr, "emv_cvmlist_itr_init() failed; r=%d\n", r);
		return 1;
	}
	if (amounts.X != test3_amount_x) {
		fprintf(stderr, "Incorrect amount X; expected %u; found %u\n", test3_amount_x, amounts.X);
		return 1;
	}
	if (amounts.Y != test3_amount_y) {
		fprintf(stderr, "Incorrect amount Y; expected %u; found %u\n", test3_amount_y, amounts.Y);
		return 1;
	}
	r = emv_cvmlist_itr_next(&itr, &rule);
	if (r != 2) {
		fprintf(stderr, "emv_cvmlist_itr_next() failed; r=%d\n", r);
		return 1;
	}
	if (rule.cvm != test3_cv_rules[0].cvm) {
		fprintf(stderr, "Incorrect CV Rule CVM Code; expected 0x%02X; found 0x%02X\n", test3_cv_rules[0].cvm, rule.cvm);
		return 1;
	}
	if (rule.cvm_cond != test3_cv_rules[0].cvm_cond) {
		fprintf(stderr, "Incorrect CV Rule CVM Condition; expected 0x%02X; found 0x%02X\n", test3_cv_rules[0].cvm_cond, rule.cvm_cond);
		return 1;
	}
	r = emv_cvmlist_itr_next(&itr, &rule);
	if (r != 0) {
		fprintf(stderr, "emv_cvmlist_itr_next() succeeded for empty iterator\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTest 4: Enciphered PIN offline CV Rule\n");
	memset(&amounts, 0, sizeof(amounts));
	memset(&rule, 0, sizeof(rule));
	r = emv_cvmlist_itr_init(test4_cvmlist, sizeof(test4_cvmlist), &amounts, &itr);
	if (r) {
		fprintf(stderr, "emv_cvmlist_itr_init() failed; r=%d\n", r);
		return 1;
	}
	if (amounts.X != test4_amount_x) {
		fprintf(stderr, "Incorrect amount X; expected %u; found %u\n", test4_amount_x, amounts.X);
		return 1;
	}
	if (amounts.Y != test4_amount_y) {
		fprintf(stderr, "Incorrect amount X; expected %u; found %u\n", test4_amount_y, amounts.Y);
		return 1;
	}
	r = emv_cvmlist_itr_next(&itr, &rule);
	if (r != 2) {
		fprintf(stderr, "emv_cvmlist_itr_next() failed; r=%d\n", r);
		return 1;
	}
	if (rule.cvm != test4_cv_rules[0].cvm) {
		fprintf(stderr, "Incorrect CV Rule CVM Code; expected 0x%02X; found 0x%02X\n", test4_cv_rules[0].cvm, rule.cvm);
		return 1;
	}
	if (rule.cvm_cond != test4_cv_rules[0].cvm_cond) {
		fprintf(stderr, "Incorrect CV Rule CVM Condition; expected 0x%02X; found 0x%02X\n", test4_cv_rules[0].cvm_cond, rule.cvm_cond);
		return 1;
	}
	r = emv_cvmlist_itr_next(&itr, &rule);
	if (r != 0) {
		fprintf(stderr, "emv_cvmlist_itr_next() succeeded for empty iterator\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTest 5: Enciphered PIN online CV Rule\n");
	memset(&amounts, 0, sizeof(amounts));
	memset(&rule, 0, sizeof(rule));
	r = emv_cvmlist_itr_init(test5_cvmlist, sizeof(test5_cvmlist), &amounts, &itr);
	if (r) {
		fprintf(stderr, "emv_cvmlist_itr_init() failed; r=%d\n", r);
		return 1;
	}
	if (amounts.X != test5_amount_x) {
		fprintf(stderr, "Incorrect amount X; expected %u; found %u\n", test5_amount_x, amounts.X);
		return 1;
	}
	if (amounts.Y != test5_amount_y) {
		fprintf(stderr, "Incorrect amount X; expected %u; found %u\n", test5_amount_y, amounts.Y);
		return 1;
	}
	r = emv_cvmlist_itr_next(&itr, &rule);
	if (r != 2) {
		fprintf(stderr, "emv_cvmlist_itr_next() failed; r=%d\n", r);
		return 1;
	}
	if (rule.cvm != test5_cv_rules[0].cvm) {
		fprintf(stderr, "Incorrect CV Rule CVM Code; expected 0x%02X; found 0x%02X\n", test5_cv_rules[0].cvm, rule.cvm);
		return 1;
	}
	if (rule.cvm_cond != test5_cv_rules[0].cvm_cond) {
		fprintf(stderr, "Incorrect CV Rule CVM Condition; expected 0x%02X; found 0x%02X\n", test5_cv_rules[0].cvm_cond, rule.cvm_cond);
		return 1;
	}
	r = emv_cvmlist_itr_next(&itr, &rule);
	if (r != 0) {
		fprintf(stderr, "emv_cvmlist_itr_next() succeeded for empty iterator\n");
		return 1;
	}
	printf("Success\n");

	printf("\nTest 6: Multiple CV Rules\n");
	memset(&amounts, 42, sizeof(amounts));
	memset(&rule, 0, sizeof(rule));
	r = emv_cvmlist_itr_init(test6_cvmlist, sizeof(test6_cvmlist), &amounts, &itr);
	if (r) {
		fprintf(stderr, "emv_cvmlist_itr_init() failed; r=%d\n", r);
		return 1;
	}
	if (amounts.X != test6_amount_x) {
		fprintf(stderr, "Incorrect amount X; expected %u; found %u\n", test6_amount_x, amounts.X);
		return 1;
	}
	if (amounts.Y != test6_amount_y) {
		fprintf(stderr, "Incorrect amount X; expected %u; found %u\n", test6_amount_y, amounts.Y);
		return 1;
	}
	for (size_t i = 0; i < sizeof(test6_cv_rules) / sizeof(test6_cv_rules[0]); ++i) {
		r = emv_cvmlist_itr_next(&itr, &rule);
		if (r != 2) {
			fprintf(stderr, "emv_cvmlist_itr_next() failed; r=%d\n", r);
			return 1;
		}
		if (rule.cvm != test6_cv_rules[i].cvm) {
			fprintf(stderr, "Incorrect CV Rule CVM Code; expected 0x%02X; found 0x%02X\n", test6_cv_rules[i].cvm, rule.cvm);
			return 1;
		}
		if (rule.cvm_cond != test6_cv_rules[i].cvm_cond) {
			fprintf(stderr, "Incorrect CV Rule CVM Condition; expected 0x%02X; found 0x%02X\n", test6_cv_rules[i].cvm_cond, rule.cvm_cond);
			return 1;
		}
	}
	r = emv_cvmlist_itr_next(&itr, &rule);
	if (r != 0) {
		fprintf(stderr, "emv_cvmlist_itr_next() succeeded for empty iterator\n");
		return 1;
	}
	printf("Success\n");

	return 0;
}
