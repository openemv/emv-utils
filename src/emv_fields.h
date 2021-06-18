/**
 * @file emv_fields.h
 * @brief EMV field definitions
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

#ifndef EMV_FIELDS_H
#define EMV_FIELDS_H

#include <sys/cdefs.h>

__BEGIN_DECLS

// Application Priority Indicator
// See EMV 4.3 Book 1, 12.2.3, table 48
#define EMV_APP_PRIORITY_INDICATOR_MASK                         (0x0F)
#define EMV_APP_PRIORITY_INDICATOR_CONF_REQUIRED                (0x80)

// Application Selection Indicator
// See EMV 4.3 Book 1, 12.3.1
#define EMV_ASI_EXACT_MATCH                                     (0x00)
#define EMV_ASI_PARTIAL_MATCH                                   (0x01)

// Terminal Capabilities (field 9F33) byte 1
// See EMV 4.3 Book 4, Annex A2, table 25
#define EMV_TERM_CAPS_INPUT_MANUAL_KEY_ENTRY                    (0x80) ///< Card Data Input Capability: Manual key entry
#define EMV_TERM_CAPS_INPUT_MAGNETIC_STRIPE                     (0x40) ///< Card Data Input Capability: Magnetic stripe
#define EMV_TERM_CAPS_INPUT_IC_WITH_CONTACTS                    (0x20) ///< Card Data Input Capability: IC with contacts
#define EMV_TERM_CAPS_INPUT_RFU                                 (0x1F) ///< Card Data Input Capability: RFU

// Terminal Capabilities (field 9F33) byte 2
// See EMV 4.3 Book 4, Annex A2, table 26
#define EMV_TERM_CAPS_CVM_PLAINTEXT_PIN_OFFLINE                 (0x80) ///< CVM Capability: Plaintext PIN for ICC verification
#define EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_ONLINE                 (0x40) ///< CVM Capability: Enciphered PIN for online verification
#define EMV_TERM_CAPS_CVM_SIGNATURE                             (0x20) ///< CVM Capability: Signature (paper)
#define EMV_TERM_CAPS_CVM_ENCIPHERED_PIN_OFFLINE                (0x10) ///< CVM Capability: Enciphered PIN for offline verification
#define EMV_TERM_CAPS_CVM_NO_CVM                                (0x08) ///< CVM Capability: No CVM required
#define EMV_TERM_CAPS_CVM_RFU                                   (0x07) ///< CVM Capability: RFU

// Terminal Capabilities (field 9F33) byte 3
// See EMV 4.3 Book 4, Annex A2, table 27
#define EMV_TERM_CAPS_SECURITY_SDA                              (0x80) ///< Security Capability: Static Data Authentication (SDA)
#define EMV_TERM_CAPS_SECURITY_DDA                              (0x40) ///< Security Capability: Dynamic Data Authentication (DDA)
#define EMV_TERM_CAPS_SECURITY_CARD_CAPTURE                     (0x20) ///< Security Capability: Card capture
#define EMV_TERM_CAPS_SECURITY_CDA                              (0x08) ///< Security Capability: Combined DDA/Application Cryptogram Generation (CDA)
#define EMV_TERM_CAPS_SECURITY_RFU                              (0x17) ///< Security Capability: RFU

__END_DECLS

#endif
