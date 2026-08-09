/**
 * @file iso14443.h
 * @brief ISO/IEC 14443 definitions and helper functions
 *
 * Copyright 2026 Leon Lynch
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

#ifndef ISO14443_H
#define ISO14443_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

__BEGIN_DECLS

// ATS response format (ISO/IEC 14443-4:2008, 5.2)
#define ISO14443_ATS_MIN_SIZE           (1)  ///< Minimum size of ATS buffer
#define ISO14443_ATS_MAX_SIZE           (20) ///< Maximum size of ATS buffer

// ATS: Interface byte TA(1) definitions (ISO/IEC 14443-4:2008, 5.2.4)
#define ISO14443_ATS_TA1_SAME_D         (0x80) ///< TA(1) bit 8 indicates only same divisor D for both directions is supported
#define ISO14443_ATS_TA1_DS_MASK        (0x70) ///< TA(1) bits 5-7 mask for DS (divisor from PICC to PCD)
#define ISO14443_ATS_TA1_DS_SHIFT       (4)    ///< TA(1) bitshift to normalise DS to bits 1-3
#define ISO14443_ATS_TA1_RFU            (0x08) ///< TA(1) bit 4 is RFU
#define ISO14443_ATS_TA1_DR_MASK        (0x07) ///< TA(1) bits 1-3 mask for DR (divisor from PCD to PICC)

// ATS info: Supported bit rate divisors (used in DS and DR bitfields) (ISO/IEC 14443-4:2008, 5.2.4)
#define ISO14443_D_ONLY_1               (0x00) ///< Only D=1 (106 kbit/s) is supported
#define ISO14443_D2_SUPPORTED           (0x01) ///< D=2 (212 kbit/s) is supported
#define ISO14443_D4_SUPPORTED           (0x02) ///< D=4 (424 kbit/s) is supported
#define ISO14443_D8_SUPPORTED           (0x04) ///< D=8 (848 kbit/s) is supported

// ATS: Format byte T0 definitions (ISO/IEC 14443-4:2008, 5.2.3)
#define ISO14443_ATS_T0_RFU             (0x80) ///< T0 bit 8 is RFU
#define ISO14443_ATS_T0_TC1_PRESENT     (0x40) ///< T0 bit 7 indicates interface byte TC(1) is present
#define ISO14443_ATS_T0_TB1_PRESENT     (0x20) ///< T0 bit 6 indicates interface byte TB(1) is present
#define ISO14443_ATS_T0_TA1_PRESENT     (0x10) ///< T0 bit 5 indicates interface byte TA(1) is present
#define ISO14443_ATS_T0_FSCI_MASK       (0x0F) ///< T0 bits 1-4 encode FSCI (Frame Size Card Integer)

// ATS: Interface byte TB(1) definitions (ISO/IEC 14443-4:2008, 5.2.5)
#define ISO14443_ATS_TB1_FWI_MASK       (0xF0) ///< TB(1) bits 5-8 encode FWI (Frame Waiting time Integer)
#define ISO14443_ATS_TB1_FWI_SHIFT      (4)    ///< TB(1) bitshift for FWI value
#define ISO14443_ATS_TB1_SFGI_MASK      (0x0F) ///< TB(1) bits 1-4 encode SFGI (Start-up Frame Guard time Integer)

// ATS: Interface byte TC(1) definitions (ISO/IEC 14443-4:2008, 5.2.6)
#define ISO14443_ATS_TC1_CID            (0x02) ///< TC(1) bit 2 indicates CID is supported
#define ISO14443_ATS_TC1_NAD            (0x01) ///< TC(1) bit 1 indicates NAD is supported

// ATS: Historical byte category indicator T1 definitions (ISO/IEC 7816-4:2005, 8.1.1)
#define ISO14443_ATS_T1_COMPACT_TLV_SI  (0x00) ///< Subsequent historical bytes are COMPACT-TLV encoded followed by mandatory status indicator
#define ISO14443_ATS_T1_DIR_DATA_REF    (0x10) ///< Subsequent historical byte is DIR data reference
#define ISO14443_ATS_T1_COMPACT_TLV     (0x80) ///< Subsequent historical bytes are COMPACT-TLV encoded and may include status indicator

// ATQB response format (ISO/IEC 14443-3:2011, 7.9.1)
#define ISO14443_ATQB_MIN_SIZE                  (12)   ///< Minimum size of ATQB buffer (Basic ATQB)
#define ISO14443_ATQB_MAX_SIZE                  (13)   ///< Maximum size of ATQB buffer (Extended ATQB)
#define ISO14443_ATQB_MARKER                    (0x50) ///< Answer To Request Type B marker byte

// ATQB: Protocol Info byte 1 definitions (ISO/IEC 14443-3:2011, 7.9.4.6)
#define ISO14443_ATQB_PI1_SAME_D                (0x80) ///< PI(1) bit 8 indicates only same divisor D for both directions is supported
#define ISO14443_ATQB_PI1_DS_MASK               (0x70) ///< PI(1) bits 5-7 mask for DS (divisor from PICC to PCD)
#define ISO14443_ATQB_PI1_DS_SHIFT              (4)    ///< PI(1) bitshift to normalise DS to bits 1-3
#define ISO14443_ATQB_PI1_RFU                   (0x08) ///< PI(1) bit 4 is RFU
#define ISO14443_ATQB_PI1_DR_MASK               (0x07) ///< PI(1) bits 1-3 mask for DR (divisor from PCD to PICC)

// ATQB: Protocol Info byte 2 definitions (ISO/IEC 14443-3:2011, 7.9.4.4 and 7.9.4.5)
#define ISO14443_ATQB_PI2_FSCI_MASK             (0xF0) ///< PI(2) bits 5-8 encode Max_Frame_Size (FSCI)
#define ISO14443_ATQB_PI2_FSCI_SHIFT            (4)    ///< PI(2) bitshift to normalise FSCI to bits 1-4
#define ISO14443_ATQB_PI2_PROTO_MASK            (0x0F) ///< PI(2) bits 1-4 mask for Protocol_Type
#define ISO14443_ATQB_PI2_PROTO_RFU             (0x08) ///< PI(2) bit 4 of Protocol_Type is RFU
#define ISO14443_ATQB_PI2_PROTO_MIN_TR2_MASK    (0x06) ///< PI(2) bits 2-3 encode minimum TR2
#define ISO14443_ATQB_PI2_PROTO_MIN_TR2_SHIFT   (1)    ///< PI(2) bitshift for minimum TR2 value
#define ISO14443_ATQB_PI2_PROTO_ISO14443_4      (0x01) ///< PI(2) bit 1 indicates ISO/IEC 14443-4 compliant PICC

// ATQB: Protocol Info byte 3 definitions (ISO/IEC 14443-3:2011, 7.9.4.1 - 7.9.4.3)
#define ISO14443_ATQB_PI3_FWI_MASK              (0xF0) ///< PI(3) bits 5-8 encode FWI (Frame Waiting time Integer)
#define ISO14443_ATQB_PI3_FWI_SHIFT             (4)    ///< PI(3) bitshift for FWI value
#define ISO14443_ATQB_PI3_ADC_MASK              (0x0C) ///< PI(3) bits 3-4 encode ADC (Application Data Coding)
#define ISO14443_ATQB_PI3_ADC_SHIFT             (2)    ///< PI(3) bitshift for ADC value
#define ISO14443_ATQB_PI3_ADC_RFU               (0x08) ///< PI(3) bit 4 of ADC is RFU
#define ISO14443_ATQB_PI3_ADC_ISO14443_3        (0x04) ///< PI(3) bit 3 of ADC indicates coding per ISO/IEC 14443-3, 7.9.3
#define ISO14443_ATQB_PI3_FO_MASK               (0x03) ///< PI(3) bits 1-2 encode FO (Frame Options)
#define ISO14443_ATQB_PI3_FO_NAD                (0x02) ///< PI(3) bit 2 indicates NAD is supported
#define ISO14443_ATQB_PI3_FO_CID                (0x01) ///< PI(3) bit 1 indicates CID is supported

// ATQB info: Application Data Coding (ADC) values (ISO/IEC 14443-3:2011, 7.9.4.2)
#define ISO14443_ADC_PROPRIETARY                (0x00) ///< Application Data Coding (ADC) is proprietary
#define ISO14443_ADC_ISO14443_3                 (0x01) ///< Application Data Coding (ADC) is as described in ISO/IEC 14443-3, 7.9.3

// ATQB info: Application Family Identifier (AFI) encoding (ISO/IEC 14443-3:2011, 7.7.3, table 22)
#define ISO14443_AFI_FAMILY_MASK                (0xF0) ///< Mask for application family
#define ISO14443_AFI_SUBFAMILY_MASK             (0x0F) ///< Mask for application sub-family

// ATQB info: Number of Applications encoding (ISO/IEC 14443-3:2011, 7.9.3.3)
#define ISO14443_NUM_APPS_MATCHING_MASK         (0xF0) ///< Mask for number of applications matching AFI
#define ISO14443_NUM_APPS_MATCHING_SHIFT        (4)    ///< Bitshift for number of applications matching AFI
#define ISO14443_NUM_APPS_TOTAL_MASK            (0x0F) ///< Mask for total number of applications in the PICC
#define ISO14443_NUM_APPS_MANY                  (15)   ///< Number of applications is 15 or more

// ATQB: Protocol Info byte 4 definitions (ISO/IEC 14443-3:2011, 7.9.4.7)
#define ISO14443_ATQB_PI4_SFGI_MASK             (0xF0) ///< PI(4) bits 5-8 encode Start-up Frame Guard time Integer (SFGI)
#define ISO14443_ATQB_PI4_SFGI_SHIFT            (4)    ///< PI(4) bitshift for SFGI value

/**
 * Parsed ATS (Answer To Select) information for ISO/IEC 14443 type A cards.
 *
 * This structure represents the parsed and decoded ATS information as defined
 * by the ISO/IEC 14443-4 protocol activation procedure (RATS response).
 *
 * The length byte TL is mandatory. All other fields (T0, TA(1), TB(1), TC(1),
 * and historical bytes) are optional. When absent, default values are applied.
 */
