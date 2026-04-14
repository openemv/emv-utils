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
#include "emv_fields.h"

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

static int parse_unsigned_int_node(xmlNode* node, unsigned int* value)
{
	xmlChar* content;
	xmlChar* trimmed;
	char* endptr;
	unsigned long val;

	content = xmlNodeGetContent(node);
	if (!content) {
		return EMV_CONFIG_XML_INVALID_DATA;
	}

	trimmed = xml_str_trim(content);
	if (!*trimmed) {
		xmlFree(content);
		return EMV_CONFIG_XML_INVALID_DATA;
	}

	val = strtoul((const char*)trimmed, &endptr, 10);
	if (*endptr != '\0') {
		xmlFree(content);
		return EMV_CONFIG_XML_INVALID_DATA;
	}
	xmlFree(content);

	*value = (unsigned int)val;
	return 0;
}

static int parse_data_node(xmlNode* data_node, struct emv_tlv_list_t* list)
{
	int r;

	for (xmlNode* node = data_node->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		if (xmlStrcmp(node->name, (const xmlChar*)"tlv") == 0) {
			r = parse_tlv_node(node, list);
			if (r) {
				return r;
			}
		}
	}

	return 0;
}

static int parse_app_node(struct emv_ctx_t* ctx, xmlNode* app_node)
{
	int r;
	xmlChar* aid_attr;
	xmlChar* match_attr;
	uint8_t aid[16];
	unsigned int aid_len;
	uint8_t asi;
	struct emv_tlv_list_t list = EMV_TLV_LIST_INIT;
	struct emv_config_app_t* app = NULL;
	unsigned int random_percentage = 0;
	unsigned int random_max_percentage = 0;
	unsigned int random_threshold = 0;

	// Parse aid attribute
	aid_attr = xmlGetProp(app_node, (const xmlChar*)"aid");
	if (!aid_attr) {
		return EMV_CONFIG_XML_PARSE_ERROR;
	}
	aid_len = sizeof(aid);
	r = parse_hex((const char*)aid_attr, aid, &aid_len);
	xmlFree(aid_attr);
	if (r) {
		return EMV_CONFIG_XML_INVALID_DATA;
	}
	if (aid_len < 5 || aid_len > 16) {
		return EMV_CONFIG_XML_INVALID_DATA;
	}

	// Parse match attribute
	match_attr = xmlGetProp(app_node, (const xmlChar*)"match");
	if (!match_attr) {
		return EMV_CONFIG_XML_PARSE_ERROR;
	}
	if (xmlStrcmp(match_attr, (const xmlChar*)"exact") == 0) {
		asi = EMV_ASI_EXACT_MATCH;
	} else if (xmlStrcmp(match_attr, (const xmlChar*)"partial") == 0) {
		asi = EMV_ASI_PARTIAL_MATCH;
	} else {
		xmlFree(match_attr);
		return EMV_CONFIG_XML_INVALID_DATA;
	}
	xmlFree(match_attr);

	// Parse children
	for (xmlNode* node = app_node->children; node; node = node->next) {
		if (node->type != XML_ELEMENT_NODE) {
			continue;
		}

		r = 0;
		if (xmlStrcmp(node->name, (const xmlChar*)"data") == 0) {
			r = parse_data_node(node, &list);
		} else if (xmlStrcmp(node->name, (const xmlChar*)"random_selection_percentage") == 0) {
			r = parse_unsigned_int_node(node, &random_percentage);
		} else if (xmlStrcmp(node->name, (const xmlChar*)"random_selection_max_percentage") == 0) {
			r = parse_unsigned_int_node(node, &random_max_percentage);
		} else if (xmlStrcmp(node->name, (const xmlChar*)"random_selection_threshold") == 0) {
			r = parse_unsigned_int_node(node, &random_threshold);
		}
		if (r) {
			emv_tlv_list_clear(&list);
			return r;
		}
		if (random_percentage > 99 || random_max_percentage > 99) {
			emv_tlv_list_clear(&list);
			return EMV_CONFIG_XML_INVALID_DATA;
		}
	}

	// Create application configuration
	r = emv_config_app_create(ctx, aid, aid_len, asi, &list, &app);
	if (r) {
		emv_tlv_list_clear(&list);
		return r;
	}
	app->random_selection_percentage = random_percentage;
	app->random_selection_max_percentage = random_max_percentage;
	app->random_selection_threshold = random_threshold;

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
			struct emv_tlv_list_t list = EMV_TLV_LIST_INIT;
			r = parse_data_node(node, &list);
			if (r) {
				emv_tlv_list_clear(&list);
				return r;
			}
			r = emv_config_data_set(ctx, &list);
			if (r) {
				emv_tlv_list_clear(&list);
				return r;
			}
		}
		if (xmlStrcmp(node->name, (const xmlChar*)"app") == 0) {
			r = parse_app_node(ctx, node);
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
