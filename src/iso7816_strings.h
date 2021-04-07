/**
 * @file iso7816_strings.h
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

#ifndef ISO7816_STRINGS_H
#define ISO7816_STRINGS_H

#include <sys/cdefs.h>
#include <stdint.h>

__BEGIN_DECLS

// Life cycle status bits
#define ISO7816_LCS_NONE                        (0x00) ///< No life cycle information
#define ISO7816_LCS_CREATION                    (0x01) ///< Creation state
#define ISO7816_LCS_INITIALISATION              (0x03) ///< Initialisation state
#define ISO7816_LCS_OPERATIONAL_MASK            (0xFD) ///< Operational state bitmask
#define ISO7816_LCS_ACTIVATED                   (0x05) ///< Operational state: activated
#define ISO7816_LCS_DEACTIVATED                 (0x04) ///< Operational state: deactivated
#define ISO7816_LCS_TERMINATION_MASK            (0xFC) ///< Termination state bitmask
#define ISO7816_LCS_TERMINATION                 (0x0C) ///< Termination state

/**
 * Stringify ISO/IEC 7816 life cycle status byte (LCS)
 * @param lcs Life cycle status byte (LCS)
 * @return String. NULL for error.
 */
const char* iso7816_lcs_get_string(uint8_t lcs);

__END_DECLS

#endif