struct iso14443_ats_info_t {
	// Store ATS bytes for interface byte pointers to use
	uint8_t ats[ISO14443_ATS_MAX_SIZE]; ///< ATS bytes
	size_t ats_len; ///< Length of ATS in bytes

	/**
	 * Length byte TL is mandatory and specifies the total length of the ATS
	 * including TL itself. The two CRC_A bytes are not included in TL.
	 */
	uint8_t TL;

	// ========================================
	// Interface byte parsing...
	// ========================================

	/**
	 * Format byte T0 indicates FSCI value and presence of interface bytes.
	 * Value is available when pointer is non-NULL. Otherwise value is absent.
	 * - Bit 8 is RFU
	 * - Bit 7 indicates presence of interface byte TC(1)
	 * - Bit 6 indicates presence of interface byte TB(1)
	 * - Bit 5 indicates presence of interface byte TA(1)
	 * - Low 4 bits encode FSCI (Frame Size Card Integer); default is 2
	 */
	const uint8_t* T0;

	/**
	 * Interface byte TA(1) indicates bit rate capabilities of the PICC.
	 * Value is available when pointer is non-NULL. Otherwise value is absent.
	 * - Bit 8: if set, only the same divisor D is supported for both directions
	 * - Bits 5-7 encode DS (divisor from PICC to PCD)
	 * - Bit 4 is RFU
	 * - Bits 1-3 encode DR (divisor from PCD to PICC)
	 */
	const uint8_t* TA1;

