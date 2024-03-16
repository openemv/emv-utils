/**
 * @file emv_fields.h
 * @brief EMV field definitions and helper functions
 *
 * Copyright (c) 2021-2024 Leon Lynch
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

#ifndef EMV_FIELDS_H
#define EMV_FIELDS_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

// Payment System Environment strings
#define EMV_PSE                                                 "1PAY.SYS.DDF01" ///< Payment System Environment (PSE)
#define EMV_PPSE                                                "2PAY.SYS.DDF01" ///< Proximity Payment System Environment (PPSE)

// Application Priority Indicator
// See EMV 4.4 Book 1, 12.2.3, table 13
#define EMV_APP_PRIORITY_INDICATOR_MASK                         (0x0F)
#define EMV_APP_PRIORITY_INDICATOR_CONF_REQUIRED                (0x80)

// Application Selection Indicator
// See EMV 4.4 Book 1, 12.3.1
#define EMV_ASI_EXACT_MATCH                                     (0x00)
#define EMV_ASI_PARTIAL_MATCH                                   (0x01)

// Transaction Type (field 9C)
// See ISO 8583:1987, 4.3.8
// See ISO 8583:1993, A.9
// See ISO 8583:2021, D.21
#define EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES                 (0x00) ///< Transaction Type: Goods and services
#define EMV_TRANSACTION_TYPE_CASH                               (0x01) ///< Transaction Type: Cash
#define EMV_TRANSACTION_TYPE_CASHBACK                           (0x09) ///< Transaction Type: Cashback
#define EMV_TRANSACTION_TYPE_REFUND                             (0x20) ///< Transaction Type: Refund
#define EMV_TRANSACTION_TYPE_INQUIRY                            (0x30) ///< Transaction Type: Inquiry

// Terminal Type (field 9F35)
// See EMV 4.4 Book 4, Annex A1, table 24
#define EMV_TERM_TYPE_OPERATIONAL_CONTROL_MASK                  (0x30) ///< Terminal Type mask for operational control
#define EMV_TERM_TYPE_OPERATIONAL_CONTROL_FINANCIAL_INSTITUTION (0x10) ///< Operational Control: Financial Institution
#define EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT              (0x20) ///< Operationsl Control: Merchant
#define EMV_TERM_TYPE_OPERATIONAL_CONTROL_CARDHOLDER            (0x30) ///< Operationsl Control: Cardholder
#define EMV_TERM_TYPE_ENV_MASK                                  (0x07) ///< Terminal Type mask for terminal environment
#define EMV_TERM_TYPE_ENV_ATTENDED_ONLINE_ONLY                  (0x01) ///< Environment: Attended, online only
#define EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE          (0x02) ///< Environment: Attended, offline with online capability
#define EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_ONLY                 (0x03) ///< Environment: Attended, offline only
#define EMV_TERM_TYPE_ENV_UNATTENDED_ONLINE_ONLY                (0x04) ///< Environment: Unattended, online only
#define EMV_TERM_TYPE_ENV_UNATTENDED_OFFLINE_WITH_ONLINE        (0x05) ///< Environment: Unattended, offline with online capability
#define EMV_TERM_TYPE_ENV_UNATTENDED_OFFLINE_ONLY               (0x06) ///< Environment: Unattended, offline only

// Terminal Capabilities (field 9F33) byte 1
// See EMV 4.4 Book 4, Annex A2, table 25
#define EMV_TERM_CAPS_INPUT_MANUAL_KEY_ENTRY                    (0x80) ///< Card Data Input Capability: Manual key entry
#define EMV_TERM_CAPS_INPUT_MAGNETIC_STRIPE                     (0x40) ///< Card Data Input Capability: Magnetic stripe
#define EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS                    (0x20) ///< Card Data Input Capability: IC with contacts
#define EMV_TERM_CAPS_INPUT_RFU                                 (0x1F) ///< Card Data Input Capability: RFU

// Terminal Capabilities (field 9F33) byte 2
// See EMV 4.4 Book 4, Annex A2, table 26
#define EMV_TERM_CAPS_CVM_PLAINTEXT_PIN_OFFLINE                 (0x80) ///< CVM Capability: Plaintext PIN for ICC verification
#define EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE                 (0x40) ///< CVM Capability: Enciphered PIN for online verification
#define EMV_TERM_CAPS_CVM_SIGNATURE                             (0x20) ///< CVM Capability: Signature
#define EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_OFFLINE_RSA            (0x10) ///< CVM Capability: Enciphered PIN for offline verification (RSA ODE)
#define EMV_TERM_CAPS_CVM_NO_CVM                                (0x08) ///< CVM Capability: No CVM required
#define EMV_TERM_CAPS_CVM_BIOMETRIC_ONLINE                      (0x04) ///< CVM Capability: Online Biometric
#define EMV_TERM_CAPS_CVM_BIOMETRIC_OFFLINE                     (0x02) ///< CVM Capability: Offline Biometric
#define EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_OFFLINE_ECC            (0x01) ///< CVM Capability: Enciphered PIN for offline verification (ECC ODE)

// Terminal Capabilities (field 9F33) byte 3
// See EMV 4.4 Book 4, Annex A2, table 27
#define EMV_TERM_CAPS_SECURITY_SDA                              (0x80) ///< Security Capability: Static Data Authentication (SDA)
#define EMV_TERM_CAPS_SECURITY_DDA                              (0x40) ///< Security Capability: Dynamic Data Authentication (DDA)
#define EMV_TERM_CAPS_SECURITY_CARD_CAPTURE                     (0x20) ///< Security Capability: Card capture
#define EMV_TERM_CAPS_SECURITY_CDA                              (0x08) ///< Security Capability: Combined DDA/Application Cryptogram Generation (CDA)
#define EMV_TERM_CAPS_SECURITY_XDA                              (0x04) ///< Security Capability: Extended Data Authentication (XDA)
#define EMV_TERM_CAPS_SECURITY_RFU                              (0x13) ///< Security Capability: RFU

// Point-of-Service (POS) Entry Mode (field 9F39)
// See ISO 8583:1987, 4.3.14
// See ISO 8583:1993, A.8
// See ISO 8583:2021 J.2.2.1
#define EMV_POS_ENTRY_MODE_UNKNOWN                              (0x00) ///< POS Entry Mode: Unknown
#define EMV_POS_ENTRY_MODE_MANUAL                               (0x01) ///< POS Entry Mode: Manual PAN entry
#define EMV_POS_ENTRY_MODE_MAG                                  (0x02) ///< POS Entry Mode: Magnetic stripe
#define EMV_POS_ENTRY_MODE_OPTICAL_CODE                         (0x03) ///< POS Entry Mode: Optical Code
#define EMV_POS_ENTRY_MODE_OCR                                  (0x04) ///< POS Entry Mode: OCR
#define EMV_POS_ENTRY_MODE_ICC_WITH_CVV                         (0x05) ///< POS Entry Mode: Integrated circuit card (ICC). CVV can be checked.
#define EMV_POS_ENTRY_MODE_CONTACTLESS_EMV                      (0x07) ///< POS Entry Mode: Auto entry via contactless EMV
#define EMV_POS_ENTRY_MODE_CARDHOLDER_ON_FILE                   (0x10) ///< POS Entry Mode: Merchant has Cardholder Credentials on File
#define EMV_POS_ENTRY_MODE_MAG_FALLBACK                         (0x80) ///< POS Entry Mode: Fallback from integrated circuit card (ICC) to magnetic stripe
#define EMV_POS_ENTRY_MODE_MAG_WITH_CVV                         (0x90) ///< POS Entry Mode: Magnetic stripe as read from track 2. CVV can be checked.
#define EMV_POS_ENTRY_MODE_CONTACTLESS_MAG                      (0x91) ///< POS Entry Mode: Auto entry via contactless magnetic stripe
#define EMV_POS_ENTRY_MODE_ICC_WITHOUT_CVV                      (0x95) ///< POS Entry Mode: Integrated circuit card (ICC). CVV may not be checked.
#define EMV_POS_ENTRY_MODE_ORIGINAL_TXN                         (0x99) ///< POS Entry Mode: Same as original transaction

// Additional Terminal Capabilities (field 9F40) byte 1
// See EMV 4.4 Book 4, Annex A3, table 28
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH                        (0x80) ///< Transaction Type Capability: Cash
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_GOODS                       (0x40) ///< Transaction Type Capability: Goods
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_SERVICES                    (0x20) ///< Transaction Type Capability: Services
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_CASHBACK                    (0x10) ///< Transaction Type Capability: Cashback
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_INQUIRY                     (0x08) ///< Transaction Type Capability: Inquiry
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_TRANSFER                    (0x04) ///< Transaction Type Capability: Transfer
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_PAYMENT                     (0x02) ///< Transaction Type Capability: Payment
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_ADMINISTRATIVE              (0x01) ///< Transaction Type Capability: Administrative

// Additional Terminal Capabilities (field 9F40) byte 2
// See EMV 4.4 Book 4, Annex A3, table 29
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH_DEPOSIT                (0x80) ///< Transaction Type Capability: Cash deposit
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_RFU                         (0x7F) ///< Transaction Type Capability: RFU

// Additional Terminal Capabilities (field 9F40) byte 3
// See EMV 4.4 Book 4, Annex A3, table 30
#define EMV_ADDL_TERM_CAPS_INPUT_NUMERIC_KEYS                   (0x80) ///< Terminal Data Input Capability: Numeric keys
#define EMV_ADDL_TERM_CAPS_INPUT_ALPHABETIC_AND_SPECIAL_KEYS    (0x40) ///< Terminal Data Input Capability: Alphabetic and special characters keys
#define EMV_ADDL_TERM_CAPS_INPUT_COMMAND_KEYS                   (0x20) ///< Terminal Data Input Capability: Command keys
#define EMV_ADDL_TERM_CAPS_INPUT_FUNCTION_KEYS                  (0x10) ///< Terminal Data Input Capability: Function keys
#define EMV_ADDL_TERM_CAPS_INPUT_RFU                            (0x0F) ///< Terminal Data Input Capability: RFU

// Additional Terminal Capabilities (field 9F40) byte 4
// See EMV 4.4 Book 4, Annex A3, table 31
#define EMV_ADDL_TERM_CAPS_OUTPUT_PRINT_OR_DISPLAY              (0xF0) ///< Terminal Data Output Capability: Print or display
#define EMV_ADDL_TERM_CAPS_OUTPUT_PRINT_ATTENDANT               (0x80) ///< Terminal Data Output Capability: Print or electronic, attendant
#define EMV_ADDL_TERM_CAPS_OUTPUT_PRINT_CARDHOLDER              (0x40) ///< Terminal Data Output Capability: Print or electronic, cardholder
#define EMV_ADDL_TERM_CAPS_OUTPUT_DISPLAY_ATTENDANT             (0x20) ///< Terminal Data Output Capability: Display, attendant
#define EMV_ADDL_TERM_CAPS_OUTPUT_DISPLAY_CARDHOLDER            (0x10) ///< Terminal Data Output Capability: Display cardholder
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_10                 (0x02) ///< Terminal Data Output Capability: Code table 10
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_9                  (0x01) ///< Terminal Data Output Capability: Code table 9
#define EMV_ADDL_TERM_CAPS_OUTPUT_RFU                           (0x0C) ///< Terminal Data Output Capability: RFU

// Additional Terminal Capabilities (field 9F40) byte 5
// See EMV 4.4 Book 4, Annex A3, table 32
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_8                  (0x80) ///< Terminal Data Output Capability: Code table 8
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_7                  (0x40) ///< Terminal Data Output Capability: Code table 7
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_6                  (0x20) ///< Terminal Data Output Capability: Code table 6
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_5                  (0x10) ///< Terminal Data Output Capability: Code table 5
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_4                  (0x08) ///< Terminal Data Output Capability: Code table 4
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_3                  (0x04) ///< Terminal Data Output Capability: Code table 3
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_2                  (0x02) ///< Terminal Data Output Capability: Code table 2
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_1                  (0x01) ///< Terminal Data Output Capability: Code table 1

// Application Selection Registered Proprietary Data (field 9F0A)
// See EMV 4.4 Book 1, 12.5
// See https://www.emvco.com/registered-ids/
#define EMV_ASRPD_ECSG                                          (0x0001) ///< European Cards Stakeholders Group
#define EMV_ASRPD_TCEA                                          (0x0002) ///< Technical Cooperation ep2 Association

// Application Interchange Profile (field 82) byte 1
// See EMV 4.4 Book 3, Annex C1, Table 41
// See EMV Contactless Book C-2 v2.10, Annex A.1.16
#define EMV_AIP_XDA_SUPPORTED                                   (0x80) ///< Application Interchange Profile: Extended Data Authentication (XDA) is supported
#define EMV_AIP_SDA_SUPPORTED                                   (0x40) ///< Application Interchange Profile: Static Data Authentication (SDA) is supported
#define EMV_AIP_DDA_SUPPORTED                                   (0x20) ///< Application Interchange Profile: Dynamic Data Authentication (DDA) is supported
#define EMV_AIP_CV_SUPPORTED                                    (0x10) ///< Application Interchange Profile: Cardholder verification is supported
#define EMV_AIP_TERMINAL_RISK_MANAGEMENT_REQUIRED               (0x08) ///< Application Interchange Profile: Terminal risk management is to be performed
#define EMV_AIP_ISSUER_AUTHENTICATION_SUPPORTED                 (0x04) ///< Application Interchange Profile: Issuer authentication is supported
#define EMV_AIP_ODCV_SUPPORTED                                  (0x02) ///< Application Interchange Profile: On device cardholder verification is supported
#define EMV_AIP_CDA_SUPPORTED                                   (0x01) ///< Application Interchange Profile: Combined DDA/Application Cryptogram Generation (CDA) is supported

// Application Interchange Profile (field 82) byte 2
// See EMV Contactless Book C-2 v2.10, Annex A.1.16
// See EMV Contactless Book C-3 v2.10, Annex A.2 (NOTE: byte 2 bit 8 is documented but no longer used by this specification)
#define EMV_AIP_EMV_MODE_SUPPORTED                              (0x80) ///< Application Interchange Profile: Contactless EMV mode is supported
#define EMV_AIP_MOBILE_PHONE                                    (0x40) ///< Application Interchange Profile: Mobile phone
#define EMV_AIP_CONTACTLESS_TXN                                 (0x20) ///< Application Interchange Profile: Contactless transaction
#define EMV_AIP_RFU                                             (0x1E) ///< Application Interchange Profile: RFU
#define EMV_AIP_RRP_SUPPORTED                                   (0x01) ///< Application Interchange Profile: Relay Resistance Protocol (RRP) is supported

// Application File Locator (field 94) byte 1
// See EMV 4.4 Book 3, 10.2
#define EMV_AFL_SFI_MASK                                        (0xF8) ///< Application File Locator (AFL) mask for Short File Identifier (SFI)
#define EMV_AFL_SFI_SHIFT                                       (3) ///< Application File Locator (AFL) shift for Short File Identifier (SFI)

// Application Usage Control (field 9F07) byte 1
// See EMV 4.4 Book 3, Annex C2, Table 42
#define EMV_AUC_DOMESTIC_CASH                                   (0x80) ///< Application Usage Control: Valid for domestic cash transactions
#define EMV_AUC_INTERNATIONAL_CASH                              (0x40) ///< Application Usage Control: Valid for international cash transactions
#define EMV_AUC_DOMESTIC_GOODS                                  (0x20) ///< Application Usage Control: Valid for domestic goods
#define EMV_AUC_INTERNATIONAL_GOODS                             (0x10) ///< Application Usage Control: Valid for international goods
#define EMV_AUC_DOMESTIC_SERVICES                               (0x08) ///< Application Usage Control: Valid for domestic services
#define EMV_AUC_INTERNATIONAL_SERVICES                          (0x04) ///< Application Usage Control: Valid for international services
#define EMV_AUC_ATM                                             (0x02) ///< Application Usage Control: Valid at ATMs
#define EMV_AUC_NON_ATM                                         (0x01) ///< Application Usage Control: Valid at terminals other than ATMs

// Application Usage Control (field 9F07) byte 2
// See EMV 4.4 Book 3, Annex C2, Table 42
#define EMV_AUC_DOMESTIC_CASHBACK                               (0x80) ///< Application Usage Control: Domestic cashback allowed
#define EMV_AUC_INTERNATIONAL_CASHBACK                          (0x40) ///< Application Usage Control: International cashback allowed
#define EMV_AUC_RFU                                             (0x3F) ///< Application Usage Control: RFU

// Account Type (field 5F57)
// See EMV 4.4 Book 3, Annex G, Table 56
#define EMV_ACCOUNT_TYPE_DEFAULT                                (0x00) ///< Account Type: Default
#define EMV_ACCOUNT_TYPE_SAVINGS                                (0x10) ///< Account Type: Savings
#define EMV_ACCOUNT_TYPE_CHEQUE_OR_DEBIT                        (0x20) ///< Account Type: Cheque/Debit
#define EMV_ACCOUNT_TYPE_CREDIT                                 (0x30) ///< Account Type: Credit

// Cardholder Verification (CV) Rule byte 1, CVM Codes
// See EMV 4.4 Book 3, Annex C3, Table 43
#define EMV_CV_RULE_APPLY_NEXT_IF_UNSUCCESSFUL                  (0x40) ///< Apply succeeding CV Rule if this CVM is unsuccessful
#define EMV_CV_RULE_CVM_MASK                                    (0x3F) ///< Cardholder Verification (CV) Rule mask for CVM Code
#define EMV_CV_RULE_CVM_FAIL                                    (0x00) ///< CVM: Fail CVM processing
#define EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT                   (0x01) ///< CVM: Plaintext PIN verification performed by ICC
#define EMV_CV_RULE_CVM_ONLINE_PIN_ENCIPHERED                   (0x02) ///< CVM: Enciphered PIN verified online
#define EMV_CV_RULE_CVM_OFFLINE_PIN_PLAINTEXT_AND_SIGNATURE     (0x03) ///< CVM: Plaintext PIN verification performed by ICC and signature
#define EMV_CV_RULE_CVM_OFFLINE_PIN_ENCIPHERED                  (0x04) ///< CVM: Enciphered PIN verification performed by ICC
#define EMV_CV_RULE_CVM_OFFLINE_PIN_ENCIPHERED_AND_SIGNATURE    (0x05) ///< CVM: Enciphered PIN verification performed by ICC and signature
#define EMV_CV_RULE_CVM_OFFLINE_BIOMETRIC_FACIAL                (0x06) ///< CVM: Facial biometric verified offline (by ICC)
#define EMV_CV_RULE_CVM_ONLINE_BIOMETRIC_FACIAL                 (0x07) ///< CVM: Facial biometric verified online
#define EMV_CV_RULE_CVM_OFFLINE_BIOMETRIC_FINGER                (0x08) ///< CVM: Finger biometric verified offline (by ICC)
#define EMV_CV_RULE_CVM_ONLINE_BIOMETRIC_FINGER                 (0x09) ///< CVM: Finger biometric verified online
#define EMV_CV_RULE_CVM_OFFLINE_BIOMETRIC_PALM                  (0x0A) ///< CVM: Palm biometric verified offline (by ICC)
#define EMV_CV_RULE_CVM_ONLINE_BIOMETRIC_PALM                   (0x0B) ///< CVM: Palm biometric verified online
#define EMV_CV_RULE_CVM_OFFLINE_BIOMETRIC_IRIS                  (0x0C) ///< CVM: Iris biometric verified offline (by ICC)
#define EMV_CV_RULE_CVM_ONLINE_BIOMETRIC_IRIS                   (0x0D) ///< CVM: Iris biometric verified online
#define EMV_CV_RULE_CVM_OFFLINE_BIOMETRIC_VOICE                 (0x0E) ///< CVM: Voice biometric verified offline (by ICC)
#define EMV_CV_RULE_CVM_ONLINE_BIOMETRIC_VOICE                  (0x0F) ///< CVM: Voice biometric verified online
#define EMV_CV_RULE_CVM_SIGNATURE                               (0x1E) ///< CVM: Signature (paper)
#define EMV_CV_RULE_NO_CVM                                      (0x1F) ///< CVM: No CVM required
#define EMV_CV_RULE_INVALID                                     (0x3F) ///< Cardholder Verification (CV) Rule invalid

// Cardholder Verification (CV) Rule byte 2, CVM Condition Codes
// See EMV 4.4 Book 3, Annex C3, Table 44
#define EMV_CV_RULE_COND_ALWAYS                                 (0x00) ///< CVM Condition: Always
#define EMV_CV_RULE_COND_UNATTENDED_CASH                        (0x01) ///< CVM Condition: If unattended cash
#define EMV_CV_RULE_COND_NOT_CASH_OR_CASHBACK                   (0x02) ///< CVM Condition: If not unattended cash and not manual cash and not purchase with cashback
#define EMV_CV_RULE_COND_CVM_SUPPORTED                          (0x03) ///< CVM Condition: If terminal supports the CVM
#define EMV_CV_RULE_COND_MANUAL_CASH                            (0x04) ///< CVM Condition: If manual cash
#define EMV_CV_RULE_COND_CASHBACK                               (0x05) ///< CVM Condition: If purchase with cashback
#define EMV_CV_RULE_COND_LESS_THAN_X                            (0x06) ///< CVM Condition: If transaction is in the application currency and is under X value
#define EMV_CV_RULE_COND_MORE_THAN_X                            (0x07) ///< CVM Condition: If transaction is in the application currency and is over X value
#define EMV_CV_RULE_COND_LESS_THAN_Y                            (0x08) ///< CVM Condition: If transaction is in the application currency and is under Y value
#define EMV_CV_RULE_COND_MORE_THAN_Y                            (0x09) ///< CVM Condition: If transaction is in the application currency and is over Y value

// Cardholder Verification Method (CVM) Results (field 9F34) byte 1
// See EMV 4.4 Book 4, Annex A4, Table 33
// NOTE: This value is invalid for CV Rules but valid for CVM Results
#define EMV_CVM_NOT_PERFORMED                                   (0x3F) ///< CVM Performed: CVM not performed

// Cardholder Verification Method (CVM) Results (field 9F34) byte 3
// See EMV 4.4 Book 4, Annex A4, Table 33
#define EMV_CVM_RESULT_UNKNOWN                                  (0x00) ///< CVM Result: Unknown
#define EMV_CVM_RESULT_FAILED                                   (0x01) ///< CVM Result: Failed
#define EMV_CVM_RESULT_SUCCESSFUL                               (0x02) ///< CVM Result: Successful

// Terminal Verification Results (field 95) byte 1
// See EMV 4.4 Book 3, Annex C5, Table 46
#define EMV_TVR_OFFLINE_DATA_AUTH_NOT_PERFORMED                 (0x80) ///< Terminal Verification Results: Offline data authentication was not performed
#define EMV_TVR_SDA_FAILED                                      (0x40) ///< Terminal Verification Results: Static Data Authentication (SDA) failed
#define EMV_TVR_ICC_DATA_MISSING                                (0x20) ///< Terminal Verification Results: Integrated circuit card (ICC) data missing
#define EMV_TVR_CARD_ON_EXCEPTION_FILE                          (0x10) ///< Terminal Verification Results: Card appears on terminal exception file
#define EMV_TVR_DDA_FAILED                                      (0x08) ///< Terminal Verification Results: Dynamic Data Authentication (DDA) failed
#define EMV_TVR_CDA_FAILED                                      (0x04) ///< Terminal Verification Results: Combined DDA/Application Cryptogram Generation (CDA) failed
#define EMV_TVR_SDA_SELECTED                                    (0x02) ///< Terminal Verification Results: Static Data Authentication (SDA) selected
#define EMV_TVR_XDA_SELECTED                                    (0x01) ///< Terminal Verification Results: Extended Data Authentication (XDA) selected

// Terminal Verification Results (field 95) byte 2
// See EMV 4.4 Book 3, Annex C5, Table 46
#define EMV_TVR_APPLICATION_VERSIONS_DIFFERENT                  (0x80) ///< Terminal Verification Results: ICC and terminal have different application versions
#define EMV_TVR_APPLICATION_EXPIRED                             (0x40) ///< Terminal Verification Results: Expired application
#define EMV_TVR_APPLICATION_NOT_EFFECTIVE                       (0x20) ///< Terminal Verification Results: Application not yet effective
#define EMV_TVR_SERVICE_NOT_ALLOWED                             (0x10) ///< Terminal Verification Results: Requested service not allowed for card product
#define EMV_TVR_NEW_CARD                                        (0x08) ///< Terminal Verification Results: New card
#define EMV_TVR_RFU                                             (0x04) ///< Terminal Verification Results: RFU
#define EMV_TVR_BIOMETRIC_PERFORMED_SUCCESSFUL                  (0x02) ///< Terminal Verification Results: Biometric performed and successful
#define EMV_TVR_BIOMETRIC_TEMPLATE_FORMAT_NOT_SUPPORTED         (0x01) ///< Terminal Verification Results: Biometric template format not supported

// Terminal Verification Results (field 95) byte 3
// See EMV 4.4 Book 3, Annex C5, Table 46
#define EMV_TVR_CV_PROCESSING_FAILED                            (0x80) ///< Terminal Verification Results: Cardholder verification was not successful
#define EMV_TVR_CVM_UNRECOGNISED                                (0x40) ///< Terminal Verification Results: Unrecognised CVM
#define EMV_TVR_PIN_TRY_LIMIT_EXCEEDED                          (0x20) ///< Terminal Verification Results: PIN Try Limit exceeded
#define EMV_TVR_PIN_PAD_FAILED                                  (0x10) ///< Terminal Verification Results: PIN entry required and PIN pad not present or not working
#define EMV_TVR_PIN_NOT_ENTERED                                 (0x08) ///< Terminal Verification Results: PIN entry required, PIN pad present, but PIN was not entered
#define EMV_TVR_ONLINE_CVM_CAPTURED                             (0x04) ///< Terminal Verification Results: Online CVM captured
#define EMV_TVR_BIOMETRIC_CAPTURE_FAILED                        (0x02) ///< Terminal Verification Results: Biometric required but Biometric capture device not working
#define EMV_TVR_BIOMETRIC_SUBTYPE_BYPASSED                      (0x01) ///< Terminal Verification Results: Biometric required, Biometric capture device present, but Biometric Subtype entry was bypassed

// Terminal Verification Results (field 95) byte 4
// See EMV 4.4 Book 3, Annex C5, Table 46
#define EMV_TVR_TXN_FLOOR_LIMIT_EXCEEDED                        (0x80) ///< Terminal Verification Results: Transaction exceeds floor limit
#define EMV_TVR_LOWER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED        (0x40) ///< Terminal Verification Results: Lower consecutive offline limit exceeded
#define EMV_TVR_UPPER_CONSECUTIVE_OFFLINE_LIMIT_EXCEEDED        (0x20) ///< Terminal Verification Results: Upper consecutive offline limit exceeded
#define EMV_TVR_RANDOM_SELECTED_ONLINE                          (0x10) ///< Terminal Verification Results: Transaction selected randomly for online processing
#define EMV_TVR_MERCHANT_FORCED_ONLINE                          (0x08) ///< Terminal Verification Results: Merchant forced transaction online
#define EMV_TVR_BIOMETRIC_TRY_LIMIT_EXCEEDED                    (0x04) ///< Terminal Verification Results: Biometric Try Limit exceeded
#define EMV_TVR_BIOMETRIC_TYPE_NOT_SUPPORTED                    (0x02) ///< Terminal Verification Results: A selected Biometric Type not supported
#define EMV_TVR_XDA_FAILED                                      (0x01) ///< Terminal Verification Results: XDA signature verification failed

// Terminal Verification Results (field 95) byte 5
// See EMV 4.4 Book 3, Annex C5, Table 46
#define EMV_TVR_DEFAULT_TDOL                                    (0x80) ///< Terminal Verification Results: Default TDOL used
#define EMV_TVR_ISSUER_AUTHENTICATION_FAILED                    (0x40) ///< Terminal Verification Results: Issuer authentication failed
#define EMV_TVR_SCRIPT_PROCESSING_FAILED_BEFORE_GEN_AC          (0x20) ///< Terminal Verification Results: Script processing failed before final GENERATE AC
#define EMV_TVR_SCRIPT_PROCESSING_FAILED_AFTER_GEN_AC           (0x10) ///< Terminal Verification Results: Script processing failed after final GENERATE AC
#define EMV_TVR_CA_ECC_KEY_MISSING                              (0x04) ///< Terminal Verification Results: CA ECC key missing
#define EMV_TVR_ECC_KEY_RECOVERY_FAILED                         (0x02) ///< Terminal Verification Results: ECC key recovery failed
#define EMV_TVR_RESERVED_FOR_CONTACTLESS                        (0x09) ///< Terminal Verification Results: Reserved for use by the EMV Contactless Specifications

// Transaction Status Information (field 9B) byte 1
// See EMV 4.4 Book 3, Annex C6, Table 47
#define EMV_TSI_OFFLINE_DATA_AUTH_PERFORMED                     (0x80) ///< Transaction Status Information: Offline data authentication was performed
#define EMV_TSI_CV_PERFORMED                                    (0x40) ///< Transaction Status Information: Cardholder verification was performed
#define EMV_TSI_CARD_RISK_MANAGEMENT_PERFORMED                  (0x20) ///< Transaction Status Information: Card risk management was performed
#define EMV_TSI_ISSUER_AUTHENTICATION_PERFORMED                 (0x10) ///< Transaction Status Information: Issuer authentication was performed
#define EMV_TSI_TERMINAL_RISK_MANAGEMENT_PERFORMED              (0x08) ///< Transaction Status Information: Terminal risk management was performed
#define EMV_TSI_SCRIPT_PROCESSING_PERFORMED                     (0x04) ///< Transaction Status Information: Script processing was performed
#define EMV_TSI_BYTE1_RFU                                       (0x03) ///< Transaction Status Information: RFU

// Transaction Status Information (field 9B) byte 2
// See EMV 4.4 Book 3, Annex C6, Table 47
#define EMV_TSI_BYTE2_RFU                                       (0xFF) ///< Transaction Status Information: RFU

/// Issuer Application Data (field 9F10) formats
enum emv_iad_format_t {
	EMV_IAD_FORMAT_INVALID = -1, ///< Invalid IAD format
	EMV_IAD_FORMAT_UNKNOWN = 0, ///< Unknown IAD format
	EMV_IAD_FORMAT_CCD, ///< CCD Version 4.1 IAD Format
	EMV_IAD_FORMAT_MCHIP4, ///< Mastercard M/Chip 4 IAD Format
	EMV_IAD_FORMAT_MCHIP_ADVANCE, ///< Mastercard M/Chip Advance IAD Format
	EMV_IAD_FORMAT_VSDC_0, ///< Visa Smart Debit/Credit (VSDC) IAD Format 0
	EMV_IAD_FORMAT_VSDC_1, ///< Visa Smart Debit/Credit (VSDC) IAD Format 1
	EMV_IAD_FORMAT_VSDC_2, ///< Visa Smart Debit/Credit (VSDC) IAD Format 2
	EMV_IAD_FORMAT_VSDC_3, ///< Visa Smart Debit/Credit (VSDC) IAD Format 3
	EMV_IAD_FORMAT_VSDC_4, ///< Visa Smart Debit/Credit (VSDC) IAD Format 4
};

// Issuer Application Data (field 9F10) for Common Core Definitions (CCD) compliant applications
// See EMV 4.4 Book 3, Annex C9.1, Table CCD 8
// See EMV 4.4 Book 3, Annex C9.2, Table CCD 9
#define EMV_IAD_CCD_LEN                                         (32) ///< Length of Issuer Application Data in bytes
#define EMV_IAD_CCD_BYTE1                                       (0x0F) ///< Issuer Application Data: Byte 1 value
#define EMV_IAD_CCD_BYTE17                                      (0x0F) ///< Issuer Application Data: Byte 17 value
#define EMV_IAD_CCD_CCI_FC_MASK                                 (0xF0) ///< Issuer Application Data mask for Common Core IAD Format Code (FC)
#define EMV_IAD_CCD_CCI_FC_4_1                                  (0xA0) ///< Common Core IAD Format Code (FC): CCD Version 4.1 IAD Format
#define EMV_IAD_CCD_CCI_CV_MASK                                 (0x0F) ///< Issuer Application Data mask for Common Core Cryptogram Version (CV)
#define EMV_IAD_CCD_CCI_CV_4_1_TDES                             (0x05) ///< Common Core Cryptogram Version (CV): CCD Version 4.1 Cryptogram Version for TDES
#define EMV_IAD_CCD_CCI_CV_4_1_AES                              (0x06) ///< Common Core Cryptogram Version (CV): CCD Version 4.1 Cryptogram Version for AES

// Card Verification Results (CVR) byte 1 for Common Core Definitions (CCD) compliant applications
// See EMV 4.4 Book 3, Annex C9.3, Table CCD 10
#define EMV_IAD_CCD_CVR_BYTE1_2GAC_MASK                         (0xC0) ///< Card Verification Results (CVR) mask for Application Cryptogram Type Returned in Second GENERATE AC
#define EMV_IAD_CCD_CVR_BYTE1_2GAC_AAC                          (0x00) ///< Card Verification Results (CVR): Second GENERATE AC returned AAC
#define EMV_IAD_CCD_CVR_BYTE1_2GAC_TC                           (0x40) ///< Card Verification Results (CVR): Second GENERATE AC returned TC
#define EMV_IAD_CCD_CVR_BYTE1_2GAC_NOT_REQUESTED                (0x80) ///< Card Verification Results (CVR): Second GENERATE AC Not Requested
#define EMV_IAD_CCD_CVR_BYTE1_2GAC_RFU                          (0xC0) ///< Card Verification Results (CVR): Second GENERATE AC RFU
#define EMV_IAD_CCD_CVR_BYTE1_1GAC_MASK                         (0x30) ///< Card Verification Results (CVR) mask for Application Cryptogram Type Returned in First GENERATE AC
#define EMV_IAD_CCD_CVR_BYTE1_1GAC_AAC                          (0x00) ///< Card Verification Results (CVR): First GENERATE AC returned AAC
#define EMV_IAD_CCD_CVR_BYTE1_1GAC_TC                           (0x10) ///< Card Verification Results (CVR): First GENERATE AC returned TC
#define EMV_IAD_CCD_CVR_BYTE1_1GAC_ARQC                         (0x20) ///< Card Verification Results (CVR): First GENERATE AC returned ARQC
#define EMV_IAD_CCD_CVR_BYTE1_1GAC_RFU                          (0x30) ///< Card Verification Results (CVR): First GENERATE AC RFU
#define EMV_IAD_CCD_CVR_BYTE1_CDA_PERFORMED                     (0x08) ///< Card Verification Results (CVR): Combined DDA/Application Cryptogram Generation (CDA) Performed
#define EMV_IAD_CCD_CVR_BYTE1_OFFLINE_DDA_PERFORMED             (0x04) ///< Card Verification Results (CVR): Offline Dynamic Data Authentication (DDA) Performed
#define EMV_IAD_CCD_CVR_BYTE1_ISSUER_AUTH_NOT_PERFORMED         (0x02) ///< Card Verification Results (CVR): Issuer Authentication Not Performed
#define EMV_IAD_CCD_CVR_BYTE1_ISSUER_AUTH_FAILED                (0x01) ///< Card Verification Results (CVR): Issuer Authentication Failed

// Card Verification Results (CVR) byte 2 for Common Core Definitions (CCD) compliant applications
// See EMV 4.4 Book 3, Annex C9.3, Table CCD 10
#define EMV_IAD_CCD_CVR_BYTE2_PIN_TRY_COUNTER_MASK              (0xF0) ///< Card Verification Results (CVR) mask for Low Order Nibble of PIN Try Counter
#define EMV_IAD_CCD_CVR_BYTE2_PIN_TRY_COUNTER_SHIFT             (4) ///< Card Verification Results (CVR) shift for Low Order Nibble of PIN Try Counter
#define EMV_IAD_CCD_CVR_BYTE2_OFFLINE_PIN_PERFORMED             (0x08) ///< Card Verification Results (CVR): Offline PIN Verification Performed
#define EMV_IAD_CCD_CVR_BYTE2_OFFLINE_PIN_NOT_SUCCESSFUL        (0x04) ///< Card Verification Results (CVR): Offline PIN Verification Performed and PIN Not Successfully Verified
#define EMV_IAD_CCD_CVR_BYTE2_PIN_TRY_LIMIT_EXCEEDED            (0x02) ///< Card Verification Results (CVR): PIN Try Limit Exceeded
#define EMV_IAD_CCD_CVR_BYTE2_LAST_ONLINE_TXN_NOT_COMPLETED     (0x01) ///< Card Verification Results (CVR): Last Online Transaction Not Completed

// Card Verification Results (CVR) byte 3 for Common Core Definitions (CCD) compliant applications
// See EMV 4.4 Book 3, Annex C9.3, Table CCD 10
#define EMV_IAD_CCD_CVR_BYTE3_L_OFFLINE_TXN_CNT_LIMIT_EXCEEDED  (0x80) ///< Card Verification Results (CVR): Lower Offline Transaction Count Limit Exceeded
#define EMV_IAD_CCD_CVR_BYTE3_U_OFFLINE_TXN_CNT_LIMIT_EXCEEDED  (0x40) ///< Card Verification Results (CVR): Upper Offline Transaction Count Limit Exceeded
#define EMV_IAD_CCD_CVR_BYTE3_L_OFFLINE_AMOUNT_LIMIT_EXCEEDED   (0x20) ///< Card Verification Results (CVR): Lower Cumulative Offline Amount Limit Exceeded
#define EMV_IAD_CCD_CVR_BYTE3_U_OFFLINE_AMOUNT_LIMIT_EXCEEDED   (0x20) ///< Card Verification Results (CVR): Upper Cumulative Offline Amount Limit Exceeded
#define EMV_IAD_CCD_CVR_BYTE3_ISSUER_DISCRETIONARY_BIT1         (0x08) ///< Card Verification Results (CVR): Issuer-discretionary bit 1
#define EMV_IAD_CCD_CVR_BYTE3_ISSUER_DISCRETIONARY_BIT2         (0x04) ///< Card Verification Results (CVR): Issuer-discretionary bit 2
#define EMV_IAD_CCD_CVR_BYTE3_ISSUER_DISCRETIONARY_BIT3         (0x02) ///< Card Verification Results (CVR): Issuer-discretionary bit 3
#define EMV_IAD_CCD_CVR_BYTE3_ISSUER_DISCRETIONARY_BIT4         (0x01) ///< Card Verification Results (CVR): Issuer-discretionary bit 4

// Card Verification Results (CVR) byte 4 for Common Core Definitions (CCD) compliant applications
// See EMV 4.4 Book 3, Annex C9.3, Table CCD 10
#define EMV_IAD_CCD_CVR_BYTE4_SCRIPT_COUNT_MASK                 (0xF0) ///< Card Verification Results (CVR) mask for Number of Successfully Processed Issuer Script Commands Containing Secure Messaging
#define EMV_IAD_CCD_CVR_BYTE4_SCRIPT_COUNT_SHIFT                (4) ///< Card Verification Results (CVR) shift for Number of Successfully Processed Issuer Script Commands Containing Secure Messaging
#define EMV_IAD_CCD_CVR_BYTE4_ISSUER_SCRIPT_PROCESSING_FAILED   (0x08) ///< Card Verification Results (CVR): Issuer Script Processing Failed
#define EMV_IAD_CCD_CVR_BYTE4_ODA_FAILED_ON_PREVIOUS_TXN        (0x04) ///< Card Verification Results (CVR): Offline Data Authentication Failed on Previous Transaction
#define EMV_IAD_CCD_CVR_BYTE4_GO_ONLINE_ON_NEXT_TXN             (0x02) ///< Card Verification Results (CVR): Go Online on Next Transaction Was Set
#define EMV_IAD_CCD_CVR_BYTE4_UNABLE_TO_GO_ONLINE               (0x01) ///< Card Verification Results (CVR): Unable to go Online

// Issuer Application Data (field 9F10) for Mastercard M/Chip applications
// See M/Chip Requirements for Contact and Contactless, 15 March 2022, Appendix B, Issuer Application Data, 9F10
#define EMV_IAD_MCHIP4_LEN                                      (18) ///< Length of M/Chip 4 Issuer Application Data
#define EMV_IAD_MCHIPADV_LEN_18                                 (18) ///< Length of M/Chip Advance Issuer Application Data
#define EMV_IAD_MCHIPADV_LEN_20                                 (20) ///< Length of M/Chip Advance Issuer Application Data
#define EMV_IAD_MCHIPADV_LEN_26                                 (26) ///< Length of M/Chip Advance Issuer Application Data
#define EMV_IAD_MCHIPADV_LEN_28                                 (28) ///< Length of M/Chip Advance Issuer Application Data

// Cryptogram Version Number (CVN) for Mastercard M/Chip applications
#define EMV_IAD_MCHIP_CVN_MASK                                  (0xF0) ///< M/Chip Cryptogram Version Number (CVN) mask for version magic
#define EMV_IAD_MCHIP_CVN_VERSION_MAGIC                         (0x10) ///< M/Chip Cryptogram Version Number (CVN): Version magic number
#define EMV_IAD_MCHIP_CVN_RFU                                   (0x08) ///< M/Chip Cryptogram Version Number (CVN): RFU
#define EMV_IAD_MCHIP_CVN_SESSION_KEY_MASK                      (0x06) ///< M/Chip Cryptogram Version Number (CVN) mask for session key
#define EMV_IAD_MCHIP_CVN_SESSION_KEY_MASTERCARD_SKD            (0x00) ///< M/Chip Cryptogram: Mastercard Proprietary SKD session key
#define EMV_IAD_MCHIP_CVN_SESSION_KEY_EMV_CSK                   (0x04) ///< M/Chip Cryptogram: EMV CSK session key
#define EMV_IAD_MCHIP_CVN_COUNTERS_INCLUDED                     (0x01) ///< M/Chip Cryptogram: Counter included in AC data

// Card Verification Results (CVR) byte 1 for Mastercard M/Chip applications
#define EMV_IAD_MCHIP_CVR_BYTE1_2GAC_MASK                       (0xC0) ///< Card Verification Results (CVR) mask for Application Cryptogram Type Returned in Second GENERATE AC
#define EMV_IAD_MCHIP_CVR_BYTE1_2GAC_AAC                        (0x00) ///< Card Verification Results (CVR): Second GENERATE AC returned AAC
#define EMV_IAD_MCHIP_CVR_BYTE1_2GAC_TC                         (0x40) ///< Card Verification Results (CVR): Second GENERATE AC returned TC
#define EMV_IAD_MCHIP_CVR_BYTE1_2GAC_NOT_REQUESTED              (0x80) ///< Card Verification Results (CVR): Second GENERATE AC Not Requested
#define EMV_IAD_MCHIP_CVR_BYTE1_2GAC_RFU                        (0xC0) ///< Card Verification Results (CVR): Second GENERATE AC RFU
#define EMV_IAD_MCHIP_CVR_BYTE1_1GAC_MASK                       (0x30) ///< Card Verification Results (CVR) mask for Application Cryptogram Type Returned in First GENERATE AC
#define EMV_IAD_MCHIP_CVR_BYTE1_1GAC_AAC                        (0x00) ///< Card Verification Results (CVR): First GENERATE AC returned AAC
#define EMV_IAD_MCHIP_CVR_BYTE1_1GAC_TC                         (0x10) ///< Card Verification Results (CVR): First GENERATE AC returned TC
#define EMV_IAD_MCHIP_CVR_BYTE1_1GAC_ARQC                       (0x20) ///< Card Verification Results (CVR): First GENERATE AC returned ARQC
#define EMV_IAD_MCHIP_CVR_BYTE1_1GAC_RFU                        (0x30) ///< Card Verification Results (CVR): First GENERATE AC RFU
#define EMV_IAD_MCHIP_CVR_BYTE1_DATE_CHECK_FAILED               (0x08) ///< Card Verification Results (CVR): Date Check Failed
#define EMV_IAD_MCHIP_CVR_BYTE1_OFFLINE_PIN_PERFORMED           (0x04) ///< Card Verification Results (CVR): Offline PIN Verification Performed
#define EMV_IAD_MCHIP_CVR_BYTE1_OFFLINE_ENCRYPTED_PIN_PERFORMED (0x02) ///< Card Verification Results (CVR): Offline Encrypted PIN Verification Performed
#define EMV_IAD_MCHIP_CVR_BYTE1_OFFLINE_PIN_SUCCESSFUL          (0x01) ///< Card Verification Results (CVR): Offline PIN Verification Successful

// Card Verification Results (CVR) byte 2 for Mastercard M/Chip applications
#define EMV_IAD_MCHIP_CVR_BYTE2_DDA                             (0x80) ///< Card Verification Results (CVR): Dynamic Data Authentication (DDA) Returned
#define EMV_IAD_MCHIP_CVR_BYTE2_1GAC_CDA                        (0x40) ///< Card Verification Results (CVR): Combined DDA/Application Cryptogram Generation (CDA) Returned in First GENERATE AC
#define EMV_IAD_MCHIP_CVR_BYTE2_2GAC_CDA                        (0x20) ///< Card Verification Results (CVR): Combined DDA/Application Cryptogram Generation (CDA) Returned in Second GENERATE AC
#define EMV_IAD_MCHIP_CVR_BYTE2_ISSUER_AUTH_PERFORMED           (0x10) ///< Card Verification Results (CVR): Issuer Authentication Performed
#define EMV_IAD_MCHIP_CVR_BYTE2_CIAC_SKIPPED_ON_CAT3            (0x08) ///< Card Verification Results (CVR): Card Issuer Action Codes (CIAC) Default Skipped on Cardholder Activated Terminal Level 3 (CAT3)
#define EMV_IAD_MCHIP_CVR_BYTE2_OFFLINE_CHANGE_PIN_SUCCESSFUL   (0x04) ///< Card Verification Results (CVR): Offline Change PIN Result Successful
#define EMV_IAD_MCHIP_CVR_BYTE2_ISSUER_DISCRETIONARY            (0x03) ///< Card Verification Results (CVR): Issuer Discretionary

// Card Verification Results (CVR) byte 3 for Mastercard M/Chip applications
#define EMV_IAD_MCHIP_CVR_BYTE3_SCRIPT_COUNTER_MASK             (0xF0) ///< Card Verification Results (CVR) mask for Right Nibble of Script Counter
#define EMV_IAD_MCHIP_CVR_BYTE3_SCRIPT_COUNTER_SHIFT            (4) ///< Card Verification Results (CVR) shift for Right Nibble of Script Counter
#define EMV_IAD_MCHIP_CVR_BYTE3_PIN_TRY_COUNTER_MASK            (0x0F) ///< Card Verification Results (CVR) mask for Right Nibble of PIN Try Counter
#define EMV_IAD_MCHIP_CVR_BYTE3_PIN_TRY_COUNTER_SHIFT           (0) ///< Card Verification Results (CVR) shift for Right Nibble of PIN Try Counter

// Card Verification Results (CVR) byte 4 for Mastercard M/Chip applications
#define EMV_IAD_MCHIP_CVR_BYTE4_LAST_ONLINE_TXN_NOT_COMPLETED   (0x80) ///< Card Verification Results (CVR): Last Online Transaction Not Completed
#define EMV_IAD_MCHIP_CVR_BYTE4_UNABLE_TO_GO_ONLINE             (0x40) ///< Card Verification Results (CVR): Unable To Go Online
#define EMV_IAD_MCHIP_CVR_BYTE4_OFFLINE_PIN_NOT_PERFORMED       (0x20) ///< Card Verification Results (CVR): Offline PIN Verification Not Performed
#define EMV_IAD_MCHIP_CVR_BYTE4_OFFLINE_PIN_FAILED              (0x10) ///< Card Verification Results (CVR): Offline PIN Verification Failed
#define EMV_IAD_MCHIP_CVR_BYTE4_PTL_EXCEEDED                    (0x08) ///< Card Verification Results (CVR): PTL Exceeded
#define EMV_IAD_MCHIP_CVR_BYTE4_INTERNATIONAL_TXN               (0x04) ///< Card Verification Results (CVR): International Transaction
#define EMV_IAD_MCHIP_CVR_BYTE4_DOMESTIC_TXN                    (0x02) ///< Card Verification Results (CVR): Domestic Transaction
#define EMV_IAD_MCHIP_CVR_BYTE4_ERR_OFFLINE_PIN_OK              (0x01) ///< Card Verification Results (CVR): Terminal Erroneously Considers Offline PIN OK

// Card Verification Results (CVR) byte 5 for Mastercard M/Chip applications
#define EMV_IAD_MCHIP_CVR_BYTE5_L_CONSECUTIVE_LIMIT_EXCEEDED    (0x80) ///< Card Verification Results (CVR): Lower Consecutive Offline (M/Chip 4) or Counter 1 (M/Chip Advance) Limit Exceeded
#define EMV_IAD_MCHIP_CVR_BYTE5_U_CONSECUTIVE_LIMIT_EXCEEDED    (0x40) ///< Card Verification Results (CVR): Upper Consecutive Offline (M/Chip 4) or Counter 1 (M/Chip Advance) Limit Exceeded
#define EMV_IAD_MCHIP_CVR_BYTE5_L_CUMULATIVE_LIMIT_EXCEEDED     (0x20) ///< Card Verification Results (CVR): Lower Cumulative Offline (M/Chip 4) or Accumulator (M/Chip Advance) Limit Exceeded
#define EMV_IAD_MCHIP_CVR_BYTE5_U_CUMULATIVE_LIMIT_EXCEEDED     (0x10) ///< Card Verification Results (CVR): Upper Cumulative Offline (M/Chip 4) or Accumulator (M/Chip Advance) Limit Exceeded
#define EMV_IAD_MCHIP_CVR_BYTE5_GO_ONLINE_ON_NEXT_TXN           (0x08) ///< Card Verification Results (CVR): Go Online On Next Transaction Was Set
#define EMV_IAD_MCHIP_CVR_BYTE5_ISSUER_AUTH_FAILED              (0x04) ///< Card Verification Results (CVR): Issuer Authentication Failed
#define EMV_IAD_MCHIP_CVR_BYTE5_SCRIPT_RECEIVED                 (0x02) ///< Card Verification Results (CVR): Script Received
#define EMV_IAD_MCHIP_CVR_BYTE5_SCRIPT_FAILED                   (0x01) ///< Card Verification Results (CVR): Script Failed

// Card Verification Results (CVR) byte 6 for Mastercard M/Chip applications
#define EMV_IAD_MCHIP_CVR_BYTE6_L_CONSECUTIVE_LIMIT_EXCEEDED    (0x80) ///< Card Verification Results (CVR): Lower Consecutive Counter 2 Limit Exceeded
#define EMV_IAD_MCHIP_CVR_BYTE6_U_CONSECUTIVE_LIMIT_EXCEEDED    (0x40) ///< Card Verification Results (CVR): Upper Consecutive Counter 2 Limit Exceeded
#define EMV_IAD_MCHIP_CVR_BYTE6_L_CUMULATIVE_LIMIT_EXCEEDED     (0x20) ///< Card Verification Results (CVR): Lower Cumulative Accumulator 2 Limit Exceeded
#define EMV_IAD_MCHIP_CVR_BYTE6_U_CUMULATIVE_LIMIT_EXCEEDED     (0x10) ///< Card Verification Results (CVR): Upper Cumulative Accumulator 2 Limit Exceeded
#define EMV_IAD_MCHIP_CVR_BYTE6_MTA_LIMIT_EXCEEDED              (0x08) ///< Card Verification Results (CVR): MTA Limit Exceeded
#define EMV_IAD_MCHIP_CVR_BYTE6_NUM_OF_DAYS_LIMIT_EXCEEDED      (0x04) ///< Card Verification Results (CVR): Number Of Days Offline Limit Exceeded
#define EMV_IAD_MCHIP_CVR_BYTE6_MATCH_ADDITIONAL_CHECK_TABLE    (0x02) ///< Card Verification Results (CVR): Match Found In Additional Check Table
#define EMV_IAD_MCHIP_CVR_BYTE6_NO_MATCH_ADDITIONAL_CHECK_TABLE (0x01) ///< Card Verification Results (CVR): No Match Found In Additional Check Table

// Issuer Application Data (field 9F10) for Visa Smart Debit/Credit (VSDC) applications
// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Appendix M
#define EMV_IAD_VSDC_0_1_3_MIN_LEN                              (7) ///< Minimum length of Issuer Application Data for VSDC IAD format 0/1/3
#define EMV_IAD_VSDC_0_1_3_BYTE1                                (0x06) ///< Issuer Application Data: Byte 1 value for VSDC IAD Format 0/1/3
#define EMV_IAD_VSDC_2_4_LEN                                    (32) ///< Length of Issuer Application Data for VSDC IAD format 2/4
#define EMV_IAD_VSDC_2_4_BYTE1                                  (0x1F) ///< Issuer Application Data: Byte 1 value for VSDC IAD Format 2/4

// Cryptogram Version Number (CVN) for Visa Smart Debit/Credit (VSDC) applications
#define EMV_IAD_VSDC_CVN_FORMAT_MASK                            (0xF0) ///< Cryptogram Version Number (CVN) mask for VSDC IAD format
#define EMV_IAD_VSDC_CVN_FORMAT_SHIFT                           (4) ///< Cryptogram Version Number (CVN) shift for VSDC IAD format

// Card Verification Results (CVR) byte 1 for Visa Smart Debit/Credit (VSDC) IAD Format 0/1/3 applications
#define EMV_IAD_VSDC_0_1_3_CVR_LEN                              (3) ///< Length of Card Verification Results (CVR) for VSDC IAD Format 0/1/3

// Card Verification Results (CVR) byte 1 for Visa Smart Debit/Credit (VSDC) IAD Format 2/4 applications
// NOTE: From unverified internet sources
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_MASK                  (0xF0) ///< Card Verification Results (CVR) mask CVM Verifying Entity
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_NONE                  (0x00) ///< Card Verification Results (CVR): No CDCVM
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_VMPA                  (0x10) ///< Card Verification Results (CVR): Visa Mobile Payment Application (VMPA)
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_MG                    (0x20) ///< Card Verification Results (CVR): MG
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_SE_APP                (0x30) ///< Card Verification Results (CVR): Co-residing SE app
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_TEE_APP               (0x40) ///< Card Verification Results (CVR): TEE app
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_MOBILE_APP            (0x50) ///< Card Verification Results (CVR): Mobile Application
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_TERMINAL              (0x60) ///< Card Verification Results (CVR): Terminal
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_CLOUD                 (0x70) ///< Card Verification Results (CVR): Verified in the cloud
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_ENTITY_MOBILE_DEVICE_OS      (0x80) ///< Card Verification Results (CVR): Verified by the mobile device OS
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_MASK                    (0x0F) ///< Card Verification Results (CVR) mask CVM Verified Type
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_NONE                    (0x00) ///< Card Verification Results (CVR): No CDCVM
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_PASSCODE                (0x01) ///< Card Verification Results (CVR): Passcode
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_BIOMETRIC_FINGER        (0x02) ///< Card Verification Results (CVR): Finger biometric
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_MOBILE_DEVICE_PATTERN   (0x03) ///< Card Verification Results (CVR): Mobile device pattern
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_BIOMETRIC_FACIAL        (0x04) ///< Card Verification Results (CVR): Facial biometric
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_BIOMETRIC_IRIS          (0x05) ///< Card Verification Results (CVR): Iris biometric
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_BIOMETRIC_VOICE         (0x06) ///< Card Verification Results (CVR): Voice biometric
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_BIOMETRIC_PALM          (0x07) ///< Card Verification Results (CVR): Palm biometric
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_SIGNATURE               (0x0D) ///< Card Verification Results (CVR): Signature
#define EMV_IAD_VSDC_CVR_BYTE1_CVM_TYPE_ONLINE_PIN              (0x0E) ///< Card Verification Results (CVR): Online PIN

// Card Verification Results (CVR) byte 2 for Visa Smart Debit/Credit (VSDC) IAD Format 0/1/2/3/4 applications
#define EMV_IAD_VSDC_CVR_BYTE2_2GAC_MASK                        (0xC0) ///< Card Verification Results (CVR) mask for Application Cryptogram Type Returned in Second GENERATE AC
#define EMV_IAD_VSDC_CVR_BYTE2_2GAC_AAC                         (0x00) ///< Card Verification Results (CVR): Second GENERATE AC returned AAC
#define EMV_IAD_VSDC_CVR_BYTE2_2GAC_TC                          (0x40) ///< Card Verification Results (CVR): Second GENERATE AC returned TC
#define EMV_IAD_VSDC_CVR_BYTE2_2GAC_NOT_REQUESTED               (0x80) ///< Card Verification Results (CVR): Second GENERATE AC Not Requested
#define EMV_IAD_VSDC_CVR_BYTE2_2GAC_RFU                         (0xC0) ///< Card Verification Results (CVR): Second GENERATE AC RFU
#define EMV_IAD_VSDC_CVR_BYTE2_1GAC_MASK                        (0x30) ///< Card Verification Results (CVR) mask for Application Cryptogram Type Returned in First GENERATE AC (for contact) or GPO (for contactless)
#define EMV_IAD_VSDC_CVR_BYTE2_1GAC_AAC                         (0x00) ///< Card Verification Results (CVR): First GENERATE AC or GPO returned AAC
#define EMV_IAD_VSDC_CVR_BYTE2_1GAC_TC                          (0x10) ///< Card Verification Results (CVR): First GENERATE AC or GPO returned TC
#define EMV_IAD_VSDC_CVR_BYTE2_1GAC_ARQC                        (0x20) ///< Card Verification Results (CVR): First GENERATE AC or GPO returned ARQC
#define EMV_IAD_VSDC_CVR_BYTE2_1GAC_RFU                         (0x30) ///< Card Verification Results (CVR): First GENERATE AC or GPO RFU
#define EMV_IAD_VSDC_CVR_BYTE2_ISSUER_AUTH_FAILED               (0x08) ///< Card Verification Results (CVR): Issuer Authentication performed and failed
#define EMV_IAD_VSDC_CVR_BYTE2_OFFLINE_PIN_PERFORMED            (0x04) ///< Card Verification Results (CVR): Offline PIN verification performed (format 0/1/3)
#define EMV_IAD_VSDC_CVR_BYTE2_CDCVM_PERFORMED                  (0x04) ///< Card Verification Results (CVR): CDCVM successfully performed (format 2/4)
#define EMV_IAD_VSDC_CVR_BYTE2_OFFLINE_PIN_FAILED               (0x02) ///< Card Verification Results (CVR): Offline PIN verification failed (format 0/1/3)
#define EMV_IAD_VSDC_CVR_BYTE2_RFU                              (0x02) ///< Card Verification Results (CVR): RFU (format 2/4)
#define EMV_IAD_VSDC_CVR_BYTE2_UNABLE_TO_GO_ONLINE              (0x01) ///< Card Verification Results (CVR): Unable to go online

// Card Verification Results (CVR) byte 3 for Visa Smart Debit/Credit (VSDC) IAD Format 0/1/2/3/4 applications
// NOTE: From unverified internet sources
#define EMV_IAD_VSDC_CVR_BYTE3_LAST_ONLINE_TXN_NOT_COMPLETED    (0x80) ///< Card Verification Results (CVR): Last online transaction not completed
#define EMV_IAD_VSDC_CVR_BYTE3_PIN_TRY_LIMIT_EXCEEDED           (0x40) ///< Card Verification Results (CVR): PIN Try Limit exceeded
#define EMV_IAD_VSDC_CVR_BYTE3_VELOCITY_COUNTERS_EXCEEDED       (0x20) ///< Card Verification Results (CVR): Exceeded velocity checking counters
#define EMV_IAD_VSDC_CVR_BYTE3_NEW_CARD                         (0x10) ///< Card Verification Results (CVR): New card
#define EMV_IAD_VSDC_CVR_BYTE3_LAST_ONLINE_ISSUER_AUTH_FAILED   (0x08) ///< Card Verification Results (CVR): Issuer Authentication failure on last online transaction
#define EMV_IAD_VSDC_CVR_BYTE3_ISSUER_AUTH_NOT_PERFORMED        (0x04) ///< Card Verification Results (CVR): Issuer Authentication not performed after online authorization
#define EMV_IAD_VSDC_CVR_BYTE3_APPLICATION_BLOCKED              (0x02) ///< Card Verification Results (CVR): Application blocked by card because PIN Try Limit exceeded
#define EMV_IAD_VSDC_CVR_BYTE3_LAST_OFFLINE_SDA_FAILED          (0x01) ///< Card Verification Results (CVR): Offline static data authentication failed on last transaction

// Card Verification Results (CVR) byte 4 for Visa Smart Debit/Credit (VSDC) IAD Format 0/1/2/3/4 applications
// NOTE: From unverified internet sources
#define EMV_IAD_VSDC_CVR_BYTE4_SCRIPT_COUNTER_MASK              (0xF0) ///< Card Verification Results (CVR) mask for Number of Issuer Script Commands
#define EMV_IAD_VSDC_CVR_BYTE4_SCRIPT_COUNTER_SHIFT             (4) ///< Card Verification Results (CVR) shift for Number of Issuer Script Commands
#define EMV_IAD_VSDC_CVR_BYTE4_ISSUER_SCRIPT_PROCESSING_FAILED  (0x08) ///< Card Verification Results (CVR): Issuer Script processing failed
#define EMV_IAD_VSDC_CVR_BYTE4_LAST_OFFLINE_DDA_FAILED          (0x04) ///< Card Verification Results (CVR): Offline dynamic data authentication failed on last transaction
#define EMV_IAD_VSDC_CVR_BYTE4_OFFLINE_DDA_PERFORMED            (0x02) ///< Card Verification Results (CVR): Offline dynamic data authentication performed
#define EMV_IAD_VSDC_CVR_BYTE4_PIN_VERIFICATION_NOT_RECEIVED    (0x01) ///< Card Verification Results (CVR): PIN verification command not received for a PIN-Expecting card

// Card Verification Results (CVR) byte 5 for Visa Smart Debit/Credit (VSDC) IAD Format 2/4 applications
// NOTE: From unverified internet sources
#define EMV_IAD_VSDC_CVR_BYTE5_CD_NOT_DEBUG_MODE                (0x80) ///< Card Verification Results (CVR): Consumer Device is not in debug mode
#define EMV_IAD_VSDC_CVR_BYTE5_CD_NOT_ROOTED                    (0x40) ///< Card Verification Results (CVR): Consumer Device is not a rooted device
#define EMV_IAD_VSDC_CVR_BYTE5_MOBILE_APP_NOT_HOOKED            (0x20) ///< Card Verification Results (CVR): Mobile Application is not hooked
#define EMV_IAD_VSDC_CVR_BYTE5_MOBILE_APP_INTEGRITY             (0x10) ///< Card Verification Results (CVR): Mobile Application integrity is intact
#define EMV_IAD_VSDC_CVR_BYTE5_CD_HAS_CONNECTIVITY              (0x08) ///< Card Verification Results (CVR): Consumer Device has data connectivity
#define EMV_IAD_VSDC_CVR_BYTE5_CD_IS_GENUINE                    (0x04) ///< Card Verification Results (CVR): Consumer Device is genuine
#define EMV_IAD_VSDC_CVR_BYTE5_CDCVM_PERFORMED                  (0x02) ///< Card Verification Results (CVR): CDCVM successfully performed
#define EMV_IAD_VSDC_CVR_BYTE5_EMV_SESSION_KEY                  (0x01) ///< Card Verification Results (CVR): Secure Messaging uses EMV Session key-based derivation

// Terminal Risk Management Data (field 9F1D) byte 1
// See EMV Contactless Book C-2 v2.11, Annex A.1.161
// See M/Chip Requirements for Contact and Contactless, 28 November 2023, Chapter 5, Terminal Risk Management Data
#define EMV_TRMD_BYTE1_RESTART_SUPPORTED                        (0x80) ///< Terminal Risk Management Data: Restart supported
#define EMV_TRMD_BYTE1_ENCIPHERED_PIN_ONLINE_CONTACTLESS        (0x40) ///< Terminal Risk Management Data: Enciphered PIN verified online (Contactless)
#define EMV_TRMD_BYTE1_SIGNATURE_CONTACTLESS                    (0x20) ///< Terminal Risk Management Data: Signature (paper) (Contactless)
#define EMV_TRMD_BYTE1_ENCIPHERED_PIN_OFFLINE_CONTACTLESS       (0x10) ///< Terminal Risk Management Data: Enciphered PIN verification performed by ICC (Contactless)
#define EMV_TRMD_BYTE1_NO_CVM_CONTACTLESS                       (0x08) ///< Terminal Risk Management Data: No CVM required (Contactless)
#define EMV_TRMD_BYTE1_CDCVM_CONTACTLESS                        (0x04) ///< Terminal Risk Management Data: CDCVM (Contactless)
#define EMV_TRMD_BYTE1_PLAINTEXT_PIN_OFFLINE_CONTACTLESS        (0x02) ///< Terminal Risk Management Data: Plaintext PIN verification performed by ICC (Contactless)
#define EMV_TRMD_BYTE1_PRESENT_AND_HOLD_SUPPORTED               (0x01) ///< Terminal Risk Management Data: Present and Hold supported

// Terminal Risk Management Data (field 9F1D) byte 2
// See EMV Contactless Book C-2 v2.11, Annex A.1.161
// See EMV Contactless Book C-8 v1.1, Annex A.1.129
// See M/Chip Requirements for Contact and Contactless, 28 November 2023, Chapter 5, Terminal Risk Management Data
#define EMV_TRMD_BYTE2_CVM_LIMIT_EXCEEDED                       (0x80) ///< Terminal Risk Management Data: CVM Limit exceeded
#define EMV_TRMD_BYTE2_ENCIPHERED_PIN_ONLINE_CONTACT            (0x40) ///< Terminal Risk Management Data: Enciphered PIN verified online (Contact)
#define EMV_TRMD_BYTE2_SIGNATURE_CONTACT                        (0x20) ///< Terminal Risk Management Data: Signature (paper) (Contact)
#define EMV_TRMD_BYTE2_ENCIPHERED_PIN_OFFLINE_CONTACT           (0x10) ///< Terminal Risk Management Data: Enciphered PIN verification performed by ICC (Contact)
#define EMV_TRMD_BYTE2_NO_CVM_CONTACT                           (0x08) ///< Terminal Risk Management Data: No CVM required (Contact)
#define EMV_TRMD_BYTE2_CDCVM_CONTACT                            (0x04) ///< Terminal Risk Management Data: CDCVM (Contact)
#define EMV_TRMD_BYTE2_PLAINTEXT_PIN_OFFLINE_CONTACT            (0x02) ///< Terminal Risk Management Data: Plaintext PIN verification performed by ICC (Contact)
#define EMV_TRMD_BYTE2_RFU                                      (0x01) ///< Terminal Risk Management Data: RFU

// Terminal Risk Management Data (field 9F1D) byte 3
// See EMV Contactless Book C-2 v2.11, Annex A.1.161
// See M/Chip Requirements for Contact and Contactless, 28 November 2023, Chapter 5, Terminal Risk Management Data
#define EMV_TRMD_BYTE3_MAGSTRIPE_MODE_CONTACTLESS_NOT_SUPPORTED (0x80) ///< Terminal Risk Management Data: Mag-stripe mode contactless transactions not supported
#define EMV_TRMD_BYTE3_EMV_MODE_CONTACTLESS_NOT_SUPPORTED       (0x40) ///< Terminal Risk Management Data: EMV mode contactless transactions not supported
#define EMV_TRMD_BYTE3_CDCVM_WITHOUT_CDA_SUPPORTED              (0x20) ///< Terminal Risk Management Data: CDCVM without CDA supported
#define EMV_TRMD_BYTE3_RFU                                      (0x1F) ///< Terminal Risk Management Data: RFU

// Terminal Risk Management Data (field 9F1D) byte 4
// See EMV Contactless Book C-2 v2.11, Annex A.1.161
// See EMV Contactless Book C-8 v1.1, Annex A.1.129
#define EMV_TRMD_BYTE4_CDCVM_BYPASS_REQUESTED                   (0x80) ///< Terminal Risk Management Data: CDCVM bypass requested
#define EMV_TRMD_BYTE4_SCA_EXEMPT                               (0x40) ///< Terminal Risk Management Data: SCA exempt
#define EMV_TRMD_BYTE4_RFU                                      (0x3F) ///< Terminal Risk Management Data: RFU

// Terminal Transaction Qualifiers (field 9F66) byte 1
// See EMV Contactless Book A v2.10, 5.7, Table 5-4
// See EMV Contactless Book C-3 v2.10, Annex A.2
// See EMV Contactless Book C-7 v2.9, 3.2.2, Table 3-1 (NOTE: byte 1 bit 7 is defined for proprietary use and ignored below)
#define EMV_TTQ_MAGSTRIPE_MODE_SUPPORTED                        (0x80) ///< Terminal Transaction Qualifiers: Mag-stripe mode supported
#define EMV_TTQ_BYTE1_RFU                                       (0x40) ///< Terminal Transaction Qualifiers: RFU
#define EMV_TTQ_EMV_MODE_SUPPORTED                              (0x20) ///< Terminal Transaction Qualifiers: EMV mode supported
#define EMV_TTQ_EMV_CONTACT_SUPPORTED                           (0x10) ///< Terminal Transaction Qualifiers: EMV contact chip supported
#define EMV_TTQ_OFFLINE_ONLY_READER                             (0x08) ///< Terminal Transaction Qualifiers: Offline-only reader
#define EMV_TTQ_ONLINE_PIN_SUPPORTED                            (0x04) ///< Terminal Transaction Qualifiers: Online PIN supported
#define EMV_TTQ_SIGNATURE_SUPPORTED                             (0x02) ///< Terminal Transaction Qualifiers: Signature supported
#define EMV_TTQ_ODA_FOR_ONLINE_AUTH_SUPPORTED                   (0x01) ///< Terminal Transaction Qualifiers: Offline Data Authentication for Online Authorizations supported

// Terminal Transaction Qualifiers (field 9F66) byte 2
// See EMV Contactless Book A v2.10, 5.7, Table 5-4
#define EMV_TTQ_ONLINE_CRYPTOGRAM_REQUIRED                      (0x80) ///< Terminal Transaction Qualifiers: Online cryptogram required
#define EMV_TTQ_CVM_REQUIRED                                    (0x40) ///< Terminal Transaction Qualifiers: CVM required
#define EMV_TTQ_OFFLINE_PIN_SUPPORTED                           (0x20) ///< Terminal Transaction Qualifiers: (Contact Chip) Offline PIN supported
#define EMV_TTQ_BYTE2_RFU                                       (0x1F) ///< Terminal Transaction Qualifiers: RFU

// Terminal Transaction Qualifiers (field 9F66) byte 3
// See EMV Contactless Book A v2.10, 5.7, Table 5-4
// See EMV Contactless Book C-6 v2.6, Annex D.11
#define EMV_TTQ_ISSUER_UPDATE_PROCESSING_SUPPORTED              (0x80) ///< Terminal Transaction Qualifiers: Issuer Update Processing supported
#define EMV_TTQ_CDCVM_SUPPORTED                                 (0x40) ///< Terminal Transaction Qualifiers: Consumer Device CVM supported
#define EMV_TTQ_CDCVM_REQUIRED                                  (0x08) ///< Terminal Transaction Qualifiers: Consumer Device CVM required
#define EMV_TTQ_BYTE3_RFU                                       (0x37) ///< Terminal Transaction Qualifiers: RFU

// Terminal Transaction Qualifiers (field 9F66) byte 4
// See EMV Contactless Book A v2.10, 5.7, Table 5-4
// See EMV Contactless Book C-7 v2.9, 3.2.2, Table 3-1
#define EMV_TTQ_FDDA_V1_SUPPORTED                               (0x80) ///< Terminal Transaction Qualifiers: fDDA v1.0 Supported
#define EMV_TTQ_BYTE4_RFU                                       (0x7F) ///< Terminal Transaction Qualifiers: RFU

// Card Transaction Qualifiers (field 9F6C) byte 1
// See EMV Contactless Book C-3 v2.10, Annex A.2
// See EMV Contactless Book C-7 v2.9, Annex A
// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
#define EMV_CTQ_ONLINE_PIN_REQUIRED                             (0x80) ///< Card Transaction Qualifiers: Online PIN Required
#define EMV_CTQ_SIGNATURE_REQUIRED                              (0x40) ///< Card Transaction Qualifiers: Signature Required
#define EMV_CTQ_ONLINE_IF_ODA_FAILED                            (0x20) ///< Card Transaction Qualifiers: Go Online if Offline Data Authentication Fails and Reader is online capable
#define EMV_CTQ_SWITCH_INTERFACE_IF_ODA_FAILED                  (0x10) ///< Card Transaction Qualifiers: Switch Interface if Offline Data Authentication fails and Reader supports contact chip
#define EMV_CTQ_ONLINE_IF_APPLICATION_EXPIRED                   (0x08) ///< Card Transaction Qualifiers: Go Online if Application Expired
#define EMV_CTQ_SWITCH_INTERFACE_IF_CASH                        (0x04) ///< Card Transaction Qualifiers: Switch Interface for Cash Transactions
#define EMV_CTQ_SWITCH_INTERFACE_IF_CASHBACK                    (0x02) ///< Card Transaction Qualifiers: Switch Interface for Cashback Transactions
#define EMV_CTQ_ATM_NOT_VALID                                   (0x01) ///< Card Transaction Qualifiers: Not valid for contactless ATM transactions

// Card Transaction Qualifiers (field 9F6C) byte 2
// See EMV Contactless Book C-3 v2.10, Annex A.2
// See EMV Contactless Book C-7 v2.9, Annex A
#define EMV_CTQ_CDCVM_PERFORMED                                 (0x80) ///< Card Transaction Qualifiers: Consumer Device CVM Performed
#define EMV_CTQ_ISSUER_UPDATE_PROCESSING_SUPPORTED              (0x40) ///< Card Transaction Qualifiers: Card supports Issuer Update Processing at the POS
#define EMV_CTQ_BYTE2_RFU                                       (0x3F) ///< Card Transaction Qualifiers: RFU

// Amex Contactless Reader Capabilities (field 9F6D)
// See EMV Contactless Book C-4 v2.10, 4.3.3, Table 4-2
#define AMEX_CL_READER_CAPS_MASK                                (0xC8) ///< Contactless Reader Capabilities mask to distinguish from Terminal Type bits
#define AMEX_CL_READER_CAPS_DEPRECATED                          (0x00) ///< Contactless Reader Capabilities: Deprecated
#define AMEX_CL_READER_CAPS_MAGSTRIPE_CVM_NOT_REQUIRED          (0x40) ///< Contactless Reader Capabilities: Mag-stripe CVM Not Required
#define AMEX_CL_READER_CAPS_MAGSTRIPE_CVM_REQUIRED              (0x48) ///< Contactless Reader Capabilities: Mag-stripe CVM Required
#define AMEX_CL_READER_CAPS_EMV_MAGSTRIPE_DEPRECATED            (0x80) ///< Contactless Reader Capabilities: Deprecated - EMV and Mag-stripe
#define AMEX_CL_READER_CAPS_EMV_MAGSTRIPE_NOT_REQUIRED          (0xC0) ///< Contactless Reader Capabilities: EMV and Mag-stripe CVM Not Required
#define AMEX_CL_READER_CAPS_EMV_MAGSTRIPE_REQUIRED              (0xC8) ///< Contactless Reader Capabilities: EMV and Mag-stripe CVM Required

// Visa Form Factor Indicator (field 9F6E) byte 1
// See EMV Contactless Book C-3 v2.10, Annex A.2
// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
#define VISA_FFI_VERSION_MASK                                   (0xE0) ///< Form Factor Indicator (FFI) mask for Version Number
#define VISA_FFI_VERSION_SHIFT                                  (5) ///< Form Factor Indicator (FFI) shift for Version Number
#define VISA_FFI_VERSION_NUMBER_1                               (0x20) ///< Form Factor Indicator (FFI): Version Number 1
#define VISA_FFI_FORM_FACTOR_MASK                               (0x1F) ///< Form Factor Indicator (FFI) mask for Consumer Payment Device Form Factor
#define VISA_FFI_FORM_FACTOR_CARD                               (0x00) ///< Consumer Payment Device Form Factor: Card
#define VISA_FFI_FORM_FACTOR_MINI_CARD                          (0x01) ///< Consumer Payment Device Form Factor: Mini-card
#define VISA_FFI_FORM_FACTOR_NON_CARD                           (0x02) ///< Consumer Payment Device Form Factor: Non-card Form Factor
#define VISA_FFI_FORM_FACTOR_CONSUMER_MOBILE_PHONE              (0x03) ///< Consumer Payment Device Form Factor: Consumer mobile phone
#define VISA_FFI_FORM_FACTOR_WRIST_WORN_DEVICE                  (0x04) ///< Consumer Payment Device Form Factor: Wrist-worn device

// Visa Form Factor Indicator (field 9F6E) byte 2
// See EMV Contactless Book C-3 v2.10, Annex A.2
// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
#define VISA_FFI_FEATURE_PASSCODE                               (0x80) ///< Consumer Payment Device Features: Passcode Capable
#define VISA_FFI_FEATURE_SIGNATURE                              (0x40) ///< Consumer Payment Device Features: Signature Panel
#define VISA_FFI_FEATURE_HOLOGRAM                               (0x20) ///< Consumer Payment Device Features: Hologram
#define VISA_FFI_FEATURE_CVV2                                   (0x10) ///< Consumer Payment Device Features: CVV2
#define VISA_FFI_FEATURE_TWO_WAY_MESSAGING                      (0x08) ///< Consumer Payment Device Features: Two-way Messaging
#define VISA_FFI_FEATURE_CLOUD_CREDENTIALS                      (0x04) ///< Consumer Payment Device Features: Cloud Based Payment Credentials
#define VISA_FFI_FEATURE_BIOMETRIC                              (0x02) ///< Consumer Payment Device Features: Biometric Cardholder Verification Capable
#define VISA_FFI_FEATURE_RFU                                    (0x01) ///< Consumer Payment Device Features: RFU

// Visa Form Factor Indicator (field 9F6E) byte 3
// See EMV Contactless Book C-3 v2.10, Annex A.2
// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
#define VISA_FFI_BYTE3_RFU                                      (0xFF) ///< Form Factor Indicator (FFI) byte 3: RFU

// Visa Form Factor Indicator (field 9F6E) byte 4
// See EMV Contactless Book C-3 v2.10, Annex A.2
// See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
#define VISA_FFI_PAYMENT_TXN_TECHNOLOGY_MASK                    (0x0F) ///< Form Factor Indicator (FFI) mask for Payment Transaction Technology
#define VISA_FFI_PAYMENT_TXN_TECHNOLOGY_CONTACTLESS             (0x00) ///< Payment Transaction Technology: Proximity Contactless interface using ISO 14443 (including NFC)
#define VISA_FFI_PAYMENT_TXN_TECHNOLOGY_NOT_VCPS                (0x01) ///< Payment Transaction Technology: Not used in VCPS
#define VISA_FFI_PAYMENT_TXN_TECHNOLOGY_RFU                     (0xF0) ///< Payment Transaction Technology: RFU

// Amex Enhanced Contactless Reader Capabilities (field 9F6E) byte 1
// See EMV Contactless Book C-4 v2.10, 4.3.4, Table 4-4
#define AMEX_ENH_CL_READER_CAPS_CONTACT_SUPPORTED               (0x80) ///< Enhanced Contactless Reader Capabilities: Contact mode supported
#define AMEX_ENH_CL_READER_CAPS_MAGSTRIPE_MODE_SUPPORTED        (0x40) ///< Enhanced Contactless Reader Capabilities: Contactless Mag-Stripe Mode supported
#define AMEX_ENH_CL_READER_CAPS_FULL_ONLINE_MODE_SUPPORTED      (0x20) ///< Enhanced Contactless Reader Capabilities: Contactless EMV full online mode supported (legacy feature and no longer supported)
#define AMEX_ENH_CL_READER_CAPS_PARTIAL_ONLINE_MODE_SUPPORTED   (0x10) ///< Enhanced Contactless Reader Capabilities: Contactless EMV partial online mode supported
#define AMEX_ENH_CL_READER_CAPS_MOBILE_SUPPORTED                (0x08) ///< Enhanced Contactless Reader Capabilities: Contactless Mobile Supported
#define AMEX_ENH_CL_READER_CAPS_TRY_ANOTHER_INTERFACE           (0x04) ///< Enhanced Contactless Reader Capabilities: Try Another Interface after a decline
#define AMEX_ENH_CL_READER_CAPS_BYTE1_RFU                       (0x03) ///< Enhanced Contactless Reader Capabilities: RFU

// Amex Enhanced Contactless Reader Capabilities (field 9F6E) byte 2
// See EMV Contactless Book C-4 v2.10, 4.3.4, Table 4-4
#define AMEX_ENH_CL_READER_CAPS_MOBILE_CVM_SUPPORTED            (0x80) ///< Enhanced Contactless Reader Capabilities: Mobile CVM supported
#define AMEX_ENH_CL_READER_CAPS_ONLINE_PIN_SUPPORTED            (0x40) ///< Enhanced Contactless Reader Capabilities: Online PIN supported
#define AMEX_ENH_CL_READER_CAPS_SIGNATURE_SUPPORTED             (0x20) ///< Enhanced Contactless Reader Capabilities: Signature supported
#define AMEX_ENH_CL_READER_CAPS_OFFLINE_PIN_SUPPORTED           (0x10) ///< Enhanced Contactless Reader Capabilities: Plaintext Offline PIN supported
#define AMEX_ENH_CL_READER_CAPS_BYTE2_RFU                       (0x0F) ///< Enhanced Contactless Reader Capabilities: RFU

// Amex Enhanced Contactless Reader Capabilities (field 9F6E) byte 3
// See EMV Contactless Book C-4 v2.10, 4.3.4, Table 4-4
#define AMEX_ENH_CL_READER_CAPS_OFFLINE_ONLY_READER             (0x80) ///< Enhanced Contactless Reader Capabilities: Reader is offline only
#define AMEX_ENH_CL_READER_CAPS_CVM_REQUIRED                    (0x40) ///< Enhanced Contactless Reader Capabilities: CVM Required
#define AMEX_ENH_CL_READER_CAPS_BYTE3_RFU                       (0x3F) ///< Enhanced Contactless Reader Capabilities: RFU

// Amex Enhanced Contactless Reader Capabilities (field 9F6E) byte 4
// See EMV Contactless Book C-4 v2.10, 4.3.4, Table 4-4
#define AMEX_ENH_CL_READER_CAPS_EXEMPT_FROM_NO_CVM              (0x80) ///< Enhanced Contactless Reader Capabilities: Terminal exempt from No CVM checks
#define AMEX_ENH_CL_READER_CAPS_DELAYED_AUTHORISATION           (0x40) ///< Enhanced Contactless Reader Capabilities: Delayed Authorisation Terminal
#define AMEX_ENH_CL_READER_CAPS_TRANSIT                         (0x20) ///< Enhanced Contactless Reader Capabilities: Transit Terminal
#define AMEX_ENH_CL_READER_CAPS_BYTE4_RFU                       (0x18) ///< Enhanced Contactless Reader Capabilities: RFU
#define AMEX_ENH_CL_READER_CAPS_KERNEL_VERSION_MASK             (0x07) ///< Enhanced Contactless Reader Capabilities mask for C-4 kernel version
#define AMEX_ENH_CL_READER_CAPS_KERNEL_VERSION_22_23            (0x01) ///< Enhanced Contactless Reader Capabilities: C-4 kernel version 2.2 - 2.3
#define AMEX_ENH_CL_READER_CAPS_KERNEL_VERSION_24_26            (0x02) ///< Enhanced Contactless Reader Capabilities: C-4 kernel version 2.4 - 2.6
#define AMEX_ENH_CL_READER_CAPS_KERNEL_VERSION_27               (0x03) ///< Enhanced Contactless Reader Capabilities: C-4 kernel version 2.7

/// EMV card schemes
enum emv_card_scheme_t {
	EMV_CARD_SCHEME_UNKNOWN = 0, ///< Unknown card scheme
	EMV_CARD_SCHEME_VISA, ///< Visa
	EMV_CARD_SCHEME_MASTERCARD, ///< Mastercard
	EMV_CARD_SCHEME_AMEX, ///< American Express
	EMV_CARD_SCHEME_DISCOVER, ///< Discover
	EMV_CARD_SCHEME_CB, ///< Cartes Bancaires (CB)
	EMV_CARD_SCHEME_JCB, ///< JCB
	EMV_CARD_SCHEME_DANKORT, ///< Dankort
	EMV_CARD_SCHEME_UNIONPAY, ///< UnionPay
	EMV_CARD_SCHEME_GIMUEMOA, ///< GIM-UEMOA
	EMV_CARD_SCHEME_DK, ///< Deutsche Kreditwirtschaft (DK)
	EMV_CARD_SCHEME_VERVE, ///< Verve
	EMV_CARD_SCHEME_EFTPOS, ///< eftpos (Australia)
	EMV_CARD_SCHEME_RUPAY, ///< RuPay
	EMV_CARD_SCHEME_MIR, ///< Mir
	EMV_CARD_SCHEME_MEEZA, ///< Meeza
};

/// EMV card products
enum emv_card_product_t {
	EMV_CARD_PRODUCT_UNKNOWN = 0, ///< Unknown card product

	// Visa
	EMV_CARD_PRODUCT_VISA_CREDIT_DEBIT, ///< Visa Credit/Debit
	EMV_CARD_PRODUCT_VISA_ELECTRON, ///< Visa Electron
	EMV_CARD_PRODUCT_VISA_VPAY, ///< V Pay
	EMV_CARD_PRODUCT_VISA_PLUS, ///< Visa Plus
	EMV_CARD_PRODUCT_VISA_USA_DEBIT, ///< Visa USA Debit

	// Mastercard
	EMV_CARD_PRODUCT_MASTERCARD_CREDIT_DEBIT, ///< Mastercard Credit/Debit
	EMV_CARD_PRODUCT_MASTERCARD_MAESTRO, ///< Maestro
	EMV_CARD_PRODUCT_MASTERCARD_CIRRUS, ///< Mastercard Cirrus
	EMV_CARD_PRODUCT_MASTERCARD_USA_DEBIT, ///< Mastercard USA Debit
	EMV_CARD_PRODUCT_MASTERCARD_MAESTRO_UK, ///< Maestro UK
	EMV_CARD_PRODUCT_MASTERCARD_TEST, ///< Mastercard Test Card

	// American Express
	EMV_CARD_PRODUCT_AMEX_CREDIT_DEBIT, ///< American Express Credit/Debit
	EMV_CARD_PRODUCT_AMEX_CHINA_CREDIT_DEBIT, ///< American Express (China Credit/Debit)

	// Discover
	EMV_CARD_PRODUCT_DISCOVER_CARD, ///< Discover Card
	EMV_CARD_PRODUCT_DISCOVER_USA_DEBIT, ///< Discover USA Debit
	EMV_CARD_PRODUCT_DISCOVER_ZIP, ///< Discover ZIP

	// Cartes Bancaires (CB)
	EMV_CARD_PRODUCT_CB_CREDIT_DEBIT, ///< Cartes Bancaires (CB) Credit/Debit
	EMV_CARD_PRODUCT_CB_DEBIT, ///< Cartes Bancaires (CB) Debit

	// Dankort
	EMV_CARD_PRODUCT_DANKORT_VISADANKORT, ///< Visa/Dankort
	EMV_CARD_PRODUCT_DANKORT_JSPEEDY, ///< Dankort (J/Speedy)

	// UnionPay
	EMV_CARD_PRODUCT_UNIONPAY_DEBIT, ///< UnionPay Debit
	EMV_CARD_PRODUCT_UNIONPAY_CREDIT, ///< UnionPay Credit
	EMV_CARD_PRODUCT_UNIONPAY_QUASI_CREDIT, ///< UnionPay Quasi-credit
	EMV_CARD_PRODUCT_UNIONPAY_ELECTRONIC_CASH, ///< UnionPay Electronic Cash
	EMV_CARD_PRODUCT_UNIONPAY_USA_DEBIT, ///< UnionPay USA Debit

	// GIM-UEMOA
	EMV_CARD_PRODUCT_GIMUEMOA_STANDARD, ///< GIM-UEMOA Standard
	EMV_CARD_PRODUCT_GIMUEMOA_PREPAID_ONLINE, ///< GIM-UEMOA Prepaye Online
	EMV_CARD_PRODUCT_GIMUEMOA_CLASSIC, ///< GIM-UEMOA Classic
	EMV_CARD_PRODUCT_GIMUEMOA_PREPAID_OFFLINE, ///< GIM-UEMOA Prepaye Possibile Offline
	EMV_CARD_PRODUCT_GIMUEMOA_RETRAIT, ///< GIM-UEMOA Retrait
	EMV_CARD_PRODUCT_GIMUEMOA_ELECTRONIC_WALLET, ///< GIM-UEMOA Porte Monnaie Electronique

	// Deutsche Kreditwirtschaft
	EMV_CARD_PRODUCT_DK_GIROCARD, ///< Deutsche Kreditwirtschaft (DK) Girocard

	// eftpos (Australia)
	EMV_CARD_PRODUCT_EFTPOS_SAVINGS, ///< eftpos (Australia) savings
	EMV_CARD_PRODUCT_EFTPOS_CHEQUE, ///< eftpos (Australia) cheque

	// Mir
	EMV_CARD_PRODUCT_MIR_CREDIT, ///< Mir Credit
	EMV_CARD_PRODUCT_MIR_DEBIT, ///< Mir Debit
};

/// EMV Application Identifier (AID) information
struct emv_aid_info_t {
	enum emv_card_scheme_t scheme; ///< EMV card scheme associated with Application Identifier (AID)
	enum emv_card_product_t product; ///< EMV card product associated with Application Identifier (AID)
};

/**
 * Determine EMV card scheme and card product from Application Identifier (AID)
 * (field 4F, 84, or 9F06)
 * @param aid Application Identifier (AID) field. Must be 5 to 16 bytes.
 * @param aid_len Length of Application Identifier (AID) field. Must be 5 to 16 bytes.
 * @param info EMV card scheme and card product output. See @ref emv_aid_info_t.
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_aid_get_info(
	const uint8_t* aid,
	size_t aid_len,
	struct emv_aid_info_t* info
);

/// Application File Locator (AFL) iterator
struct emv_afl_itr_t {
	const void* ptr;
	size_t len;
};

/// Application File Locator (AFL) entry
struct emv_afl_entry_t {
	uint8_t sfi; ///< Short File Identifier (SFI) for AFL entry
	uint8_t first_record; ///< First record in SFI for AFL entry
	uint8_t last_record; ///< Last record in SFI for AFL entry
	uint8_t oda_record_count; ///< Number of records (starting at @ref first_record) involved in offline data authentication
};

/**
 * Initialise Application File Locator (AFL) iterator
 * @param afl Application File Locator (AFL) field. Must be multiples of 4 bytes.
 * @param afl_len Length of Application File Locator (AFL) field. Must be multiples of 4 bytes.
 * @param itr Application File Locator (AFL) iterator output
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_afl_itr_init(const void* afl, size_t afl_len, struct emv_afl_itr_t* itr);

/**
 * Decode next entry and advance iterator
 * @param itr Application File Locator (AFL) iterator
 * @param entry Decoded Application File Locator (AFL) entry output
 * @return Number of bytes consumed. Zero for end of data. Less than zero for error.
 */
