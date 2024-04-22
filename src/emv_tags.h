/**
 * @file emv_tags.h
 * @brief EMV tag definitions
 * @remark See EMV 4.4 Book 1, Annex B
 * @remark See EMV 4.4 Book 3, Annex A
 * @remark See ISO 7816-4:2005, 5.2.4
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

/// EMV tag 56 Track 1 Data. Template 70.
/// @remark See EMV Contactless Book C-2 v2.10, Annex A.1.176
#define EMV_TAG_56_TRACK1_DATA                                  (0x56)

/// EMV tag 57 Track 2 Equivalent Data. Template 70 or 77.
#define EMV_TAG_57_TRACK2_EQUIVALENT_DATA                       (0x57)

/// EMV tag 5A Application Primary Account Number (PAN). Template 70 or 77.
#define EMV_TAG_5A_APPLICATION_PAN                              (0x5A)

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

/// EMV tag 71 Issuer Script Template 1
#define EMV_TAG_71_ISSUER_SCRIPT_TEMPLATE_1                     (0x71)

/// EMV tag 72 Issuer Script Template 2
#define EMV_TAG_72_ISSUER_SCRIPT_TEMPLATE_2                     (0x72)

/// EMV tag 73 Directory Discretionary Template. Template 61.
#define EMV_TAG_73_DIRECTORY_DISCRETIONARY_TEMPLATE             (0x73)

/// EMV tag 77 Response Message Template Format 2
/// @remark Used as GET PROCESSING OPTIONS Response Message Template
/// @remark Used as INTERNAL AUTHENTICATE Response Message Template
/// @remark Used as GENERATE AC Response Message Template
#define EMV_TAG_77_RESPONSE_MESSAGE_TEMPLATE_FORMAT_2           (0x77)

/// EMV tag 80 Response Message Template Format 1
/// @remark Used as GET PROCESSING OPTIONS Response Message Template
/// @remark Used as INTERNAL AUTHENTICATE Response Message Template
/// @remark Used as GENERATE AC Response Message Template
#define EMV_TAG_80_RESPONSE_MESSAGE_TEMPLATE_FORMAT_1           (0x80)

/// EMV tag 81 Amount, Authorised (Binary)
#define EMV_TAG_81_AMOUNT_AUTHORISED_BINARY                     (0x81)

/// EMV tag 82 Application Interchange Profile (AIP). Template 77 or 80.
#define EMV_TAG_82_APPLICATION_INTERCHANGE_PROFILE              (0x82)

/// EMV tag 83 Command Template
#define EMV_TAG_83_COMMAND_TEMPLATE                             (0x83)

/// EMV tag 84 Dedicated File (DF) Name. Template 6F.
#define EMV_TAG_84_DF_NAME                                      (0x84)

/// EMV tag 86 Issuer Script Command. Template 71 or 72.
#define EMV_TAG_86_ISSUER_SCRIPT_COMMAND                        (0x86)

/// EMV tag 87 Application Priority Indicator. Template 61 or A5.
#define EMV_TAG_87_APPLICATION_PRIORITY_INDICATOR               (0x87)

/// EMV tag 88 Short File Identifier (SFI). Template A5.
#define EMV_TAG_88_SFI                                          (0x88)

/// EMV tag 89 Authorisation Code
/// @remark See EMV Contactless Book C-3 v2.11, Annex A.2
/// @remark See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
/// @remark See M/Chip Requirements for Contact and Contactless, 28 November 2023, Appendix A
#define EMV_TAG_89_AUTHORISATION_CODE                           (0x89)

/// EMV tag 8A Authorisation Response Code
/// @remark See ISO 8583:2021, J.2.2.2
#define EMV_TAG_8A_AUTHORISATION_RESPONSE_CODE                  (0x8A)

/// EMV tag 8C Card Risk Management Data Object List 1 (CDOL1). Template 70 or 77.
#define EMV_TAG_8C_CDOL1                                        (0x8C)

/// EMV tag 8D Card Risk Management Data Object List 2 (CDOL2). Template 70 or 77.
#define EMV_TAG_8D_CDOL2                                        (0x8D)

/// EMV tag 8E Cardholder Verification Method (CVM) List. Template 70 or 77.
#define EMV_TAG_8E_CVM_LIST                                     (0x8E)

/// EMV tag 8F Certification Authority Public Key (CAPK) Index. Template 70 or 77.
#define EMV_TAG_8F_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX     (0x8F)

/// EMV tag 90 Issuer Public Key Certificate. Template 70 or 77.
#define EMV_TAG_90_ISSUER_PUBLIC_KEY_CERTIFICATE                (0x90)

/// EMV tag 91 Issuer Authentication Data
#define EMV_TAG_91_ISSUER_AUTHENTICATION_DATA                   (0x91)

/// EMV tag 92 Issuer Public Key Remainder. Template 70 or 77.
#define EMV_TAG_92_ISSUER_PUBLIC_KEY_REMAINDER                  (0x92)

/// EMV tag 93 Signed Static Application Data. Template 70 or 77.
#define EMV_TAG_93_SIGNED_STATIC_APPLICATION_DATA               (0x93)

/// EMV tag 94 Application File Locator (AFL). Template 77 or 80.
#define EMV_TAG_94_APPLICATION_FILE_LOCATOR                     (0x94)

/// EMV tag 95 Terminal Verification Results (TVR)
#define EMV_TAG_95_TERMINAL_VERIFICATION_RESULTS                (0x95)

/// EMV tag 9A Transaction Date
#define EMV_TAG_9A_TRANSACTION_DATE                             (0x9A)

/// EMV tag 9B Transaction Status Information (TSI)
#define EMV_TAG_9B_TRANSACTION_STATUS_INFORMATION               (0x9B)

/// EMV tag 9C Transaction Type
#define EMV_TAG_9C_TRANSACTION_TYPE                             (0x9C)

/// EMV tag 9D Directory Definition File (DDF) Name. Template 6F.
#define EMV_TAG_9D_DDF_NAME                                     (0x9D)

/// EMV tag A5 File Control Information (FCI) Proprietary Template. Template 6F.
#define EMV_TAG_A5_FCI_PROPRIETARY_TEMPLATE                     (0xA5)

/// EMV tag 5F20 Cardholder Name. Template 70 or 77.
#define EMV_TAG_5F20_CARDHOLDER_NAME                            (0x5F20)

/// EMV tag 5F24 Application Expiration Date. Template 70 or 77.
#define EMV_TAG_5F24_APPLICATION_EXPIRATION_DATE                (0x5F24)

/// EMV tag 5F25 Application Effective Date. Template 70 or 77.
#define EMV_TAG_5F25_APPLICATION_EFFECTIVE_DATE                 (0x5F25)

/// EMV tag 5F28 Issuer Country Code. Template 70 or 77.
#define EMV_TAG_5F28_ISSUER_COUNTRY_CODE                        (0x5F28)

/// EMV tag 5F2A Transaction Currency Code
#define EMV_TAG_5F2A_TRANSACTION_CURRENCY_CODE                  (0x5F2A)

/// EMV tag 5F2D Language Preference. Template A5.
#define EMV_TAG_5F2D_LANGUAGE_PREFERENCE                        (0x5F2D)

/// EMV tag 5F30 Service Code. Template 70 or 77.
#define EMV_TAG_5F30_SERVICE_CODE                               (0x5F30)

/// EMV tag 5F34 Application Primary Account Number (PAN) Sequence Number. Template 70 or 77.
#define EMV_TAG_5F34_APPLICATION_PAN_SEQUENCE_NUMBER            (0x5F34)

/// EMV tag 5F36 Transaction Currency Exponent
#define EMV_TAG_5F36_TRANSACTION_CURRENCY_EXPONENT              (0x5F36)

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

/// EMV tag 5F57 Account Type
#define EMV_TAG_5F57_ACCOUNT_TYPE                               (0x5F57)

/// EMV tag 9F01 Acquirer Identifier
#define EMV_TAG_9F01_ACQUIRER_IDENTIFIER                        (0x9F01)

/// EMV tag 9F02 Amount, Authorised (Numeric)
#define EMV_TAG_9F02_AMOUNT_AUTHORISED_NUMERIC                  (0x9F02)

/// EMV tag 9F03 Amount, Other (Numeric)
#define EMV_TAG_9F03_AMOUNT_OTHER_NUMERIC                       (0x9F03)

/// EMV tag 9F04 Amount, Other (Binary)
#define EMV_TAG_9F04_AMOUNT_OTHER_BINARY                        (0x9F04)

/// EMV tag 9F05 Application Discretionary Data. Template 70 or 77.
#define EMV_TAG_9F05_APPLICATION_DISCRETIONARY_DATA             (0x9F05)

/// EMV tag 9F06 Application Identifier (AID) - terminal
#define EMV_TAG_9F06_AID                                        (0x9F06)

/// EMV tag 9F07 Application Usage Control. Template 70 or 77.
#define EMV_TAG_9F07_APPLICATION_USAGE_CONTROL                  (0x9F07)

/// EMV tag 9F08 Application Version Number. Template 70 or 77.
#define EMV_TAG_9F08_APPLICATION_VERSION_NUMBER                 (0x9F08)

/// EMV tag 9F09 Application Version Number - terminal
#define EMV_TAG_9F09_APPLICATION_VERSION_NUMBER_TERMINAL        (0x9F09)

/// EMV tag 9F0A Application Selection Registered Proprietary Data (ASRPD)
#define EMV_TAG_9F0A_ASRPD                                      (0x9F0A)

/// EMV tag 9F0B Cardholder Name Extended. Template 70 or 77.
#define EMV_TAG_9F0B_CARDHOLDER_NAME_EXTENDED                   (0x9F0B)

/// EMV tag 9F0C Issuer Identification Number Extended (IINE). Template BF0C or 73.
#define EMV_TAG_9F0C_IINE                                       (0x9F0C)

/// EMV tag 9F0D Issuer Action Code (IAC) - Default. Template 70 or 77.
#define EMV_TAG_9F0D_ISSUER_ACTION_CODE_DEFAULT                 (0x9F0D)

/// EMV tag 9F0E Issuer Action Code (IAC) - Denial. Template 70 or 77.
#define EMV_TAG_9F0E_ISSUER_ACTION_CODE_DENIAL                  (0x9F0E)

/// EMV tag 9F0F Issuer Action Code (IAC) - Online. Template 70 or 77.
#define EMV_TAG_9F0F_ISSUER_ACTION_CODE_ONLINE                  (0x9F0F)

/// EMV tag 9F10 Issuer Application Data (IAD). Template 77 or 80.
#define EMV_TAG_9F10_ISSUER_APPLICATION_DATA                    (0x9F10)

/// EMV tag 9F11 Issuer Code Table Index. Template A5.
#define EMV_TAG_9F11_ISSUER_CODE_TABLE_INDEX                    (0x9F11)

/// EMV tag 9F12 Application Preferred Name. Template 61 or A5.
#define EMV_TAG_9F12_APPLICATION_PREFERRED_NAME                 (0x9F12)

/// EMV tag 9F13 Last Online Application Transaction Counter (ATC) Register
#define EMV_TAG_9F13_LAST_ONLINE_ATC_REGISTER                   (0x9F13)

/// EMV tag 9F14 Lower Consecutive Offline Limit. Template 70 or 77.
#define EMV_TAG_9F14_LOWER_CONSECUTIVE_OFFLINE_LIMIT            (0x9F14)

/// EMV tag 9F15 Merchant Category Code (MCC)
#define EMV_TAG_9F15_MCC                                        (0x9F15)

/// EMV tag 9F16 Merchant Identifier
/// @remark Also used as the Card Acceptor Identification by ISO 8583. See EMV 4.4 Book 4, Annex C, Table 40
#define EMV_TAG_9F16_MERCHANT_IDENTIFIER                        (0x9F16)

/// EMV tag 9F17 Personal Identification Number (PIN) Try Counter
#define EMV_TAG_9F17_PIN_TRY_COUNTER                            (0x9F17)

/// EMV tag 9F18 Issuer Script Identifier. Template 71 or 72.
#define EMV_TAG_9F18_ISSUER_SCRIPT_IDENTIFIER                   (0x9F18)

/// EMV tag 9F19 Token Requestor ID. Template 70 or 77.
#define EMV_TAG_9F19_TOKEN_REQUESTOR_ID                         (0x9F19)

/// EMV tag 9F1A Terminal Country Code
#define EMV_TAG_9F1A_TERMINAL_COUNTRY_CODE                      (0x9F1A)

/// EMV tag 9F1B Terminal Floor Limit
#define EMV_TAG_9F1B_TERMINAL_FLOOR_LIMIT                       (0x9F1B)

/// EMV tag 9F1C Terminal Identification
/// @remark Also used as the Card Acceptor Terminal Identification by ISO 8583. See EMV 4.4 Book 4, Annex C, Table 40
#define EMV_TAG_9F1C_TERMINAL_IDENTIFICATION                    (0x9F1C)

/// EMV tag 9F1D Terminal Risk Management Data
/// @remark See EMV Contactless Book C-2 v2.11, Annex A.1.161
/// @remark See EMV Contactless Book C-8 v1.1, Annex A.1.129
/// @remark See M/Chip Requirements for Contact and Contactless, 28 November 2023, Chapter 5, Terminal Risk Management Data
#define EMV_TAG_9F1D_TERMINAL_RISK_MANAGEMENT_DATA              (0x9F1D)

/// EMV tag 9F1E Interface Device (IFD) Serial Number
#define EMV_TAG_9F1E_IFD_SERIAL_NUMBER                          (0x9F1E)

/// EMV tag 9F1F Track 1 Discretionary Data. Template 70 or 77.
#define EMV_TAG_9F1F_TRACK1_DISCRETIONARY_DATA                  (0x9F1F)

/// EMV tag 9F20 Track 2 Discretionary Data. Template 70 or 77.
#define EMV_TAG_9F20_TRACK2_DISCRETIONARY_DATA                  (0x9F20)

/// EMV tag 9F21 Transaction Time
#define EMV_TAG_9F21_TRANSACTION_TIME                           (0x9F21)

/// EMV tag 9F22 Certification Authority Public Key (CAPK) Index
#define EMV_TAG_9F22_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX   (0x9F22)

/// EMV tag 9F23 Upper Consecutive Offline Limit. Template 70 or 77.
#define EMV_TAG_9F23_UPPER_CONSECUTIVE_OFFLINE_LIMIT            (0x9F23)

/// EMV tag 9F24 Payment Account Reference (PAR). Template 70 or 77.
#define EMV_TAG_9F24_PAYMENT_ACCOUNT_REFERENCE                  (0x9F24)

/// EMV tag 9F25 Last 4 Digits of PAN. Template 70 or 77.
#define EMV_TAG_9F25_LAST_4_DIGITS_OF_PAN                       (0x9F25)

/// EMV tag 9F26 Application Cryptogram. Template 77 or 80.
#define EMV_TAG_9F26_APPLICATION_CRYPTOGRAM                     (0x9F26)

/// EMV tag 9F27 Cryptogram Information Data. Template 77 or 80.
#define EMV_TAG_9F27_CRYPTOGRAM_INFORMATION_DATA                (0x9F27)

/// EMV tag 9F32 Issuer Public Key Exponent. Template 70 or 77.
#define EMV_TAG_9F32_ISSUER_PUBLIC_KEY_EXPONENT                 (0x9F32)

/// EMV tag 9F33 Terminal Capabilities
#define EMV_TAG_9F33_TERMINAL_CAPABILITIES                      (0x9F33)

/// EMV tag 9F34 Cardholder Verification Method (CVM) Results
#define EMV_TAG_9F34_CVM_RESULTS                                (0x9F34)

/// EMV tag 9F35 Terminal Type
#define EMV_TAG_9F35_TERMINAL_TYPE                              (0x9F35)

/// EMV tag 9F36 Application Transaction Counter (ATC). Template 77 or 80.
#define EMV_TAG_9F36_APPLICATION_TRANSACTION_COUNTER            (0x9F36)

/// EMV tag 9F37 Unpredictable Number
#define EMV_TAG_9F37_UNPREDICTABLE_NUMBER                       (0x9F37)

/// EMV tag 9F38 Processing Options Data Object List (PDOL). Template A5.
#define EMV_TAG_9F38_PDOL                                       (0x9F38)

/// EMV tag 9F39 Point-of-Service (POS) Entry Mode
#define EMV_TAG_9F39_POS_ENTRY_MODE                             (0x9F39)

/// EMV tag 9F3A Amount, Reference Currency
#define EMV_TAG_9F3A_AMOUNT_REFERENCE_CURRENCY                  (0x9F3A)

/// EMV tag 9F3B Application Reference Currency. Template 70 or 77.
#define EMV_TAG_9F3B_APPLICATION_REFERENCE_CURRENCY             (0x9F3B)

/// EMV tag 9F3C Transaction Reference Currency
#define EMV_TAG_9F3C_TRANSACTION_REFERENCE_CURRENCY             (0x9F3C)

/// EMV tag 9F3D Transaction Reference Currency Exponent
#define EMV_TAG_9F3D_TRANSACTION_REFERENCE_CURRENCY_EXPONENT    (0x9F3D)

/// EMV tag 9F40 Additional Terminal Capabilities
#define EMV_TAG_9F40_ADDITIONAL_TERMINAL_CAPABILITIES           (0x9F40)

/// EMV tag 9F41 Transaction Sequence Counter
#define EMV_TAG_9F41_TRANSACTION_SEQUENCE_COUNTER               (0x9F41)

/// EMV tag 9F42 Application Currency Code. Template 70 or 77.
#define EMV_TAG_9F42_APPLICATION_CURRENCY_CODE                  (0x9F42)

/// EMV tag 9F43 Application Reference Currency Exponent. Template 70 or 77.
#define EMV_TAG_9F43_APPLICATION_REFERENCE_CURRENCY_EXPONENT    (0x9F43)

/// EMV tag 9F44 Application Currency Exponent. Template 70 or 77.
#define EMV_TAG_9F44_APPLICATION_CURRENCY_EXPONENT              (0x9F44)

/// EMV tag 9F45 Data Authentication Code
#define EMV_TAG_9F45_DATA_AUTHENTICATION_CODE                   (0x9F45)

/// EMV tag 9F46 Integrated Circuit Card (ICC) Public Key Certificate. Template 70 or 77.
#define EMV_TAG_9F46_ICC_PUBLIC_KEY_CERTIFICATE                 (0x9F46)

/// EMV tag 9F47 Integrated Circuit Card (ICC) Public Key Exponent. Template 70 or 77.
#define EMV_TAG_9F47_ICC_PUBLIC_KEY_EXPONENT                    (0x9F47)

/// EMV tag 9F48 Integrated Circuit Card (ICC) Public Key Remainder. Template 70 or 77.
#define EMV_TAG_9F48_ICC_PUBLIC_KEY_REMAINDER                   (0x9F48)

/// EMV tag 9F49 Dynamic Data Authentication Data Object List (DDOL). Template 70 or 77.
#define EMV_TAG_9F49_DDOL                                       (0x9F49)

/// EMV tag 9F4A Static Data Authentication (SDA) Tag List. Template 70 or 77.
#define EMV_TAG_9F4A_SDA_TAG_LIST                               (0x9F4A)

/// EMV tag 9F4C Integrated Circuit Card (ICC) Dynamic Number
#define EMV_TAG_9F4C_ICC_DYNAMIC_NUMBER                         (0x9F4C)

/// EMV tag 9F4D Log Entry. Template BF0C or 73.
#define EMV_TAG_9F4D_LOG_ENTRY                                  (0x9F4D)

/// EMV tag 9F4E Merchant Name and Location
#define EMV_TAG_9F4E_MERCHANT_NAME_AND_LOCATION                 (0x9F4E)

/// EMV tag 9F66 Terminal Transaction Qualifiers (TTQ)
/// @remark See EMV Contactless Book A v2.10, 5.7, Table 5-4
#define EMV_TAG_9F66_TTQ                                        (0x9F66)

/// EMV tag 9F6C Card Transaction Qualifiers (CTQ)
/// @remark See EMV Contactless Book C-3 v2.10, Annex A.2
/// @remark See EMV Contactless Book C-7 v2.9, Annex A
/// @remark See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
#define EMV_TAG_9F6C_CTQ                                        (0x9F6C)

/// Amex tag 9F6D Contactless Reader Capabilities
/// @remark See EMV Contactless Book C-4 v2.10, 4.3.3, Table 4-2
#define AMEX_TAG_9F6D_CONTACTLESS_READER_CAPABILITIES           (0x9F6D)

/// Mastercard tag 9F6E Third Party Data. Template BF0C or 70.
/// @remark See EMV Contactless Book C-2 v2.10, Annex A.1.171
/// @remark See M/Chip Requirements for Contact and Contactless, 15 March 2022, Chapter 5, Third Party Data, Table 12
#define MASTERCARD_TAG_9F6E_THIRD_PARTY_DATA                    (0x9F6E)

/// Visa tag 9F6E Form Factor Indicator (FFI)
/// @remark See EMV Contactless Book C-3 v2.10, Annex A.2
/// @remark See Visa Contactless Payment Specification (VCPS) Supplemental Requirements, version 2.2, January 2016, Annex D
#define VISA_TAG_9F6E_FORM_FACTOR_INDICATOR                     (0x9F6E)

/// Amex tag 9F6E Enhanced Contactless Reader Capabilities
/// @remark See EMV Contactless Book C-4 v2.10, 4.3.4, Table 4-4
#define AMEX_TAG_9F6E_ENHANCED_CONTACTLESS_READER_CAPABILITIES  (0x9F6E)

/// EMV tag BF0C File Control Information (FCI) Issuer Discretionary Data. Template A5.
#define EMV_TAG_BF0C_FCI_ISSUER_DISCRETIONARY_DATA              (0xBF0C)

__END_DECLS

#endif