	/**
	 * Interface byte TB(1) indicates FWI and SFGI timing parameters.
	 * Value is available when pointer is non-NULL. Otherwise value is absent.
	 * - High 4 bits encode Frame Waiting time Integer (FWI). Default is 4.
	 * - Low 4 bits encode Start-up Frame Guard time Integer (SFGI). Default is 0.
	 */
	const uint8_t* TB1;

	/**
	 * Interface byte TC(1) indicates whether CID and NAD are supported.
	 * Value is available when pointer is non-NULL. Otherwise value is absent.
	 * - Bit 2 indicates whether CID is supported
	 * - Bit 1 indicates whether NAD is supported
	 */
	const uint8_t* TC1;

	// ========================================
	// Historical byte parsing...
	// ========================================

	uint8_t K_count; ///< Number of historical bytes, including category indicator byte T1

	/**
	 * Category indicator byte T1 indicates the format of the historical bytes.
	 * Only valid when @ref K_count is non-zero.
	 * - 0x00: Subsequent historical bytes are COMPACT-TLV encoded followed by mandatory status indicator
	 * - 0x10: Subsequent historical byte is DIR data reference
	 * - 0x80: Subsequent historical bytes are COMPACT-TLV encoded and may include status indicator
	 * - 0x81-0x8F: RFU
	 * - Other values are proprietary
	 */
	uint8_t T1;

