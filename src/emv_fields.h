/**
 * @file emv_fields.h
 * @brief EMV field definitions
 *
 * Copyright (c) 2021 Leon Lynch
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

__BEGIN_DECLS

// Application Priority Indicator
// See EMV 4.3 Book 1, 12.2.3, table 48
#define EMV_APP_PRIORITY_INDICATOR_MASK                         (0x0F)
#define EMV_APP_PRIORITY_INDICATOR_CONF_REQUIRED                (0x80)

// Application Selection Indicator
// See EMV 4.3 Book 1, 12.3.1
#define EMV_ASI_EXACT_MATCH                                     (0x00)
#define EMV_ASI_PARTIAL_MATCH                                   (0x01)

// Transaction Type (field 9C)
// See ISO 8583:1987
#define EMV_TRANSACTION_TYPE_GOODS_AND_SERVICES                 (0x00) ///< Transaction Type: Goods and services
#define EMV_TRANSACTION_TYPE_CASH                               (0x01) ///< Transaction Type: Cash
#define EMV_TRANSACTION_TYPE_CASHBACK                           (0x09) ///< Transaction Type: Cashback
#define EMV_TRANSACTION_TYPE_REFUND                             (0x20) ///< Transaction Type: Refund
#define EMV_TRANSACTION_TYPE_INQUIRY                            (0x30) ///< Transaction Type: Inquiry

// Terminal Type (field 9F35)
// See EMV 4.3 Book 4, Annex A1, table 24
#define EMV_TERM_TYPE_OPERATIONAL_CONTROL_MASK                  (0xF0) ///< Terminal Type mask for operational control
#define EMV_TERM_TYPE_OPERATIONAL_CONTROL_FINANCIAL_INSTITUTION (0x10) ///< Operational Control: Financial Institution
#define EMV_TERM_TYPE_OPERATIONAL_CONTROL_MERCHANT              (0x20) ///< Operationsl Control: Merchant
#define EMV_TERM_TYPE_OPERATIONAL_CONTROL_CARDHOLDER            (0x30) ///< Operationsl Control: Cardholder
#define EMV_TERM_TYPE_ENV_MASK                                  (0x0F) ///< Terminal Type mask for terminal environment
#define EMV_TERM_TYPE_ENV_ATTENDED_ONLINE_ONLY                  (0x01) ///< Environment: Attended, online only
#define EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_WITH_ONLINE          (0x02) ///< Environment: Attended, offline with online capability
#define EMV_TERM_TYPE_ENV_ATTENDED_OFFLINE_ONLY                 (0x03) ///< Environment: Attended, offline only
#define EMV_TERM_TYPE_ENV_UNATTENDED_ONLINE_ONLY                (0x04) ///< Environment: Unattended, online only
#define EMV_TERM_TYPE_ENV_UNATTENDED_OFFLINE_WITH_ONLINE        (0x05) ///< Environment: Unattended, offline with online capability
#define EMV_TERM_TYPE_ENV_UNATTENDED_OFFLINE_ONLY               (0x06) ///< Environment: Unattended, offline only

// Terminal Capabilities (field 9F33) byte 1
// See EMV 4.3 Book 4, Annex A2, table 25
#define EMV_TERM_CAPS_INPUT_MANUAL_KEY_ENTRY                    (0x80) ///< Card Data Input Capability: Manual key entry
#define EMV_TERM_CAPS_INPUT_MAGNETIC_STRIPE                     (0x40) ///< Card Data Input Capability: Magnetic stripe
#define EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS                    (0x20) ///< Card Data Input Capability: IC with contacts
#define EMV_TERM_CAPS_INPUT_RFU                                 (0x1F) ///< Card Data Input Capability: RFU

// Terminal Capabilities (field 9F33) byte 2
// See EMV 4.3 Book 4, Annex A2, table 26
#define EMV_TERM_CAPS_CVM_PLAINTEXT_PIN_OFFLINE                 (0x80) ///< CVM Capability: Plaintext PIN for ICC verification
#define EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE                 (0x40) ///< CVM Capability: Enciphered PIN for online verification
#define EMV_TERM_CAPS_CVM_SIGNATURE                             (0x20) ///< CVM Capability: Signature (paper)
#define EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_OFFLINE                (0x10) ///< CVM Capability: Enciphered PIN for offline verification
#define EMV_TERM_CAPS_CVM_NO_CVM                                (0x08) ///< CVM Capability: No CVM required
#define EMV_TERM_CAPS_CVM_RFU                                   (0x07) ///< CVM Capability: RFU

// Terminal Capabilities (field 9F33) byte 3
// See EMV 4.3 Book 4, Annex A2, table 27
#define EMV_TERM_CAPS_SECURITY_SDA                              (0x80) ///< Security Capability: Static Data Authentication (SDA)
#define EMV_TERM_CAPS_SECURITY_DDA                              (0x40) ///< Security Capability: Dynamic Data Authentication (DDA)
#define EMV_TERM_CAPS_SECURITY_CARD_CAPTURE                     (0x20) ///< Security Capability: Card capture
#define EMV_TERM_CAPS_SECURITY_CDA                              (0x08) ///< Security Capability: Combined DDA/Application Cryptogram Generation (CDA)
#define EMV_TERM_CAPS_SECURITY_RFU                              (0x17) ///< Security Capability: RFU

// Additional Terminal Capabilities (field 9F40) byte 1
// See EMV 4.3 Book 4, Annex A3, table 28
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH                        (0x80) ///< Transaction Type Capability: Cash
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_GOODS                       (0x40) ///< Transaction Type Capability: Goods
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_SERVICES                    (0x20) ///< Transaction Type Capability: Services
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_CASHBACK                    (0x10) ///< Transaction Type Capability: Cashback
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_INQUIRY                     (0x08) ///< Transaction Type Capability: Inquiry
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_TRANSFER                    (0x04) ///< Transaction Type Capability: Transfer
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_PAYMENT                     (0x02) ///< Transaction Type Capability: Payment
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_ADMINISTRATIVE              (0x01) ///< Transaction Type Capability: Administrative

// Additional Terminal Capabilities (field 9F40) byte 2
// See EMV 4.3 Book 4, Annex A3, table 29
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_CASH_DEPOSIT                (0x80) ///< Transaction Type Capability: Cash deposit
#define EMV_ADDL_TERM_CAPS_TXN_TYPE_RFU                         (0x7F) ///< Transaction Type Capability: RFU

// Additional Terminal Capabilities (field 9F40) byte 3
// See EMV 4.3 Book 4, Annex A3, table 30
#define EMV_ADDL_TERM_CAPS_INPUT_NUMERIC_KEYS                   (0x80) ///< Terminal Data Input Capability: Numeric keys
#define EMV_ADDL_TERM_CAPS_INPUT_ALPHABETIC_AND_SPECIAL_KEYS    (0x40) ///< Terminal Data Input Capability: Alphabetic and special character keys
#define EMV_ADDL_TERM_CAPS_INPUT_COMMAND_KEYS                   (0x20) ///< Terminal Data Input Capability: Command keys
#define EMV_ADDL_TERM_CAPS_INPUT_FUNCTION_KEYS                  (0x10) ///< Terminal Data Input Capability: Function keys
#define EMV_ADDL_TERM_CAPS_INPUT_RFU                            (0x0F) ///< Terminal Data Input Capability: RFU

// Additional Terminal Capabilities (field 9F40) byte 4
// See EMV 4.3 Book 4, Annex A3, table 31
#define EMV_ADDL_TERM_CAPS_OUTPUT_PRINT_ATTENDANT               (0x80) ///< Terminal Data Output Capability: Print, attendant
#define EMV_ADDL_TERM_CAPS_OUTPUT_PRINT_CARDHOLDER              (0x40) ///< Terminal Data Output Capability: Print, cardholder
#define EMV_ADDL_TERM_CAPS_OUTPUT_DISPLAY_ATTENDANT             (0x20) ///< Terminal Data Output Capability: Display, attendant
#define EMV_ADDL_TERM_CAPS_OUTPUT_DISPLAY_CARDHOLDER            (0x10) ///< Terminal Data Output Capability: Display cardholder
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_10                 (0x02) ///< Terminal Data Output Capability: Code table 10
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_9                  (0x01) ///< Terminal Data Output Capability: Code table 9
#define EMV_ADDL_TERM_CAPS_OUTPUT_RFU                           (0x0C) ///< Terminal Data Output Capability: RFU

// Additional Terminal Capabilities (field 9F40) byte 5
// See EMV 4.3 Book 4, Annex A3, table 32
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_8                  (0x80) ///< Terminal Data Output Capability: Code table 8
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_7                  (0x40) ///< Terminal Data Output Capability: Code table 7
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_6                  (0x20) ///< Terminal Data Output Capability: Code table 6
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_5                  (0x10) ///< Terminal Data Output Capability: Code table 5
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_4                  (0x08) ///< Terminal Data Output Capability: Code table 4
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_3                  (0x04) ///< Terminal Data Output Capability: Code table 3
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_2                  (0x02) ///< Terminal Data Output Capability: Code table 2
#define EMV_ADDL_TERM_CAPS_OUTPUT_CODE_TABLE_1                  (0x01) ///< Terminal Data Output Capability: Code table 1

__END_DECLS

#endif
