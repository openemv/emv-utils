/**
 * @file pcsc_compat.h
 * @brief PC/SC compatibility helpers
 *
 * Copyright (c) 2024 Leon Lynch
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

#ifndef PCSC_COMPAT_H
#define PCSC_COMPAT_H

#include <sys/cdefs.h>
#include <stdint.h>
#include <stdio.h>

__BEGIN_DECLS

#if !defined(SCARD_CTL_CODE) && defined(CTL_CODE)
// Not all implementations define SCARD_CTL_CODE but it can be derived
// from CTL_CODE when available
#define SCARD_CTL_CODE(code) CTL_CODE(FILE_DEVICE_SMARTCARD, (code), METHOD_BUFFERED, FILE_ANY_ACCESS)
#endif

// See PC/SC Part 10 Rev 2.02.09, 2.2
#ifndef CM_IOCTL_GET_FEATURE_REQUEST
#define CM_IOCTL_GET_FEATURE_REQUEST SCARD_CTL_CODE(3400)
#endif

// See PC/SC Part 10 Rev 2.02.09, 2.3
#define PCSC_FEATURE_IFD_PIN_PROPERTIES         (0x0A) ///< Interface Device (IFD) PIN handling properties
#define PCSC_FEATURE_IFD_DISPLAY_PROPERTIES     (0x11) ///< Interface Device (IFD) display properties
#define PCSC_FEATURE_GET_TLV_PROPERTIES         (0x12) ///< Interface Device (IFD) properties in Tag-Length-Value (TLV) form

#ifdef USE_PCSCLITE

#define PCSC_MAX_BUFFER_SIZE (MAX_BUFFER_SIZE)

#else // !USE_PCSCLITE

#define PCSC_MAX_BUFFER_SIZE (264)

// See PC/SC Part 10 Rev 2.02.09, 2.2
typedef struct
{
	uint8_t tag;
	uint8_t length;
	uint32_t value;
} __attribute__((packed))
PCSC_TLV_STRUCTURE;

// See PC/SC Part 10 Rev 2.02.09, 2.6.8
typedef struct {
	uint16_t wLcdLayout;
	uint8_t bEntryValidationCondition;
	uint8_t bTimeOut2;
} __attribute__((packed))
PIN_PROPERTIES_STRUCTURE;

// Only PCSCLite provides pcsc_stringify_error()
static const char* pcsc_stringify_error(unsigned int result)
{
	static char str[16];
	snprintf(str, sizeof(str), "0x%08X", result);
	return str;
}

#endif // USE_PCSCLITE

// PCSCLite does not provide DISPLAY_PROPERTIES_STRUCTURE
// See PC/SC Part 10 Rev 2.02.09, 2.6.9
typedef struct {
	uint16_t wLcdMaxCharacters;
	uint16_t wLcdMaxLines;
} __attribute__((packed))
DISPLAY_PROPERTIES_STRUCTURE;

__END_DECLS

#endif