	const uint8_t* historical_bytes; ///< Historical byte payload after category indicator byte T1. NULL if absent.
	size_t historical_bytes_len; ///< Length of historical byte payload, excluding explicit status indicator

	/**
	 * Pointer to status indicator bytes. Available when pointer is non-NULL. NULL if absent.
	 * @see @ref status_indicator for extracted values
	 */
	const uint8_t* status_indicator_bytes;

	/**
	 * Number of status indicator bytes at @ref status_indicator_bytes
	 * @see @ref status_indicator for extracted values
	 */
	size_t status_indicator_bytes_len;

	// ========================================
	// Extracted info...
	// ========================================

	// Extracted from T0 (ISO/IEC 14443-4:2008, 5.2.3)
	unsigned int FSC; ///< Frame Size for Card in bytes; default is 32 bytes (FSCI=2)

	// Extracted from TA(1); defaults apply when TA(1) is absent (ISO/IEC 14443-4:2008, 5.2.4)
	bool same_d_required; ///< If true, only same divisor D is supported for both directions; default is false
	unsigned int DS; ///< Bitfield of supported divisors from PICC to PCD; default is @ref ISO14443_D_ONLY_1
	unsigned int DR; ///< Bitfield of supported divisors from PCD to PICC; default is @ref ISO14443_D_ONLY_1

	// Extracted from TB(1); defaults apply when TB(1) is absent (ISO/IEC 14443-4:2008, 5.2.5)
	unsigned int FWI;  ///< Frame Waiting time Integer; default is 4
	unsigned int SFGI; ///< Start-up Frame Guard time Integer; default is 0

	// Extracted from TC(1); defaults apply when TC(1) is absent (ISO/IEC 14443-4:2008, 5.2.6)
	bool CID_supported; ///< Card Identifier (CID) supported; default is true
	bool NAD_supported; ///< Node Address (NAD) supported; default is false

	struct {
		uint8_t LCS; ///< Card life cycle status; Zero if not available
		uint8_t SW1; ///< Status Word byte 1; If both SW1 and SW2 are zero, then status word is not available
		uint8_t SW2; ///< Status Word byte 2; If both SW1 and SW2 are zero, then status word is not available
	} status_indicator; ///< Status indicator bytes (ISO/IEC 7816-4:2005, 8.1.1.3)
};

/**
 * Parse ISO/IEC 14443 Answer To Select (ATS) message
 * @param ats ATS data (starting with mandatory TL byte)
 * @param ats_len Length of ATS data in bytes
 * @param ats_info Parsed ATS info output
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int iso14443_ats_parse(const uint8_t* ats, size_t ats_len, struct iso14443_ats_info_t* ats_info);

/**
 * Stringify ISO/IEC 14443 ATS format byte T0
 * @param ats_info Parsed ATS info
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return String. NULL for error.
 */
const char* iso14443_ats_T0_get_string(const struct iso14443_ats_info_t* ats_info, char* str, size_t str_len);

/**
 * Stringify ISO/IEC 14443 ATS interface byte TA(1)
 * @param ats_info Parsed ATS info
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return String. NULL for error.
 */
const char* iso14443_ats_TA1_get_string(const struct iso14443_ats_info_t* ats_info, char* str, size_t str_len);

