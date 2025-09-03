/**
 * @file emvtlvinfo.cpp
 * @brief Abstraction for information related to decoded EMV fields
 *
 * Copyright 2025 Leon Lynch
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "emvtlvinfo.h"

#include "iso8825_ber.h"
#include "emv_tlv.h"
#include "emv_dol.h"
#include "emv_strings.h"

#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <cstring>

static constexpr EmvFormat convertFormat(enum emv_format_t format)
{
	switch (format) {
		case EMV_FORMAT_A: return EmvFormat::A;
		case EMV_FORMAT_AN: return EmvFormat::AN;
		case EMV_FORMAT_ANS: return EmvFormat::ANS;
		case EMV_FORMAT_CN: return EmvFormat::CN;
		case EMV_FORMAT_N: return EmvFormat::N;
		case EMV_FORMAT_VAR: return EmvFormat::VAR;
		case EMV_FORMAT_DOL: return EmvFormat::DOL;
		case EMV_FORMAT_TAG_LIST: return EmvFormat::TAG_LIST;

		default:
			// Default is binary format
			return EmvFormat::B;
	}
}

static bool formatIsString(EmvFormat format, const struct iso8825_tlv_t* tlv)
{
	return (
		format == EmvFormat::A ||
		format == EmvFormat::AN ||
		format == EmvFormat::ANS ||
		iso8825_ber_is_string(tlv)
	);
}

EmvTlvInfo::EmvTlvInfo(const struct iso8825_tlv_t* tlv)
{
	struct emv_tlv_t emv_tlv;
	struct emv_tlv_info_t info;
	QByteArray valueStr(2048, 0);

	if (!tlv) {
		return;
	}
	m_error = false;
	m_tag = tlv->tag;

	emv_tlv.ber = *tlv;
	emv_tlv_get_info(
		&emv_tlv,
		nullptr,
		&info,
		valueStr.data(),
		valueStr.size()
	);
	if (info.tag_name) {
		m_tagName = info.tag_name;
	}
	if (info.tag_desc) {
		m_tagDescription = info.tag_desc;
	}
	// NOTE: Qt6 differs from Qt5 for when a QByteArray is passed to
	// QString::fromUtf8(), resulting in a QString with the same size
	// as the whole QByteArray, regardless of null-termination. It is
	// safe for both Qt5 and Qt6 to pass the content of QByteArray as
	// a C-string instead. For safety, it is useful to compute the size
	// using qstrnlen() to ensure that it does not exceed the size of
	// the QByteArray content.
	m_valueStr = QString::fromUtf8(
		valueStr.constData(),
		qstrnlen(valueStr.constData(), valueStr.size() - 1)
	);

	m_constructed = iso8825_ber_is_constructed(tlv);
	m_format = convertFormat(info.format);
	m_format_is_string = ::formatIsString(m_format, tlv);
}

EmvTlvInfo::EmvTlvInfo(const struct emv_dol_entry_t* entry)
{
	struct emv_tlv_t emv_tlv;
	struct emv_tlv_info_t info;

	if (!entry) {
		// Error
		return;
	}
	m_error = false;
	m_tag = entry->tag;

	std::memset(&emv_tlv, 0, sizeof(emv_tlv));
	emv_tlv.tag = entry->tag;
	emv_tlv.length = entry->length;

	emv_tlv_get_info(&emv_tlv, nullptr, &info, nullptr, 0);
	if (info.tag_name) {
		m_tagName = info.tag_name;
	}
	if (info.tag_desc) {
		m_tagDescription = info.tag_desc;
	}
	m_format = convertFormat(info.format);
	m_format_is_string = ::formatIsString(m_format, &emv_tlv.ber);
}

EmvTlvInfo::EmvTlvInfo(unsigned int tag)
{
	struct emv_tlv_t emv_tlv;
	struct emv_tlv_info_t info;

	// Assume that tag is valid
	m_error = false;
	m_tag = tag;

	std::memset(&emv_tlv, 0, sizeof(emv_tlv));
	emv_tlv.tag = tag;

	emv_tlv_get_info(&emv_tlv, nullptr, &info, nullptr, 0);
	if (info.tag_name) {
		m_tagName = info.tag_name;
	}
	if (info.tag_desc) {
		m_tagDescription = info.tag_desc;
	}
	m_format = convertFormat(info.format);
	m_format_is_string = ::formatIsString(m_format, &emv_tlv.ber);
}

bool EmvTlvInfo::valueStrIsList() const
{
	if (m_valueStr.isEmpty()) {
		return false;
	}

	// If the last character is a newline, assume that it is a list of strings
	return m_valueStr.back() == '\n';
}
