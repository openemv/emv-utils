/**
 * @file emv_tags.h
 * @brief EMV tag definitions
 * @remark See EMV 4.3 Book 1, Annex B
 * @remark See EMV 4.3 Book 3, Annex A
 * @remark See ISO 7816-4:2005, 5.2.4
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

#ifndef EMV_TAGS_H
#define EMV_TAGS_H

#include <sys/cdefs.h>

__BEGIN_DECLS

/// EMV tag 42 Issuer Identification Number (IIN). Template BF0C or 73.
#define EMV_TAG_42_IIN                                          (0x42)

/// EMV tag 4F Application Dedicated File (ADF) Name. Template 61.
#define EMV_TAG_4F_APPLICATION_DF_NAME                          (0x4F)

/// EMV tag 50 Application Label. Template 61 or A5.
#define EMV_TAG_50_APPLICATION_LABEL                            (0x50)

/// EMV tag 61 Application Template. Template 70.
/// @remark See ISO 7816-4:2005, 8.2.1.3
#define EMV_TAG_61_APPLICATION_TEMPLATE                         (0x61)

/// EMV tag 6F File Control Information (FCI) Template
#define EMV_TAG_6F_FCI_TEMPLATE                                 (0x6F)

/// EMV tag 70 Data Template
/// @remark Used as READ RECORD Response Message Template
/// @remark Used as Application Elementary File (AEF) Data Template
/// @remark Used as generic EMV data template
#define EMV_TAG_70_DATA_TEMPLATE                                (0x70)

/// EMV tag 73 Directory Discretionary Template. Template 61.
#define EMV_TAG_73_DIRECTORY_DISCRETIONARY_TEMPLATE             (0x73)

/// EMV tag 84 Dedicated File (DF) Name. Template 6F.
#define EMV_TAG_84_DF_NAME                                      (0x84)

/// EMV tag 87 Application Priority Indicator. Template 61 or A5.
#define EMV_TAG_87_APPLICATION_PRIORITY_INDICATOR               (0x87)

/// EMV tag 88 Short File Indicator (SFI). Template A5.
#define EMV_TAG_88_SFI                                          (0x88)

/// EMV tag 9D Directory Definition File (DDF) Name. Template 6F.
#define EMV_TAG_9D_DDF_NAME                                     (0x9D)

/// EMV tag A5 File Control Information (FCI) Proprietary Template. Template 6F.
#define EMV_TAG_A5_FCI_PROPRIETARY_TEMPLATE                     (0xA5)

/// EMV tag 5F2D Language Preference. Template A5.
#define EMV_TAG_5F2D_LANGUAGE_PREFERENCE                        (0x5F2D)

/// EMV tag 5F50 Issuer URL. Template BF0C or 73.
#define EMV_TAG_5F50_ISSUER_URL                                 (0x5F50)

/// EMV tag 5F53 International Bank Account Number (IBAN). Template BF0C or 73.
#define EMV_TAG_5F53_IBAN                                       (0x5F53)

/// EMV tag 5F54 Bank Identifier Code (BIC). Template BF0C or 73.
#define EMV_TAG_5F54_BANK_IDENTIFIER_CODE                       (0x5F54)

/// EMV tag 5F55 Issuer Country Code (alpha2 format). Template BF0C or 73.
#define EMV_TAG_5F55_ISSUER_COUNTRY_CODE_ALPHA2                 (0x5F55)

/// EMV tag 5F56 Issuer Country Code (alpha3 format). Template BF0C or 73.
#define EMV_TAG_5F56_ISSUER_COUNTRY_CODE_ALPHA3                 (0x5F56)

/// EMV tag 9F06 Application Identifier (AID) - terminal
#define EMV_TAG_9F06_AID                                        (0x9F06)

/// EMV tag 9F11 Issuer Code Table Index. Template A5.
#define EMV_TAG_9F11_ISSUER_CODE_TABLE_INDEX                    (0x9F11)

/// EMV tag 9F12 Application Preferred Name. Template 61 or A5.
#define EMV_TAG_9F12_APPLICATION_PREFERRED_NAME                 (0x9F12)

/// EMV tag 9F1A Terminal Country Code
#define EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE                      (0x9F1A)

/// EMV tag 9F1B Terminal Floor Limit
#define EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT                       (0x9F1B)

/// EMV tag 9F1C Terminal Identification
#define EMV_TAG_9F1C_TERMINAL_IDENTIFICATION                    (0x9F1C)

/// EMV tag 9F1E Interface Device (IFD) Serial Number
#define EMV_TAG_9F1E_IFD_SERIAL_NUMBER                          (0x9F1E)

/// EMV tag 9F33 Terminal Capabilities
#define EMV_TAG_9F33_TERMINAL_CAPABILITIES                      (0x9F33)

/// EMV tag 9F35 Terminal Type
#define EMV_TAG_9F35_TERMINAL_TYPE                              (0x9F35)

/// EMV tag 9F40 Additional Terminal Capabilities
#define EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES           (0x9F40)

/// EMV tag 9F38 Processing Options Data Object List (PDOL). Template A5.
#define EMV_TAG_9F38_PDOL                                       (0x9F38)

/// EMMV tag 9F4D Log Entry. Template BF0C or 73.
#define EMV_TAG_9F4D_LOG_ENTRY                                  (0x9F4D)

/// EMV tag BF0C File Control Information (FCI) Issuer Discretionary Data. Template A5.
#define EMV_TAG_BF0C_FCI_ISSUER_DISCRETIONARY_DATA              (0xBF0C)

__END_DECLS

#endif