/**
 * Stringify ISO/IEC 14443 ATS interface byte TB(1)
 * @param ats_info Parsed ATS info
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return String. NULL for error.
 */
const char* iso14443_ats_TB1_get_string(const struct iso14443_ats_info_t* ats_info, char* str, size_t str_len);

/**
 * Stringify ISO/IEC 14443 ATS interface byte TC(1)
 * @param ats_info Parsed ATS info
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return String. NULL for error.
 */
const char* iso14443_ats_TC1_get_string(const struct iso14443_ats_info_t* ats_info, char* str, size_t str_len);

/**
 * Stringify ISO/IEC 14443 ATS category indicator byte T1
 * @param ats_info Parsed ATS info
 * @return String. NULL for error.
 */
const char* iso14443_ats_T1_get_string(const struct iso14443_ats_info_t* ats_info);

/**
 * Parsed ATQB (Answer To Request Type B) information for ISO/IEC 14443 type B
 * cards.
 *
 * This structure represents the parsed and decoded ATQB information as defined
 * by the ISO/IEC 14443-3 protocol activation procedure (REQB/WUPB response).
 *
 * The ATQB is either 12 bytes (basic) or 13 bytes (extended, when Protocol
 * Info byte 4 is present). The leading byte is the mandatory 0x50 marker.
 */
struct iso14443_atqb_info_t {
	// Store ATQB bytes for field pointers to use
	uint8_t atqb[ISO14443_ATQB_MAX_SIZE]; ///< ATQB bytes
	size_t atqb_len; ///< Length of ATQB in bytes

	// ========================================
	// Raw format parsing...
	// ========================================

	/**
	 * Pseudo-Unique PICC Identifier (PUPI). Always 4 bytes.
	 */
	const uint8_t* pupi;

	/**
	 * Application Data field. Always 4 bytes.
	 * When Application Data Coding (ADC) indicates ISO/IEC 14443-3, the layout
	 * is:
	 * - Byte 1: Application Family Identifier (AFI)
	 * - Bytes 2-3: CRC_B(AID)
	 * - Byte 4: Number of Applications
	 */
	const uint8_t* application_data;

	/**
	 * Protocol Info byte 1 indicates bit rate capabilities of the PICC.
	 * - Bit 8: if set, only the same divisor D is supported for both directions
	 * - Bits 5-7 encode DS (divisor from PICC to PCD)
	 * - Bit 4 is RFU
	 * - Bits 1-3 encode DR (divisor from PCD to PICC)
	 */
	const uint8_t* PI1;

	/**
	 * Protocol Info byte 2 indicates frame and protocol parameters.
	 * - High 4 bits encode Max_Frame_Size (FSCI)
	 * - Low 4 bits encode Protocol_Type:
	 *   - Bit 4 is RFU
	 *   - Bits 2-3 encode minimum TR2
	 *   - Bit 1 indicates ISO/IEC 14443-4 compliant PICC
	 */
	const uint8_t* PI2;

	/**
	 * Protocol Info byte 3 indicates frame parameters.
	 * - High 4 bits encode Frame Waiting time Integer (FWI)
	 * - Bits 3-4 encode Application Data Coding (ADC)
	 * - Bits 1-2 encode Frame Options (FO): CID (bit 1), NAD (bit 2)
	 */
	const uint8_t* PI3;

	/**
	 * Protocol Info byte 4 is optional and indicates frame parameters.
	 * Non-NULL for extended ATQB. NULL for basic ATQB.
	 * - High 4 bits encode Start-up Frame Guard time Integer (SFGI)
	 * - Low 4 bits are RFU
	 */
	const uint8_t* PI4;

	// ========================================
	// Extracted info...
	// ========================================

	// Extracted from Application Data when ADC indicates ISO/IEC 14443-3 coding (ISO/IEC 14443-3:2011, 7.9.3)
	// All zero when ADC does not indicate ISO/IEC 14443-3 coding.
	uint8_t AFI; ///< Application Family Identifier
	uint16_t CRC_B_AID; ///< CRC_B computed over AID, in host byte order
	unsigned int num_apps_matching_afi; ///< Number of applications matching AFI; @ref ISO14443_NUM_APPS_MANY means 15 or more
	unsigned int num_apps_total; ///< Total number of applications in PICC; @ref ISO14443_NUM_APPS_MANY means 15 or more

