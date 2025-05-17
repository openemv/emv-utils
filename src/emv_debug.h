/**
 * @file emv_debug.h
 * @brief EMV debug implementation
 *
 * Copyright 2021, 2023, 2025 Leon Lynch
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

#ifndef EMV_DEBUG_H
#define EMV_DEBUG_H

#include <sys/cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

/// Debug event source
enum emv_debug_source_t {
	EMV_DEBUG_SOURCE_NONE = 0x00,               ///< Source: None. Can be passed to @ref emv_debug_init().
	EMV_DEBUG_SOURCE_TTL = 0x01,                ///< Source: Terminal Transport Layer (TTL)
	EMV_DEBUG_SOURCE_TAL = 0x02,                ///< Source: Terminal Application Layer (TAL)
	EMV_DEBUG_SOURCE_EMV = 0x04,                ///< Source: EMV kernel
	EMV_DEBUG_SOURCE_APP = 0x08,                ///< Source: Application
	EMV_DEBUG_SOURCE_ALL = 0xFF,                ///< Source: All. Can be passed to @ref emv_debug_init().
};

/// Debug event level in descending order of importance
enum emv_debug_level_t {
	EMV_DEBUG_NONE = 0,                         ///< No events. Can be passed to @ref emv_debug_init().
	EMV_DEBUG_ERROR,                            ///< Error event
	EMV_DEBUG_INFO,                             ///< Info event
	EMV_DEBUG_CARD,                             ///< Card event
	EMV_DEBUG_TRACE,                            ///< Software trace
	EMV_DEBUG_ALL,                              ///< All events. Can be passed to @ref emv_debug_init().
};

/// Debug event content type
enum emv_debug_type_t {
	EMV_DEBUG_TYPE_MSG = 1,                     ///< Debug event contains only a string message and no data
	EMV_DEBUG_TYPE_DATA,                        ///< Debug event contains binary data
	EMV_DEBUG_TYPE_TLV,                         ///< Debug event contains ISO 8825-1 BER encoded data
	EMV_DEBUG_TYPE_ATR,                         ///< Debug event contains ISO 7816 Answer To Reset (ATR) data
	EMV_DEBUG_TYPE_CAPDU,                       ///< Debug event contains ISO 7816 C-APDU (command APDU) data
	EMV_DEBUG_TYPE_RAPDU,                       ///< Debug event contains ISO 7816 R-APDU (response APDU) data
	EMV_DEBUG_TYPE_CTPDU,                       ///< Debug event contains ISO 7816 C-TPDU (request TPDU) data
	EMV_DEBUG_TYPE_RTPDU,                       ///< Debug event contains ISO 7816 R-TPDU (response TPDU) data
};

/**
 * Debug event function signature
 *
 * @param timestamp 32-bit microsecond timestamp value
 * @param source Debug event source
 * @param level Debug event level
 * @param debug_type Debug event type
 * @param str Debug event string
 * @param buf Debug event data
 * @param buf_len Length of debug event data in bytes
 */
typedef void (*emv_debug_func_t)(
	unsigned int timestamp,
	enum emv_debug_source_t source,
	enum emv_debug_level_t level,
	enum emv_debug_type_t debug_type,
	const char* str,
	const void* buf,
	size_t buf_len
);

/**
 * Initialise debug event function
 *
 * @param sources_mask Bitmask of debug sources to pass to event function. See @ref emv_debug_source_t
 * @param level Maximum debug level event to pass to event function
 * @param func Callback function to use for debug events
 */
int emv_debug_init(
	unsigned int sources_mask,
	enum emv_debug_level_t level,
	emv_debug_func_t func
);

/**
 * Internal debugging implementation used by macros. Callers should use the
 * macros instead.
 *
 * @param source Debug event source
 * @param level Debug event level
 * @param debug_type Debug event type
 * @param fmt Debug event format string (printf-style)
 * @param buf Debug event data
 * @param buf_len Length of debug event data in bytes
 * @param ... Variable arguments for @c fmt
 */
void emv_debug_internal(
	enum emv_debug_source_t source,
	enum emv_debug_level_t level,
	enum emv_debug_type_t debug_type,
	const char* fmt,
	const void* buf,
	size_t buf_len,
	...
)
#if defined(__clang__)
	// Check for Clang first because it also defines __GNUC__
	__attribute__((format(printf, 4, 7)));
