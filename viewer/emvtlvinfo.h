/**
 * @file emvtlvinfo.h
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

#ifndef EMV_TLV_INFO_H
#define EMV_TLV_INFO_H

#include <QtCore/QString>

// Forward declarations
struct iso8825_tlv_t;
struct emv_dol_entry_t;

/// See @ref emv_format_t
enum class EmvFormat {
	/// Alphabetic data
	A,
	/// Alphabetic data.
	AN,
	/// Alphanumeric special data.
	ANS,
	/// Fixed length binary data.
	B,
	/// Compressed numeric data.
	CN,
	/// Numeric data
	N,
	/// Variable length binary data
	VAR,
	/// Data Object List (DOL)
	DOL,
	/// Tag List
	TAG_LIST,
};

class EmvTlvInfo
{
public:
	EmvTlvInfo(const struct iso8825_tlv_t* tlv);
	EmvTlvInfo(const struct emv_dol_entry_t* entry);
	EmvTlvInfo(unsigned int tag);

public:
	bool error() const { return m_error; }
	unsigned int tag() const { return m_tag; }
	QString tagName() const { return m_tagName; }
	QString tagDescription() const { return m_tagDescription; }
	QString valueStr() const { return m_valueStr; }

	bool valueStrIsList() const;
	bool isConstructed() const { return m_constructed; }
	EmvFormat format() const { return m_format; }
	bool formatIsString() const { return m_format_is_string; }

private:
	bool m_error = true;
	unsigned int m_tag;
	QString m_tagName;
	QString m_tagDescription;
	QString m_valueStr;
	bool m_constructed;
	EmvFormat m_format;
	bool m_format_is_string;
};

#endif