	// Extracted from PI(1); defaults apply when PI(1) RFU bit is set (ISO/IEC 14443-3:2011, 7.9.4.6)
	bool same_d_required; ///< If true, only same divisor D is supported for both directions; default is false
	unsigned int DS; ///< Bitfield of supported divisors from PICC to PCD; default is @ref ISO14443_D_ONLY_1
	unsigned int DR; ///< Bitfield of supported divisors from PCD to PICC; default is @ref ISO14443_D_ONLY_1

	// Extracted from PI(2) (ISO/IEC 14443-3:2011, 7.9.4.4 and 7.9.4.5)
	unsigned int FSC; ///< Frame Size for Card in bytes
	bool protocol_type_rfu; ///< If true, PCD should not continue communicating with PICC
	unsigned int min_TR2; ///< Minimum delay between PICC EOF start and PCD SOF start (TR2)
	bool iso14443_4_compliant; ///< PICC compliant with ISO/IEC 14443-4

	// Extracted from PI(3) (ISO/IEC 14443-3:2011, 7.9.4.1 - 7.9.4.3)
	unsigned int FWI; ///< Frame Waiting time Integer
	unsigned int ADC; ///< Application Data Coding
	bool CID_supported; ///< Card Identifier (CID) supported
	bool NAD_supported; ///< Node Address (NAD) supported

	// Extracted from PI(4); defaults apply when PI(4) is absent (ISO/IEC 14443-3:2011, 7.9.4.7)
	unsigned int SFGI; ///< Start-up Frame Guard time Integer; default is 0
};

/**
 * Parse ISO/IEC 14443 Answer To Request Type B (ATQB) message
 * @param atqb ATQB data (starting with mandatory 0x50 marker byte)
 * @param atqb_len Length of ATQB data in bytes (12 for basic, 13 for extended)
 * @param atqb_info Parsed ATQB info output
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int iso14443_atqb_parse(const uint8_t* atqb, size_t atqb_len, struct iso14443_atqb_info_t* atqb_info);

/**
 * Stringify ISO/IEC 14443 ATQB Application Data
 * @param atqb_info Parsed ATQB info
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return String. NULL for error.
 */
const char* iso14443_atqb_application_data_get_string(const struct iso14443_atqb_info_t* atqb_info, char* str, size_t str_len);

/**
 * Stringify ISO/IEC 14443 ATQB Protocol Info byte 1 (Bit_Rate_Capability)
 * @param atqb_info Parsed ATQB info
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return String. NULL for error.
 */
const char* iso14443_atqb_PI1_get_string(const struct iso14443_atqb_info_t* atqb_info, char* str, size_t str_len);

/**
 * Stringify ISO/IEC 14443 ATQB Protocol Info byte 2 (Max_Frame_Size + Protocol_Type)
 * @param atqb_info Parsed ATQB info
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return String. NULL for error.
 */
const char* iso14443_atqb_PI2_get_string(const struct iso14443_atqb_info_t* atqb_info, char* str, size_t str_len);

/**
 * Stringify ISO/IEC 14443 ATQB Protocol Info byte 3 (FWI + ADC + FO)
 * @param atqb_info Parsed ATQB info
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return String. NULL for error.
 */
const char* iso14443_atqb_PI3_get_string(const struct iso14443_atqb_info_t* atqb_info, char* str, size_t str_len);

/**
 * Stringify ISO/IEC 14443 ATQB Protocol Info byte 4 (SFGI)
 * @param atqb_info Parsed ATQB info
 * @param str String buffer output
 * @param str_len Length of string buffer in bytes
 * @return String. NULL for error.
 */
const char* iso14443_atqb_PI4_get_string(const struct iso14443_atqb_info_t* atqb_info, char* str, size_t str_len);

__END_DECLS

#endif
