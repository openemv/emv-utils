/**
 * @file iso8825_ber.h
 * @brief Basic Encoding Rules (BER) implementation
 *        (see ISO/IEC 8825-1:2021 or Rec. ITU-T X.690 02/2021)
 *
 * Copyright 2021, 2024 Leon Lynch
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

#ifndef ISO8825_BER_H
#define ISO8825_BER_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

__BEGIN_DECLS

// Encoding of tag class (see ISO 8825-1:2021, 8.1.2, table 1)
#define ISO8825_BER_CLASS_MASK                  (0xC0) ///< BER tag mask for class bits
#define ISO8825_BER_CLASS_UNIVERSAL             (0x00) ///< BER class: universal
#define ISO8825_BER_CLASS_APPLICATION           (0x40) ///< BER class: application
#define ISO8825_BER_CLASS_CONTEXT               (0x80) ///< BER class: context-specific
#define ISO8825_BER_CLASS_PRIVATE               (0xC0) ///< BER class: private

// Primitive/constructed encoding (see ISO 8825-1:2021, 8.1.2.5)
#define ISO8825_BER_CONSTRUCTED                 (0x20) ///< BER primitive/constructed bit

// Tag number encoding (see ISO 8825-1:2021, 8.1.2)
#define ISO8825_BER_TAG_NUMBER_MASK             (0x1F) ///< BER tag mask for tag number
#define ISO8825_BER_TAG_HIGH_FORM               (0x1F) ///< BER high tag number form; for tag numbers >= 31
#define ISO8825_BER_TAG_HIGH_FORM_MORE          (0x80) ///< BER high tag number form: more octets to follow
#define ISO8825_BER_TAG_HIGH_FORM_NUMBER_MASK   (0x7F) ///< BER high tag number form: next 7 bits of tag number

// Length encoding (see ISO 8825-1:2021, 8.1.3)
#define ISO8825_BER_LEN_INDEFINITE_FORM         (0x80) ///< BER indefinite length form value
#define ISO8825_BER_LEN_LONG_FORM               (0x80) ///< BER definite long length form bit; for length values > 127
#define ISO8825_BER_LEN_LONG_FORM_COUNT_MASK    (0x7F) ///< BER definite long length form mask: number of length octets

// Universal ASN.1 types (see ISO 8824-1:2021, 8.4)
#define ASN1_EOC                                (0x00) ///< ASN.1 End-of-content
#define ASN1_BOOLEAN                            (0x01) ///< ASN.1 Boolean type
#define ASN1_INTEGER                            (0x02) ///< ASN.1 Integer type
#define ASN1_BIT_STRING                         (0x03) ///< ASN.1 Bit string type
#define ASN1_OCTET_STRING                       (0x04) ///< ASN.1 Octet string type
#define ASN1_NULL                               (0x05) ///< ASN.1 Null type
#define ASN1_OBJECT_IDENTIFIER                  (0x06) ///< ASN.1 Object Identifier (OID) type
#define ASN1_OBJECT_DESCRIPTOR                  (0x07) ///< ASN.1 Object descriptor type
#define ASN1_EXTERNAL                           (0x08) ///< ASN.1 External type
#define ASN1_REAL                               (0x09) ///< ASN.1 Real type
#define ASN1_ENUMERATED                         (0x0A) ///< ASN.1 Enumerated type
#define ASN1_EMBEDDED_PDV                       (0x0B) ///< ASN.1 Embedded PDV type
#define ASN1_UTF8STRING                         (0x0C) ///< ASN.1 UTF-8 string type
#define ASN1_RELATIVE_OBJECT_IDENTIFIER         (0x0D) ///< ASN.1 Relative object identifier type
#define ASN1_TIME                               (0x0E) ///< ASN.1 Time type
#define ASN1_SEQUENCE                           (0x10) ///< ASN.1 Sequence and Sequence-of types
#define ASN1_SET                                (0x11) ///< ASN.1 Set and Set-of types
#define ASN1_NUMERICSTRING                      (0x12) ///< ASN.1 Numeric string type
#define ASN1_PRINTABLESTRING                    (0x13) ///< ASN.1 Printable string type
#define ASN1_TELETEXSTRING                      (0x14) ///< ASN.1 Teletex (T61) string type
#define ASN1_VIDEOTEXSTRING                     (0x15) ///< ASN.1 Videotex string type
#define ASN1_IA5STRING                          (0x16) ///< ASN.1 IA5 (ISO 646) string type
#define ASN1_UTCTIME                            (0x17) ///< ASN.1 UTC time type
#define ASN1_GENERALIZEDTIME                    (0x18) ///< ASN.1 Generalized time type
#define ASN1_GRAPHICSTRING                      (0x19) ///< ASN.1 Graphic string type
#define ASN1_VISIBLESTRING                      (0x1A) ///< ASN.1 Visible (ISO 646) string type
#define ASN1_GENERALSTRING                      (0x1B) ///< ASN.1 General string type
#define ASN1_UNIVERSALSTRING                    (0x1C) ///< ASN.1 Universal string type
#define ASN1_CHARACTERSTRING                    (0x1D) ///< ASN.1 Unrestricted character string type
#define ASN1_BMPSTRING                          (0x1E) ///< ASN.1 Basic Multilingual Plane (BMP) string type
#define ASN1_DATE                               (0x1F) ///< ASN.1 Date type
#define ASN1_TIME_OF_DAY                        (0x20) ///< ASN.1 Time-of-day type
#define ASN1_DATE_TIME                          (0x21) ///< ASN.1 Date-Time type
#define ASN1_DURATION                           (0x22) ///< ASN.1 Duration type
#define ASN1_OID_IRI                            (0x23) ///< ASN.1 Object Identifier (OID) Internationalized Resource Identifier (IRI)
#define ASN1_RELATIVE_OID_IRI                   (0x24) ///< ASN.1 Relative Object Identifier (OID) Internationalized Resource Identifier (IRI)

// ASN.1 object identifier top-level authorities (see ISO 9834-1:2012, Annex A.2)
#define ASN1_OID_ITU_T                          (0) ///< ASN.1 ITU-T
#define ASN1_OID_ISO                            (1) ///< ASN.1 ISO
#define ASN1_OID_JOINT                          (2) ///< Joint ISO/IEC and ITU-T

// ASN.1 object identifier arcs for ITU-T (see ISO 9834-1:2012, Annex A.3)
#define ASN1_OID_ITU_T_RECOMMENDED              (0) ///< ASN.1 ITU-T arc for ITU-T and CCITT recommendations
#define ASN1_OID_ITU_T_QUESTION                 (1) ///< ASN.1 ITU-T arc for ITU-T Study Groups
#define ASN1_OID_ITU_T_ADMINISTRATION           (2) ///< ASN.1 ITU-T arc for ITU-T X.121 Data Country Codes (DCCs)
#define ASN1_OID_ITU_T_NETWORK_OPERATOR         (3) ///< ASN.1 ITU-T arc for ITU-T X.121 Data Network Identification Code (DNICs)
#define ASN1_OID_ITU_T_IDENTIFIED_ORG           (4) ///< ASN.1 ITU-T arc for ITU-T Telecommunication Standardization Bureau (TSB) identified organisations
#define ASN1_OID_ITU_R_RECOMMENDATION           (5) ///< ASN.1 ITU-T arc for ITU-R Recommendations

// ASN.1 object identifier arcs for ISO (see ISO 9834-1:2012, Annex A.4)
#define ASN1_OID_ISO_STANDARD                   (0) ///< ASN.1 ISO arc for ISO standards
#define ASN1_OID_ISO_REGISTRATION_AUTHORITY     (1) ///< ASN.1 ISO arc for ISO standards for operation of a Registration Authority
#define ASN1_OID_ISO_MEMBER_BODY                (2) ///< ASN.1 ISO arc for ISO National Bodies
#define ASN1_OID_ISO_IDENTIFIED_ORG             (3) ///< ASN.1 ISO arc for ISO identified organisations (ISO 6523)

// ASN.1 object identifier arcs for Joint ISO/IEC and ITU-T directory services (interesting ones only)
#define ASN1_OID_JOINT_DS                       (5) ///< ASN.1 joint-iso-itu-t directory services
#define ASN1_OID_JOINT_DS_ATTR_TYPE             (4) ///< ASN.1 joint-iso-itu-t directory services attribyte type
#define ASN1_OID_JOINT_DS_OBJ_CLASS             (6) ///< ASN.1 joint-iso-itu-t directory services object class

// ASN.1 object identifiers provided by ISO 9797
#define ASN1_OID_cbcmac { 1, 0, 9797, 1, 3, 1 } ///< ASN.1 OID for ISO 9797-1:2011 MAC Algorithm 1, also known as CBC-MAC
#define ASN1_OID_retailmac { 1, 0, 9797, 1, 3, 3 } ///< ASN.1 OID for ISO 9797-1:2011 MAC Algorithm 3, also known as ANSI X9.19 Retail MAC
#define ASN1_OID_cmac { 1, 0, 9797, 1, 3, 5 } ///< ASN.1 OID for ISO 9797-1:2011 MAC Algorithm 5, also known as CMAC
#define ASN1_OID_hmac { 1, 0, 9797, 2, 2 } ///< ASN.1 OID for ISO 9797-2:2011 MAC Algorithm 2, also known as HMAC

// ASN.1 object identifiers provided by ANSI X9.62 / X9.142
#define ASN1_OID_ecPublicKey { 1, 2, 840, 10045, 2, 1 } ///< ASN.1 OID for ANSI X9.62 Elliptic curve public key
#define ASN1_OID_prime256v1 { 1, 2, 840, 10045, 3, 1, 7 } ///< ASN.1 OID for ANSI X9.62 Elliptic curve prime256v1 / secp256r1 / NIST P-256

// ASN.1 object identifiers provided by PKCS#1 v2.2 (RFC 8017) and PKCS#9 v2.0 (RFC 2985)
#define ASN1_OID_rsaEncryption { 1, 2, 840, 113549, 1, 1, 1 } ///< ASN.1 OID for PKCS#1 v2.2 RSA
#define ASN1_OID_sha1WithRSAEncryption { 1, 2, 840, 113549, 1, 1, 5 } ///< ASN.1 OID for PKCS#1 v2.2 SHA1 with RSA
#define ASN1_OID_sha256WithRSAEncryption { 1, 2, 840, 113549, 1, 1, 11 } ///< ASN.1 OID for PKCS#1 v2.2 SHA256 with RSA
#define ASN1_OID_emailAddress { 1, 2, 840, 113549, 1, 9, 1 } ///< ASN.1 OID for PKCS#9 v2.0 email address

// ASN.1 object identifiers provided by ANSI X9.24-3:2017, 6.1.2
#define ASN1_OID_dukpt_aes128 { 1, 3, 133, 16, 840, 9, 24, 1, 1 } ///< ASN.1 OID for ANSI X9.24-3:2017 AES-128 transaction key
#define ASN1_OID_dukpt_aes192 { 1, 3, 133, 16, 840, 9, 24, 1, 2 } ///< ASN.1 OID for ANSI 2X9.24-3:2017 AES-192 transaction key
#define ASN1_OID_dukpt_aes256 { 1, 3, 133, 16, 840, 9, 24, 1, 3 } ///< ASN.1 OID for ANSI X9.24-3:2017 AES-256 transaction key
#define ASN1_OID_dukpt_tdes2 { 1, 3, 133, 16, 840, 9, 24, 1, 4 } ///< ASN.1 OID for ANSI X9.24-3:2017 TDES2 transaction key
#define ASN1_OID_dukpt_tdes3 { 1, 3, 133, 16, 840, 9, 24, 1, 5 } ///< ASN.1 OID for ANSI X9.24-3:2017 TDES3 transaction key

// ASN.1 object identifiers provided by Rec. ITU-T X.520 Annex A
#define ASN1_OID_commonName { 2, 5, 4, 3 } ///< ASN.1 OID for ITU-T X.520 commonName
#define ASN1_OID_surname { 2, 5, 4, 4 } ///< ASN.1 OID for ITU-T X.520 surname
#define ASN1_OID_serialNumber { 2, 5, 4, 5 } ///< ASN.1 OID for ITU-T X.520 serialNumber
#define ASN1_OID_countryName { 2, 5, 4, 6 } ///< ASN.1 OID for ITU-T X.520 countryName
#define ASN1_OID_localityName { 2, 5, 4, 7 } ///< ASN.1 OID for ITU-T X.520 localityName
#define ASN1_OID_stateOrProvinceName { 2, 5, 4, 8 } ///< ASN.1 OID for ITU-T X.520 stateOrProvinceName
#define ASN1_OID_streetAddress { 2, 5, 4, 9 } ///< ASN.1 OID for ITU-T X.520 streetAddress
#define ASN1_OID_organizationName { 2, 5, 4, 10 } ///< ASN.1 OID for ITU-T X.520 organizationName
#define ASN1_OID_organizationalUnitName { 2, 5, 4, 11 } ///< ASN.1 OID for ITU-T X.520 organizationalUnitName
#define ASN1_OID_title { 2, 5, 4, 12 } ///< ASN.1 OID for ITU-T X.520 title
#define ASN1_OID_description { 2, 5, 4, 13 } ///< ASN.1 OID for ITU-T X.520 description
#define ASN1_OID_postalAddress { 2, 5, 4, 16 } ///< ASN.1 OID for ITU-T X.520 postalAddress
#define ASN1_OID_postalCode { 2, 5, 4, 17 } ///< ASN.1 OID for ITU-T X.520 postalCode
#define ASN1_OID_postOfficeBox { 2, 5, 4, 18 } ///< ASN.1 OID for ITU-T X.520 postOfficeBox
#define ASN1_OID_telephoneNumber { 2, 5, 4, 20 } ///< ASN.1 OID for ITU-T X.520 telephoneNumber
#define ASN1_OID_name { 2, 5, 4, 41 } ///< ASN.1 OID for ITU-T X.520 name
#define ASN1_OID_givenName { 2, 5, 4, 42 } ///< ASN.1 OID for ITU-T X.520 givenName
#define ASN1_OID_initials { 2, 5, 4, 43 } ///< ASN.1 OID for ITU-T X.520 initials
#define ASN1_OID_uniqueIdentifier { 2, 5, 4, 45 } ///< ASN.1 OID for ITU-T X.520 uniqueIdentifier
#define ASN1_OID_url { 2, 5, 4, 87 } ///< ASN.1 OID for ITU-T X.520 url

/// Macro used to compute ASN.1 OID arc length by name
#define ASN1_OID_ARC_LEN(name) (sizeof((uint32_t[])ASN1_OID_##name) / sizeof((uint32_t[])ASN1_OID_##name[0]))

/// Macro used to create initializer list for @ref iso8825_oid_t
#define ASN1_OID(name) { ASN1_OID_ARC_LEN(name), ASN1_OID_##name }

/// ISO 8825 TLV field
struct iso8825_tlv_t {
	unsigned int tag;                           ///< BER encoded tag, including class, primitive/structured bit, and tag number
	unsigned int length;                        ///< BER decoded length of @c value in bytes
	const uint8_t* value;                       ///< BER value
	uint8_t flags;                              ///< Flags for use by helper functions
};

/// ISO 8825 BER iterator
struct iso8825_ber_itr_t {
	const void* ptr;
	size_t len;
};

/// ASN.1 OID
struct iso8825_oid_t {
	unsigned int length;        ///< Number of component values (arc length)
	uint32_t value[10];         ///< List of component values
};

/**
 * Decode BER tag octets
 * @param ptr BER encoded data
 * @param len Length of BER encoded data in bytes
 * @param tag Decoded tag output
 * @return Number of bytes consumed. Zero for end of data. Less than zero for error.
 */
