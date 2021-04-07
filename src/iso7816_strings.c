/**
 * @file iso7816_strings.c
 * @brief ISO/IEC 7816 string helper functions
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

#include "iso7816_strings.h"

const char* iso7816_lcs_get_string(uint8_t lcs)
{
	// See ISO 7816-4:2005, 5.3.3.2, table 13

	if (lcs == ISO7816_LCS_NONE) {
		return "No information given";
	}

	if (lcs == ISO7816_LCS_CREATION) {
		return "Creation state";
	}

	if (lcs == ISO7816_LCS_INITIALISATION) {
		return "Initialisation state";
	}

	if ((lcs & ISO7816_LCS_OPERATIONAL_MASK) == ISO7816_LCS_ACTIVATED) {
		return "Operational state (activated)";
	}

	if ((lcs & ISO7816_LCS_OPERATIONAL_MASK) == ISO7816_LCS_DEACTIVATED) {
		return "Operational state (deactivated)";
	}

	if ((lcs & ISO7816_LCS_TERMINATION_MASK) == ISO7816_LCS_TERMINATION) {
		return "Termination state";
	}

	return "Proprietary";
}
