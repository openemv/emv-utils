/**
 * @file iso7816.h
 * @brief ISO/IEC 7816 definitions and helper functions
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

#ifndef ISO7816_H
#define ISO7816_H

#include <sys/cdefs.h>
#include <stddef.h>
#include <stdint.h>

__BEGIN_DECLS

#define ISO7816_ATR_MIN_SIZE    (2) ///< Minimum size of ATR buffer
#define ISO7816_ATR_MAX_SIZE    (33) ///< Maximum size of ATR buffer

// ATR: Initial byte TS values
#define ISO7816_ATR_TS_DIRECT           (0x3B) ///< TS value for direct convention
#define ISO7816_ATR_TS_INVERSE          (0x3F) ///< TS value for inverse convention

// ATR: Interface byte definitions for T0 or TD[x]
#define ISO7816_ATR_Tx_OTHER_MASK       (0x0F) ///< T0 or TD[x] mask. When T0, for K value (number of historical bytes). When TD[x], for T value (protocol / global indicator)
#define ISO7816_ATR_Tx_PROTOCOL_T0      (0x00) ///< TD[x] K value indicating protocol T=0
#define ISO7816_ATR_Tx_PROTOCOL_T1      (0x01) ///< TD[x] K value indicating protocol T=1
#define ISO7816_ATR_Tx_GLOBAL           (0x0F) ///< TD[x] K value indicating T=15: the subsequent interface bytes are global interface bytes; Not allowed for TD[1]
#define ISO7816_ATR_Tx_TAi_PRESENT      (0x10) ///< T0 or TD[x] bit indicating interface byte TA(i=x+1) is present
#define ISO7816_ATR_Tx_TBi_PRESENT      (0x20) ///< T0 or TD[x] bit indicating interface byte TB(i=x+1) is present
#define ISO7816_ATR_Tx_TCi_PRESENT      (0x40) ///< T0 or TD[x] bit indicating interface byte TC(i=x+1) is present
#define ISO7816_ATR_Tx_TDi_PRESENT      (0x80) ///< T0 or TD[x] bit indicating interface byte TD(i=x+1) is present

// ATR: Historical byte definitions
#define ISO7816_ATR_T1_COMPACT_TLV_SI   (0x00) ///< Subsequent historical bytes are COMPACT-TLV encoded followed by mandatory status indicator
#define ISO7816_ATR_T1_DIR_DATA_REF     (0x10) ///< Subsequent historical byte is DIR data reference
#define ISO7816_ATR_T1_COMPACT_TLV      (0x80) ///< Subsequent historical bytes are COMPACT-TLV encoded and many include status indicator

struct iso7816_atr_info_t {
	/**
	 * Initial character TS indicates bit order and polarity.
	 * - 0x3B: Direct convention
	 * - 0x3F: Inverse convention
	 */
	uint8_t TS;

	uint8_t T0; ///< Format byte T0 indicates ATR format

	// Store ATR bytes for below pointers to use
	uint8_t atr[ISO7816_ATR_MAX_SIZE]; ///< ATR bytes
	size_t atr_len; ///< Length of ATR in bytes

	// ========================================
	// Interface byte parsing...
	// ========================================

	/**
	 * Interface bytes TA[x]. Value is available when pointer is non-NULL. Otherwise value is absent.
	 * - Interface byte TA[1] indicates maximum clock frequency and clock periods per ETU; default is 0x11 if absent
	 * - Interface byte TA[2] indicates that reader should use specific mode as indicated by earlier global interface bytes, instead of negotiable mode
	 * - Further interface bytes TA[x>2] indicate the maximum receive block size (if protocol T=1)
	 *   or supported supply voltages and low power modes (if global T=15)
	 */
	const uint8_t* TA[5];

	/**
	 * Interface bytes TB[x]. Value is available when pointer is non-NULL. Otherwise value is absent.
	 * - Interface byte TB[1] indicates coarse Vpp voltage (PI1); deprecated and should be ignored by reader; nonetheless required by EMV to be 0x00
	 * - Interface byte TB2[2] indicates precise Vpp voltage (PI2); deprecated and should be ignored by reader
	 * - Further interface bytes TB[x>2] indicate the maximum delay between characters (if protocol T=1)
	 *   or use of SPU contact C6 (if global T=15)
	 */
	const uint8_t* TB[5];

	/**
	 * Interface bytes TC[x]. Value is available when pointer is non-NULL. Otherwise value is absent.
	 * - Interface byte TC[1] indicates Extra Guard Time Integer (N)
	 * - Interface byte TC[2] indicates Work Waiting Time (WI) for protocol T=0; default is 0xA if absent
	 * - Further interface bytes TC[x>2] indicate the error detection code used (if protocol T=1)
	 */
	const uint8_t* TC[5];

	/**
	 * Interface bytes TD[x] indicates card protocol, presence of subsequent interface bytes,
	 * and whether they are global or specific to the indicates protocol.
	 * Value is available when pointer is non-NULL. Otherwise value is absent.
	 * - Interface byte TD[1] indicates primary card protocol and subsequent global interface bytes
	 * - Further interface bytes TD[x>2] indicate additional supported card protocols, their
	 *   associated specific interface bytes, or additional global interface bytes
	 */
	const uint8_t* TD[5];

	// ========================================
	// Historical byte parsing...
	// ========================================

	uint8_t K_count; ///< Number of historical bytes

	/**
	 * Category indicator byte T1 indicates format of historical bytes
	 * - 0x00: Subsequent historical bytes are COMPACT-TLV encoded followed by mandatory status indicator
	 * - 0x10: Subsequent historical byte is DIR data reference
	 * - 0x80: Subsequent historical bytes are COMPACT-TLV encoded and many include status indicator
	 * - 0x81-0x8F: RFU
	 * - Other values are proprietary
	 */
	uint8_t T1;

	const uint8_t* historical_payload; ///< Historical byte payload after category indicator byte T1. NULL if absent.
	size_t historical_payload_len; ///< Length of historical byte payload, excluding explicit status indicator

	/**
	 * Status indicator. Available when pointer is non-NULL. NULL if absent.
	 * @see @ref status_indicator for values
	 */
	const uint8_t* status_indicator_bytes;

	uint8_t TCK; ///< Check character. Not available when T=0 is the only available protocol. Otherwise mandatory.

	// ========================================
	// Extracted info...
	// ========================================

	struct {
		uint8_t LCS; ///< Card life cycle status; Zero if not available
		uint8_t SW1; ///< Status Word byte 1; If both SW1 and SW2 are zero, then status word is not avaiable
		uint8_t SW2; ///< Status Word byte 2; If both SW1 and SW2 are zero, then status word is not avaiable
	} status_indicator;
};

/**
 * Parse ISO/IEC 7816 Answer To Reset (ATR) message
 * @param atr ATR data
 * @param atr_len Length of ATR data
 * @param atr_info Parsed ATR info
 * @return Zero for success. Less than zero for internal error. Greater than zero for parse error.
 */
int iso7816_atr_parse(const uint8_t* atr, size_t atr_len, struct iso7816_atr_info_t* atr_info);

/**
 * Stringify ISO/IEC 7816 ATR initial character TS
 * @param atr_info Parsed ATR info
 * @return String. NULL for error.
 */
const char* iso7816_atr_TS_get_string(const struct iso7816_atr_info_t* atr_info);

__END_DECLS

#endif