int iso8825_ber_tag_decode(const void* ptr, size_t len, unsigned int* tag);

/**
 * Decode BER data
 * @param ptr BER encoded data
 * @param len Length of BER encoded data in bytes
 * @param tlv Decoded TLV output
 * @return Number of bytes consumed. Zero for end of data. Less than zero for error.
 */
int iso8825_ber_decode(const void* ptr, size_t len, struct iso8825_tlv_t* tlv);

/**
 * Retrieve class of BER tag type
 * @param tlv Decoded TLV structure
 * @retval ISO8825_BER_CLASS_UNIVERSAL
 * @retval ISO8825_BER_CLASS_APPLICATION
 * @retval ISO8825_BER_CLASS_CONTEXT
 * @retval ISO8825_BER_CLASS_PRIVATE
 */
static inline uint8_t iso8825_ber_get_class(const struct iso8825_tlv_t* tlv) { return tlv ? (tlv->flags & ISO8825_BER_CLASS_MASK) : 0; }

/**
 * Determine whether BER tag type is constructed
 * @param tlv Decoded TLV structure
 * @return Boolean indicating whether BER tag type is constructed
 */
static inline bool iso8825_ber_is_constructed(const struct iso8825_tlv_t* tlv) { return tlv && (tlv->flags & ISO8825_BER_CONSTRUCTED); }

/**
 * Determine whether BER tag type is a string
 * @param tlv Decoded TLV structure
 * @return Boolean indicating whether BER tag type is a string
 */
bool iso8825_ber_is_string(const struct iso8825_tlv_t* tlv);

/**
 * Initialise BER iterator
 * @param ptr BER encoded data
 * @param len Length of BER encoded data in bytes
 * @param itr BER iterator output
 * @return Zero for success. Less than zero for error.
 */
int iso8825_ber_itr_init(const void* ptr, size_t len, struct iso8825_ber_itr_t* itr);

/**
 * Decode next element and advance iterator
 * @param itr BER iterator
 * @param tlv Decoded TLV output
 * @return Number of bytes consumed. Zero for end of data. Less than zero for error.
 */
int iso8825_ber_itr_next(struct iso8825_ber_itr_t* itr, struct iso8825_tlv_t* tlv);

/**
 * Decode BER object identifier (OID)
 * @param ptr BER encoded object identifer
 * @param len Length of BER encoded object identifer in bytes
 * @param oid Decoded OID output
 * @return Zero for success. Less than zero for error.
 */
int iso8825_ber_oid_decode(const void* ptr, size_t len, struct iso8825_oid_t* oid);

__END_DECLS

#endif
