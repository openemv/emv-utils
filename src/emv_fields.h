/**
 * @file emv_fields.h
 * @brief EMV field definitions and helper functions
 *
 * Copyright (c) 2021, 2022 Leon Lynch
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
// See ISO 8583:2003
#define EMV_POS_ENTRY_MODE_UNKNOWN                              (0x00) ///< POS Entry Mode: Unknown
#define EMV_POS_ENTRY_MODE_MANUAL                               (0x01) ///< POS Entry Mode: Manual PAN entry
#define EMV_POS_ENTRY_MODE_MAG                                  (0x02) ///< POS Entry Mode: Magnetic stripe
#define EMV_POS_ENTRY_MODE_BARCODE                              (0x03) ///< POS Entry Mode: Barcode
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
 * Initialize Application File Locator (AFL) iterator
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

__END_DECLS

#endif
