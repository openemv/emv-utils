/**
 * @file emv.c
 * @brief High level EMV library interface
 *
 * Copyright (c) 2023 Leon Lynch
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

#include "emv.h"
#include "emv_utils_config.h"

#include "iso7816.h"

#define EMV_DEBUG_SOURCE EMV_DEBUG_SOURCE_EMV
#include "emv_debug.h"

const char* emv_lib_version_string(void)
{
	return EMV_UTILS_VERSION_STRING;
}

int emv_atr_parse(const void* atr, size_t atr_len)
{
	int r;
	struct iso7816_atr_info_t atr_info;
	unsigned int TD1_protocol = 0; // Default is T=0
	unsigned int TD2_protocol = 0; // Default is T=0

	r = iso7816_atr_parse(atr, atr_len, &atr_info);
	if (r) {
		emv_debug_trace_msg("iso7816_atr_parse() failed; r=%d", r);
		emv_debug_error("Failed to parse ATR");
		return r;
	}

	// The intention of this function is to validate the ATR in accordance with
	// EMV Level 1 Contact Interface Specification v1.0, 8.3. Some of the
	// validation may already be performed by iso7816_atr_parse() and should be
	// noted below in comments. The intention is also not to limit this
	// function to only the "basic ATR", but instead to allow all possible ATRs
	// that are allowed by the specification.

	// TS - Initial character
	// See EMV Level 1 Contact Interface v1.0, 8.3.1
	// Validated by iso7816_atr_parse()

	// T0 - Format character
	// See EMV Level 1 Contact Interface v1.0, 8.3.2
	// Validated by iso7816_atr_parse()

	// TA1 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.1
	if (atr_info.TA[1]) { // TA1 is present
		if (atr_info.TA[2] && // TA2 is present
			(*atr_info.TA[2] & ISO7816_ATR_TA2_IMPLICIT) == 0 && // Specific mode
			(*atr_info.TA[1] < 0x11 || *atr_info.TA[1] > 0x13) // TA1 must be in the range 0x11 to 0x13
		) {
			emv_debug_error("TA2 indicates specific mode but TA1 is invalid");
			return 1;
		}

		if (!atr_info.TA[2]) { // TA2 is absent
			// Max frequency must be at least 5 MHz
			if ((*atr_info.TA[1] & ISO7816_ATR_TA1_FI_MASK) == 0) {
				emv_debug_error("TA2 indicates negotiable mode but TA1 is invalid");
				return 1;
			}

			// Baud rate adjustment factor must be at least 4
			if ((*atr_info.TA[1] & ISO7816_ATR_TA1_DI_MASK) < 3) {
				emv_debug_error("TA2 indicates negotiable mode but TA1 is invalid");
				return 1;
			}
		}
	}

	// TB1 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.2
	// Validated by iso7816_atr_parse()

	// TC1 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.3
	if (atr_info.TC[1]) { // TC1 is present
		// TC1 must be either 0x00 or 0xFF
		if (*atr_info.TC[1] != 0x00 && *atr_info.TC[1] != 0xFF) {
			emv_debug_error("TC1 is invalid");
			return 1;
		}
	}

	// TD1 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.4
	if (atr_info.TD[1]) { // TD1 is present
		// TD1 protocol type must be T=0 or T=1
		if ((*atr_info.TD[1] & ISO7816_ATR_Tx_OTHER_MASK) > ISO7816_PROTOCOL_T1) {
			emv_debug_error("TD1 protocol is invalid");
			return 1;
		}
		TD1_protocol = *atr_info.TD[1] & ISO7816_ATR_Tx_OTHER_MASK;
	}

	// TA2 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.5
	if (atr_info.TA[2]) { // TA2 is present
		unsigned int TA2_protocol;

		// TA2 protocol must be the same as the first indicated protocol
		TA2_protocol = *atr_info.TA[2] & ISO7816_ATR_TA2_PROTOCOL_MASK;
		if (TA2_protocol != TD1_protocol) {
			emv_debug_error("TA2 protocol differs from TD1 protocol");
			return 1;
		}

		// TA2 must indicate specific mode, not implicit mode
		if (*atr_info.TA[2] & ISO7816_ATR_TA2_IMPLICIT) {
			emv_debug_error("TA2 implicit mode is invalid");
			return 1;
		}
	}

	// TB2 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.6
	// Validated by iso7816_atr_parse()

	// TC2 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.7
	if (atr_info.TC[2]) { // TC2 is present
		// TC2 is specific to T=0
		if (TD1_protocol != ISO7816_PROTOCOL_T0) {
			emv_debug_error("TC2 is not allowed when protocol is not T=0");
			return 1;
		}

		// TC2 for T=0 must be 0x0A
		if (*atr_info.TC[2] != 0x0A) {
			emv_debug_error("TC2 for T=0 is invalid");
			return 1;
		}
	}

	// TD2 - Interface Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.3.8
	if (atr_info.TD[2]) { // TD2 is present
		// TD2 protocol type must be T=15 if TD1 protocol type was T=0
		if (TD1_protocol == ISO7816_PROTOCOL_T0 &&
			(*atr_info.TD[2] & ISO7816_ATR_Tx_OTHER_MASK) != ISO7816_PROTOCOL_T15
		) {
			emv_debug_error("TD2 protocol is invalid");
			return 1;
		}

		// TD2 protocol type must be T=1 if TD1 protocol type was T=1
		if (TD1_protocol == ISO7816_PROTOCOL_T1 &&
			(*atr_info.TD[2] & ISO7816_ATR_Tx_OTHER_MASK) != ISO7816_PROTOCOL_T1
		) {
			emv_debug_error("TD2 protocol is invalid");
			return 1;
		}

		TD2_protocol = *atr_info.TD[2] & ISO7816_ATR_Tx_OTHER_MASK;

	} else { // TD2 is absent
		// TB3, and therefore TD2, must be present for T=1
		// See EMV Level 1 Contact Interface v1.0, 8.3.3.10
		if (TD1_protocol == ISO7816_PROTOCOL_T1) {
			emv_debug_error("TD2 for T=1 is absent");
			return 1;
		}
	}

	// T=1 Interface Characters
	if (TD2_protocol == ISO7816_PROTOCOL_T1) {
		// TA3 - Interface Character
		// See EMV Level 1 Contact Interface v1.0, 8.3.3.9
		if (atr_info.TA[3]) { // TA3 is present
			// TA3 for T=1 must be in the range 0x10 to 0xFE
			// iso7816_atr_parse() already rejects 0xFF
			if (*atr_info.TA[3] < 0x10) {
				emv_debug_error("TA3 for T=1 is invalid");
				return 1;
			}
		}

		// TB3 - Interface Character
		// See EMV Level 1 Contact Interface v1.0, 8.3.3.10
		if (atr_info.TB[3]) { // TB3 is present
			// TB3 for T=1 BWI must be 4 or less
			if (((*atr_info.TB[3] & ISO7816_ATR_TBi_BWI_MASK) >> ISO7816_ATR_TBi_BWI_SHIFT) > 4) {
				emv_debug_error("TB3 for T=1 has invalid BWI");
				return 1;
			}

			// TB3 for T=1 CWI must be 5 or less
			if ((*atr_info.TB[3] & ISO7816_ATR_TBi_CWI_MASK) > 5) {
				emv_debug_error("TB3 for T=1 has invalid CWI");
				return 1;
			}

			// For T=1, reject 2^CWI < (N + 1)
			// - if N==0xFF, consider N to be -1
			// - if N==0x00, consider CWI to be 1
			// See EMV Level 1 Contact Interface v1.0, 8.3.3.1
			int N = (atr_info.global.N != 0xFF) ? atr_info.global.N : -1;
			unsigned int CWI = atr_info.global.N ? atr_info.protocol_T1.CWI : 1;
			unsigned int pow_2_CWI = 1 << CWI;
			if (pow_2_CWI < (N + 1)) {
				emv_debug_error("2^CWI < (N + 1) for T=1 is not allowed");
				return 1;
			}

		} else { // TB3 is absent
			emv_debug_error("TB3 for T=1 is absent");
			return 1;
		}

		// TC3 - Interface Character
		// See EMV Level 1 Contact Interface v1.0, 8.3.3.11
		if (atr_info.TC[3]) { // TC3 is present
			// TC for T=1 must be 0x00
			if (*atr_info.TC[3] != 0x00) {
				emv_debug_error("TC3 for T=1 is invalid");
				return 1;
			}
		}
	}

	// TCK - Check Character
	// See EMV Level 1 Contact Interface v1.0, 8.3.4
	// Validated by iso7816_atr_parse()

	return 0;
}