#elif defined(__GNUC__)
	// Use gnu_printf for GCC and MSYS2's MinGW
	__attribute__((format(gnu_printf, 4, 7)));
#else
	// Otherwise no format string checks
	;
#endif

#ifdef EMV_DEBUG_ENABLED

/**
 * Emit debug error message
 *
 * @param fmt Format string (printf-style)
 * @param ... Variable arguments for @c fmt
 */
#define emv_debug_error(fmt, ...) do { emv_debug_internal(EMV_DEBUG_SOURCE, EMV_DEBUG_ERROR, EMV_DEBUG_TYPE_MSG, fmt, NULL, 0, ##__VA_ARGS__); } while (0)

/**
 * Emit debug info message
 *
 * @param fmt Format string (printf-style)
 * @param ... Variable arguments for @c fmt
 */
#define emv_debug_info(fmt, ...) do { emv_debug_internal(EMV_DEBUG_SOURCE, EMV_DEBUG_INFO, EMV_DEBUG_TYPE_MSG, fmt, NULL, 0, ##__VA_ARGS__); } while (0)

/**
 * Emit debug info message with binary data
 *
 * @param fmt Format string (printf-style)
 * @param buf Debug event data
 * @param buf_len Length of debug event data in bytes
 * @param ... Variable arguments for @c fmt
 */
#define emv_debug_info_data(fmt, buf, buf_len, ...) do { emv_debug_internal(EMV_DEBUG_SOURCE, EMV_DEBUG_INFO, EMV_DEBUG_TYPE_DATA, fmt, buf, buf_len, ##__VA_ARGS__); } while (0)

/**
 * Emit debug info message with ISO 8825-1 BER encoded (TLV) data
 *
 * @param fmt Format string (printf-style)
 * @param buf BER encoded data
 * @param buf_len Length of BER encoded data in bytes
 * @param ... Variable arguments for @c fmt
 */
#define emv_debug_info_tlv(fmt, buf, buf_len, ...) do { emv_debug_internal(EMV_DEBUG_SOURCE, EMV_DEBUG_INFO, EMV_DEBUG_TYPE_TLV, fmt, buf, buf_len, ##__VA_ARGS__); } while (0)

#else // EMV_DEBUG_ENABLED
#define emv_debug_error(fmt, ...) do {} while (0)
#define emv_debug_info(fmt, ...) do {} while (0)
#define emv_debug_info_data(fmt, buf, buf_len, ...) do {} while (0)
#define emv_debug_info_tlv(fmt, buf, buf_len, ...) do {} while (0)
#endif // EMV_DEBUG_ENABLED

#if defined(EMV_DEBUG_ENABLED) && !defined(EMV_DEBUG_CARD_DISABLED)

/**
 * Emit debug event with decoded ISO 7816 ATR
 *
 * @param atr_info Pointer to parsed ATR info (@ref iso7816_atr_info_t)
 */
#define emv_debug_atr_info(atr_info) do { emv_debug_internal(EMV_DEBUG_SOURCE, EMV_DEBUG_CARD, EMV_DEBUG_TYPE_ATR, "ATR", atr_info, sizeof(*atr_info)); } while (0)

#else // EMV_DEBUG_ENABLED && !EMV_DEBUG_CARD_DISABLED
#define emv_debug_atr_info(atr_info) do {} while (0)
#endif // EMV_DEBUG_ENABLED && !EMV_DEBUG_CARD_DISABLED

#if defined(EMV_DEBUG_ENABLED) && !defined(EMV_DEBUG_CARD_DISABLED) && !defined(EMV_DEBUG_APDU_DISABLED)

/**
 * Emit debug APDU message
 *
 * @param fmt Format string (printf-style)
 * @param ... Variable arguments for @c fmt
 */
#define emv_debug_apdu(fmt, ...) do { emv_debug_internal(EMV_DEBUG_SOURCE, EMV_DEBUG_CARD, EMV_DEBUG_TYPE_MSG, fmt, NULL, 0, ##__VA_ARGS__); } while (0)

/**
 * Emit debug event with ISO 7816 C-APDU (command APDU) data
 *
 * @param buf C-APDU data
 * @param buf_len Length of C-APDU data in bytes
 */
