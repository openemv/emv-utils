/**
 * @file emv_config_xml.c
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

#include "emv_config_xml.h"
#include "emv.h"
#include "emv_tlv.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Maximum value length for a single TLV field in bytes
#define EMV_CONFIG_XML_MAX_VALUE_LEN (256)

static int parse_hex(const char* hex, void* buf, unsigned int* buf_len)
{
	unsigned int max_buf_len;

	if (!buf_len) {
		return -1;
	}
	max_buf_len = *buf_len;
	*buf_len = 0;

	while (*hex && max_buf_len) {
		uint8_t* ptr = buf;
		char str[3];
		unsigned int str_idx = 0;

		// Find next two valid hex digits
		while (*hex && str_idx < 2) {
			// Skip spaces
			if (isspace((unsigned char)*hex)) {
				++hex;
				continue;
			}
			// Only allow hex digits
			if (!isxdigit((unsigned char)*hex)) {
				return -2;
			}

			str[str_idx++] = *hex;
			++hex;
		}
		if (!str_idx) {
			// No hex digits left
			continue;
		}
		if (str_idx != 2) {
			// Uneven number of hex digits
			return -3;
		}
		str[2] = 0;

		*ptr = strtoul(str, NULL, 16);
		++buf;
		++*buf_len;
		--max_buf_len;
	}

	if (*hex && !max_buf_len) {
		// Insufficient space in output buffer
		return -4;
	}

	return 0;
}

static xmlChar* xml_str_trim(xmlChar* str)
{
	xmlChar* start;
	xmlChar* end;

	if (!str) {
		return str;
	}

	// Find first non-space character
	start = str;
	while (*start && isspace((unsigned char)*start)) {
		++start;
	}

	// Terminate after last non-space character
	end = start + xmlStrlen(start);
	while (end > start && isspace((unsigned char)*(end - 1))) {
		--end;
	}
	*end = 0;

	return start;
}

static int parse_tlv_content(
	const xmlChar* content,
	uint8_t* buf,
	unsigned int* buf_len
)
{
	int r;
	unsigned int content_len = xmlStrlen(content);

	if (content_len >= 2 &&
		content[0] == '"' &&
		content[content_len - 1] == '"'
	) {
		unsigned int str_len;

		// Parse content as string
		str_len = content_len - 2;
		if (str_len > *buf_len) {
			return EMV_CONFIG_XML_INVALID_DATA;
		}
		memcpy(buf, content + 1, str_len);
		*buf_len = str_len;

	} else {
		// Parse content as ASCII-HEX
		r = parse_hex((const char*)content, buf, buf_len);
		if (r) {
			return EMV_CONFIG_XML_INVALID_DATA;
		}
	}

	return 0;
}

static int parse_tlv_node(xmlNode* tlv_node, struct emv_tlv_list_t* list)
{
	int r;
	xmlChar* id_attr;
	char* endptr;
	unsigned long tag;
	xmlChar* content;
	xmlChar* trimmed;
	uint8_t value[EMV_CONFIG_XML_MAX_VALUE_LEN];
	unsigned int value_len;

	// Parse id attribute
	id_attr = xmlGetProp(tlv_node, (const xmlChar*)"id");
	if (!id_attr) {
		return EMV_CONFIG_XML_PARSE_ERROR;
	}
	tag = strtoul((const char*)id_attr, &endptr, 16);
	if (*endptr != '\0' || tag == 0) {
		xmlFree(id_attr);
		return EMV_CONFIG_XML_INVALID_DATA;
	}
	xmlFree(id_attr);

	// Parse content
	content = xmlNodeGetContent(tlv_node);
	if (!content) {
		return EMV_CONFIG_XML_INVALID_DATA;
	}
	trimmed = xml_str_trim(content);
	value_len = sizeof(value);
	r = parse_tlv_content(trimmed, value, &value_len);
	xmlFree(content);
	if (r) {
		return r;
	}

	// Create TLV field
	r = emv_tlv_list_push(list, (unsigned int)tag, (unsigned int)value_len, value, 0);
	if (r) {
		return EMV_ERROR_INTERNAL;
	}

	return 0;
}

static int parse_data_node(struct emv_ctx_t* ctx, xmlNode* data_node)
{
	struct emv_tlv_list_t list = EMV_TLV_LIST_INIT;
	int r;

	for (xmlNode* node = data_node->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		if (xmlStrcmp(node->name, (const xmlChar*)"tlv") == 0) {
			r = parse_tlv_node(node, &list);
			if (r) {
				emv_tlv_list_clear(&list);
				return r;
			}
		}
	}

	r = emv_config_data_set(ctx, &list);
	if (r) {
		emv_tlv_list_clear(&list);
		return r;
	}

	return 0;
}

static int emv_config_xml_parse(struct emv_ctx_t* ctx, xmlDoc* doc)
{
	int r;
	xmlNode* root;

	root = xmlDocGetRootElement(doc);
	if (!root || xmlStrcmp(root->name, (const xmlChar*)"emv") != 0) {
		return EMV_CONFIG_XML_PARSE_ERROR;
	}

	for (xmlNode* node = root->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		if (xmlStrcmp(node->name, (const xmlChar*)"data") == 0) {
			r = parse_data_node(ctx, node);
			if (r) {
				return r;
			}
		}
	}

	return 0;
}

int emv_config_xml_load(struct emv_ctx_t* ctx, const char* filename)
{
	int r;
	xmlDoc* doc;

	if (!ctx || !filename) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	doc = xmlReadFile(filename, NULL, 0);
	if (!doc) {
		return EMV_CONFIG_XML_PARSE_ERROR;
	}

	r = emv_config_xml_parse(ctx, doc);
	xmlFreeDoc(doc);
	if (r) {
		emv_config_clear(&ctx->config);
	}

	return r;
}

int emv_config_xml_load_buf(struct emv_ctx_t* ctx, const void* buf, size_t len)
{
	int r;
	xmlDoc* doc;

	if (!ctx || !buf || !len) {
		return EMV_ERROR_INVALID_PARAMETER;
	}

	doc = xmlReadMemory(buf, (int)len, NULL, NULL, 0);
	if (!doc) {
		return EMV_CONFIG_XML_PARSE_ERROR;
	}

	r = emv_config_xml_parse(ctx, doc);
	xmlFreeDoc(doc);
	if (r) {
		emv_config_clear(&ctx->config);
	}

	return r;
}