int emv_afl_itr_next(struct emv_afl_itr_t* itr, struct emv_afl_entry_t* entry);

/// Cardholder Verification Method (CVM) List iterator
struct emv_cvmlist_itr_t {
	const void* ptr;
	size_t len;
};

/// Cardholder Verification Method (CVM) List amounts
struct emv_cvmlist_amounts_t {
	uint32_t X; ///< First amount field, referred to as "X" in EMV 4.4 Book 3, Annex C3, Table 44
	uint32_t Y; ///< Second amount field, referred to as "Y" in EMV 4.4 Book 3, Annex C3, Table 44
};

/// Cardholder Verification (CV) Rule
/// @remark See EMV 4.4 Book 3, Annex C3
struct emv_cv_rule_t {
	uint8_t cvm; ///< Cardholder Verification Method (CVM) Code
	uint8_t cvm_cond; ///< Cardholder Verification Method (CVM) Condition
};

/**
 * Initialize Cardholder Verification Method (CVM) List amounts and iterator
 * @param cvmlist Cardholder Verification Method (CVM) List field
 * @param cvmlist_len Length of Cardholder Verification Method (CVM) List field
 * @param amounts Cardholder Verification Method (CVM) List amounts output
 * @param itr Cardholder Verification Method (CVM) List iterator output
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int emv_cvmlist_itr_init(
	const void* cvmlist,
	size_t cvmlist_len,
	struct emv_cvmlist_amounts_t* amounts,
	struct emv_cvmlist_itr_t* itr
);

/**
 * Decode next Cardholder Verification (CV) Rule and advance iterator
 * @param itr Cardholder Verification Method (CVM) List iterator
 * @param rule Decoded Cardholder Verification (CV) Rule output
 * @return Number of bytes consumed. Zero for end of data. Less than zero for error.
 */
int emv_cvmlist_itr_next(struct emv_cvmlist_itr_t* itr, struct emv_cv_rule_t* rule);

/**
 * Determine Issuer Application Data (field 9F10) format
 * @param iad Issuer Application Data (IAD) field. Must be 1 to 32 bytes.
 * @param iad_len Length of Issuer Application Data (IAD) field. Must be 1 to 32 bytes.
 * @return Issuer Application Data (IAD) format. See @ref emv_iad_format_t
 */
enum emv_iad_format_t emv_iad_get_format(const uint8_t* iad, size_t iad_len);

__END_DECLS

#endif