#define emv_debug_capdu(buf, buf_len) do { emv_debug_internal(EMV_DEBUG_SOURCE, EMV_DEBUG_CARD, EMV_DEBUG_TYPE_CAPDU, "C-APDU", buf, buf_len); } while (0)

/**
 * Emit debug event with ISO 7816 R-APDU (response APDU) data
 *
 * @param buf R-APDU data
 * @param buf_len Length of R-APDU data in bytes
 */
#define emv_debug_rapdu(buf, buf_len) do { emv_debug_internal(EMV_DEBUG_SOURCE, EMV_DEBUG_CARD, EMV_DEBUG_TYPE_RAPDU, "R-APDU", buf, buf_len); } while (0)

#else // EMV_DEBUG_ENABLED && !EMV_DEBUG_CARD_DISABLED && !EMV_DEBUG_APDU_DISABLED
#define emv_debug_apdu(fmt, ...) do {} while (0)
#define emv_debug_capdu(buf, buf_len) do {} while (0)
#define emv_debug_rapdu(buf, buf_len) do {} while (0)
#endif // EMV_DEBUG_ENABLED && !EMV_DEBUG_CARD_DISABLED && !EMV_DEBUG_APDU_DISABLED

#if defined(EMV_DEBUG_ENABLED) && !defined(EMV_DEBUG_CARD_DISABLED) && !defined(EMV_DEBUG_TPDU_DISABLED)

/**
 * Emit debug event with ISO 7816 C-TPDU (request TPDU) data
 *
 * @param buf C-TPDU data
 * @param buf_len Length of C-TPDU data in bytes
 */
#define emv_debug_ctpdu(buf, buf_len) do { emv_debug_internal(EMV_DEBUG_SOURCE, EMV_DEBUG_CARD, EMV_DEBUG_TYPE_CTPDU, "C-TPDU", buf, buf_len); } while (0)

/**
 * Emit debug event with ISO 7816 R-TPDU (response TPDU) data
 *
 * @param buf R-TPDU data
 * @param buf_len Length of R-TPDU data in bytes
 */
#define emv_debug_rtpdu(buf, buf_len) do { emv_debug_internal(EMV_DEBUG_SOURCE, EMV_DEBUG_CARD, EMV_DEBUG_TYPE_RTPDU, "R-TPDU", buf, buf_len); } while (0)

#else // EMV_DEBUG_ENABLED && !EMV_DEBUG_CARD_DISABLED && !EMV_DEBUG_TPDU_DISABLED
#define emv_debug_ctpdu(buf, buf_len) do {} while (0)
#define emv_debug_rtpdu(buf, buf_len) do {} while (0)
#endif // EMV_DEBUG_ENABLED && !EMV_DEBUG_CARD_DISABLED && !EMV_DEBUG_TPDU_DISABLED

#if defined(EMV_DEBUG_ENABLED) && !defined(EMV_DEBUG_TRACE_DISABLED)

/**
 * Emit debug trace message
 *
 * @param fmt Format string (printf-style)
 * @param ... Variable arguments for @c fmt
 */
#define emv_debug_trace_msg(fmt, ...) do { emv_debug_internal(EMV_DEBUG_SOURCE, EMV_DEBUG_TRACE, EMV_DEBUG_TYPE_MSG, "%s[%u]: "fmt, NULL, 0, __FILE__, __LINE__, ##__VA_ARGS__); } while (0)

/**
 * Emit debug trace data
 *
 * @param fmt Format string (printf-style)
 * @param buf Debug event data
 * @param buf_len Length of debug event data in bytes
 * @param ... Variable arguments for @c fmt
 */
#define emv_debug_trace_data(fmt, buf, buf_len, ...) do { emv_debug_internal(EMV_DEBUG_SOURCE, EMV_DEBUG_TRACE, EMV_DEBUG_TYPE_DATA, fmt, buf, buf_len, ##__VA_ARGS__); } while (0)

#else // EMV_DEBUG_ENABLED && !EMV_DEBUG_TRACE_DISABLED
#define emv_debug_trace_msg(...) do {} while (0)
#define emv_debug_trace_data(...) do {} while (0)
#endif // EMV_DEBUG_ENABLED && !EMV_DEBUG_TRACE_DISABLED

__END_DECLS

#endif
