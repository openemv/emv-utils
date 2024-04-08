/**
 * @file iso8825_ber.h
 * @brief Basic Encoding Rules (BER) implementation
 *        (see ISO/IEC 8825-1 or ITU-T Rec X.690 07/2002)
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

#ifndef ISO8825_BER_H
#define ISO8825_BER_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

__BEGIN_DECLS

// Encoding of tag class (see ISO 8825-1:2003, 8.1.2, table 1)
#define ISO8825_BER_CLASS_MASK                  (0xC0) ///< BER tag mask for class bits
#define ISO8825_BER_CLASS_UNIVERSAL             (0x00) ///< BER class: universal
#define ISO8825_BER_CLASS_APPLICATION           (0x40) ///< BER class: application
#define ISO8825_BER_CLASS_CONTEXT               (0x80) ///< BER class: context-specific
#define ISO8825_BER_CLASS_PRIVATE               (0xC0) ///< BER class: private

// Primitive/constructed encoding (see ISO 8825-1:2003, 8.1.2.5)
#define ISO8825_BER_CONSTRUCTED                 (0x20) ///< BER primitive/constructed bit

// Tag number encoding (see ISO 8825-1:2003, 8.1.2)
#define ISO8825_BER_TAG_NUMBER_MASK             (0x1F) ///< BER tag mask for tag number
#define ISO8825_BER_TAG_HIGH_FORM               (0x1F) ///< BER high tag number form; for tag numbers >= 31
#define ISO8825_BER_TAG_HIGH_FORM_MORE          (0x80) ///< BER high tag number form: more octets to follow
#define ISO8825_BER_TAG_HIGH_FORM_NUMBER_MASK   (0x7F) ///< BER high tag number form: next 7 bits of tag number

// Length encoding (see ISO 8825-1:2003, 8.1.3)
#define ISO8825_BER_LEN_INDEFINITE_FORM         (0x80) ///< BER indefinite length form value
#define ISO8825_BER_LEN_LONG_FORM               (0x80) ///< BER definite long length form bit; for length values > 127
#define ISO8825_BER_LEN_LONG_FORM_COUNT_MASK    (0x7F) ///< BER definite long length form mask: number of length octets

// Universal ASN.1 types
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
#define ASN1_SEQUENCE                           (0x10) ///< ASN.1 Sequence and Sequence-of types
#define ASN1_SET                                (0x11) ///< ASN.1 Set and Set-of types
#define ASN1_NUMERICSTRING                      (0x12) ///< ASN.1 Numeric string type
#define ASN1_PRINTABLESTRING                    (0x13) ///< ASN.1 Printable string type
#define ASN1_T61STRING                          (0x14) ///< ASN.1 T61 / Teletex string type
#define ASN1_VIDEOTEXSTRING                     (0x15) ///< ASN.1 Videotex string type
#define ASN1_IA5STRING                          (0x16) ///< ASN.1 IA5 (ISO 646) string type
#define ASN1_UTCTIME                            (0x17) ///< ASN.1 UTC time type
#define ASN1_GENERALIZEDTIME                    (0x18) ///< ASN.1 Generalised time type
#define ASN1_GRAPHICSTRING                      (0x19) ///< ASN.1 Graphic string type
#define ASN1_ISO646STRING                       (0x1A) ///< ASN.1 ISO 646 string type
#define ASN1_GENERALSTRING                      (0x1B) ///< ASN.1 General string type
#define ASN1_UNIVERSALSTRING                    (0x1C) ///< ASN.1 Universal string type
#define ASN1_BMPSTRING                          (0x1E) ///< ASN.1 Basic Multilingual Plane (BMP) string type

// ASN.1 object identifier top-level authorities (see ISO 8824-1:2003, Annex D)
#define ASN1_OID_ITU_T                          (0) ///< ASN.1 ITU-T
#define ASN1_OID_ISO                            (1) ///< ASN.1 ISO
#define ASN1_OID_JOINT                          (2) ///< Joint ISO/IEC and ITU-T

// ASN.1 object identifier arcs for ITU-T
#define ASN1_OID_ITU_T_RECOMMENDED              (0) ///< ASN.1 ITU-T arc for ITU-T and CCITT recommendations
#define ASN1_OID_ITU_T_QUESTION                 (1) ///< ASN.1 ITU-T arc for ITU-T Study Groups
#define ASN1_OID_ITU_T_ADMINISTRATION           (2) ///< ASN.1 ITU-T arc for ITU-T X.121 Data Country Codes (DCCs)
#define ASN1_OID_ITU_T_NETWORK_OPERATOR         (3) ///< ASN.1 ITU-T arc for ITU-T X.121 Data Network Identification Code (DNICs)
#define ASN1_OID_ITU_T_IDENTIFIED_ORG           (4) ///< ASN.1 ITU-T arc for ITU-T Telecommunication Standardization Bureau (TSB) identified organisations

// ASN.1 object identifier arcs for ISO
#define ASN1_OID_ISO_STANDARD                   (0) ///< ASN.1 ISO arc for ISO standards
#define ASN1_OID_ISO_MEMBER_BODY                (1) ///< ASN.1 ISO arc for ISO National Bodies
#define ASN1_OID_ISO_IDENTIFIED_ORG             (2) ///< ASN.1 ISO arc for ISO identified organisations (ISO 6523)

// ASN.1 object identifier arcs for Joint ISO/IEC and ITU-T directory services (interesting ones only)
#define ASN1_OID_JOINT_DS                       (5) ///< ASN.1 joint-iso-itu-t directory services
#define ASN1_OID_JOINT_DS_ATTR_TYPE             (4) ///< ASN.1 joint-iso-itu-t directory services attribyte type
#define ASN1_OID_JOINT_DS_OBJ_CLASS             (6) ///< ASN.1 joint-iso-itu-t directory services object class

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
	uint32_t value[10];
	unsigned int length;
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
 * Decode BER object identifier
 * @param ptr BER encoded object identifer
 * @param len Length of BER encoded object identifer in bytes
 * @param oid Decoded OID output
 * @return Zero for success. Less than zero for error.
 */
int iso8825_ber_oid_decode(const void* ptr, size_t len, struct iso8825_oid_t* oid);

__END_DECLS

#endif
