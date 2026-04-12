/**
 * @file emv_config_xml.h
 * @brief EMV configuration XML loading functions
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

#ifndef EMV_CONFIG_XML_H
#define EMV_CONFIG_XML_H

#include <sys/cdefs.h>
#include <stddef.h>

__BEGIN_DECLS

// Forward declarations
struct emv_ctx_t;

/**
 * @brief EMV XML configuration errors
 *
 * These indicate errors related to parsing of the EMV XML configuration data
 * and must have positive values.
 */
enum emv_config_xml_error_t {
	EMV_CONFIG_XML_PARSE_ERROR = 1, ///< XML parse or structure error
	EMV_CONFIG_XML_INVALID_DATA = 2, ///< Invalid field value in XML
};

/**
 * Load EMV configuration from XML file.
 *
 * This function can be used after @ref emv_ctx_init() and before EMV
 * processing to populate the application independent data and application
 * dependent data in @ref emv_config_t from XML data.
 *
 * @param ctx EMV processing context
 * @param filename Path to XML configuration file
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for parse/validation errors. See @ref emv_config_xml_error_t
 */
int emv_config_xml_load(struct emv_ctx_t* ctx, const char* filename);

/**
 * Load EMV configuration from XML buffer.
 *
 * This function can be used after @ref emv_ctx_init() and before EMV
 * processing to populate the application independent data and application
 * dependent data in @ref emv_config_t from XML data.
 *
 * @param ctx EMV processing context
 * @param buf XML data buffer
 * @param len Length of @p buf in bytes (excluding NULL termination)
 *
 * @return Zero for success
 * @return Less than zero for errors. See @ref emv_error_t
 * @return Greater than zero for parse/validation errors. See @ref emv_config_xml_error_t
 */
int emv_config_xml_load_buf(struct emv_ctx_t* ctx, const void* buf, size_t len);

__END_DECLS

#endif
