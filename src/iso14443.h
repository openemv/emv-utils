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

#define ISO14443_ATS_MIN_SIZE   (1)  ///< Minimum size of ATS buffer
#define ISO14443_ATS_MAX_SIZE   (20) ///< Maximum size of ATS buffer

// ATS: Interface byte TA(1) definitions (ISO/IEC 14443-4:2008, 5.2.4)
#define ISO14443_ATS_TA1_SAME_D         (0x80) ///< TA(1) bit 8 indicates only same divisor D for both directions is supported
#define ISO14443_ATS_TA1_DS_MASK        (0x70) ///< TA(1) bits 5-7 mask for DS (divisor from PICC to PCD)
#define ISO14443_ATS_TA1_DS_SHIFT       (4)    ///< TA(1) bitshift to normalise DS to bits 2-0
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
	 * Format byte T0 indicates the presence of interface bytes and the FSCI value.
	 * Value is available when pointer is non-NULL. Otherwise value is absent.
	 * - Bit 8 is RFU
	 * - Bit 7 indicates presence of interface byte TC(1)
	 * - Bit 6 indicates presence of interface byte TB(1)
	 * - Bit 5 indicates presence of interface byte TA(1)
	 * - Low 4 bits encode FSCI (Frame Size Card Integer); default is 2
	 */
	const uint8_t* T0;

	/**
	 * Interface byte TA(1) conveys the bit rate capabilities of the PICC.
	 * Value is available when pointer is non-NULL. Otherwise value is absent.
	 * - Bit 8: if set, only the same divisor D is supported for both directions
	 * - Bits 5-7 encode DS (divisor from PICC to PCD)
	 * - Bit 4 is RFU
	 * - Bits 1-3 encode DR (divisor from PCD to PICC)
	 * @see same_d_required, DS, DR for extracted values
	 */
	const uint8_t* TA1;

	/**
	 * Interface byte TB(1) conveys FWI and SFGI timing parameters.
	 * Value is available when pointer is non-NULL. Otherwise value is absent.
	 * - High 4 bits encode FWI (Frame Waiting time Integer); default is 4
	 * - Low 4 bits encode SFGI (Start-up Frame Guard time Integer); default is 0
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

	unsigned int FSC; ///< Frame Size for Card in bytes; default is 32 bytes (FSCI=2) (ISO/IEC 14443-4:2008, 5.2.3)

	// Extracted from TA(1); defaults apply when TA(1) is absent (ISO/IEC 14443-4:2008, 5.2.4)
	bool same_d_required; ///< If true, only same divisor D is supported for both directions; default is false
	unsigned int DS; ///< Bitfield of supported divisors from PICC to PCD; default is @ref ISO14443_D_ONLY_1
	unsigned int DR; ///< Bitfield of supported divisors from PCD to PICC; default is @ref ISO14443_D_ONLY_1

	unsigned int FWI;  ///< Frame Waiting time Integer; default is 4 (ISO/IEC 14443-4:2008, 5.2.5)
	unsigned int SFGI; ///< Start-up Frame Guard time Integer; default is 0 (ISO/IEC 14443-4:2008, 5.2.5)
	bool CID_supported; ///< Card Identifier (CID) supported; default is true (ISO/IEC 14443-4:2008, 5.2.6)
	bool NAD_supported; ///< Node Address (NAD) supported; default is false (ISO/IEC 14443-4:2008, 5.2.6)

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

__END_DECLS

#endif
