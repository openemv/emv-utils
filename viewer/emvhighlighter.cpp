/**
 * @file emvhighlighter.cpp
 * @brief QSyntaxHighlighter derivative that applies highlighting to EMV data
 *
 * Copyright 2024 Leon Lynch
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

#include "emvhighlighter.h"
#include "iso8825_ber.h"

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtGui/QTextDocument>
#include <QtGui/QTextBlock>
#include <QtGui/QTextCharFormat>
#include <QtGui/QColor>
#include <QtGui/QFont>

#include <cstddef>
#include <cctype>

static bool parseBerData(
	const void* ptr,
	std::size_t len,
	std::size_t* validBytes
)
{
	int r;
	struct iso8825_ber_itr_t itr;
	struct iso8825_tlv_t tlv;
	bool valid;

	r = iso8825_ber_itr_init(ptr, len, &itr);
	if (r) {
		qWarning("iso8825_ber_itr_init() failed; r=%d", r);
		return false;
	}

	while ((r = iso8825_ber_itr_next(&itr, &tlv)) > 0) {

		if (iso8825_ber_is_constructed(&tlv)) {
			// If the field is constructed, only consider the tag and length
			// to be valid until the value has been parsed. The fields inside
			// the value will be added when they are parsed.
			*validBytes += (r - tlv.length);

			// Recursively parse constructed fields
			valid = parseBerData(tlv.value, tlv.length, validBytes);
			if (!valid) {
				qDebug("parseBerData() failed; validBytes=%zu", *validBytes);
				return false;
			}

		} else {
			// If the field is not constructed, consider all of the bytes to
			// be valid BER encoded data
			*validBytes += r;
		}
	}
	if (r < 0) {
		qDebug("iso8825_ber_itr_next() failed; r=%d", r);
		return false;
	}

	return true;
}

class EmvTextBlockUserData : public QTextBlockUserData
{
public:
	EmvTextBlockUserData(unsigned int startPos, unsigned int length)
	: startPos(startPos),
	  length(length)
	{}

public:
	unsigned int startPos;
	unsigned int length;
};

void EmvHighlighter::parseBlocks()
{
	// This function is responsible for updating these member variables:
	// - strLen (length of string without whitespace)
	// - hexStrLen (length of string containing only hex digits)
	// - berStrLen (length of string containing valid BER encoded data)
	// The caller is responsible for calling this function before rehighlight()
	// when the widget text changes to ensure that these member variables are
	// updated appropriately. This allows highlightBlock() to use these member
	// variables to determine the appropriate highlight formatting.

	QTextDocument* doc = document();
	QString str;
	QByteArray data;
	std::size_t validBytes = 0;

	// Concatenate all blocks without whitespace and compute start position
	// and length of each block within concatenated string
	strLen = 0;
	for (QTextBlock block = doc->begin(); block != doc->end(); block = block.next()) {
		QString blockStr = block.text().simplified().remove(' ');
		block.setUserData(new EmvTextBlockUserData(strLen, blockStr.length()));

		strLen += blockStr.length();
		str += blockStr;
	}
	if (strLen != (unsigned int)str.length()) {
		// Internal error
		qWarning("strLen=%u; str.length()=%d", strLen, (int)str.length());
		strLen = str.length();
	}
	hexStrLen = strLen;

	// Ensure that hex string contains only hex digits
	for (unsigned int i = 0; i < hexStrLen; ++i) {
		if (!std::isxdigit(str[i].unicode())) {
			// Only parse up to invalid digit
			hexStrLen = i;
			break;
		}
	}

	// Ensure that hex string has even number of digits
	if (hexStrLen & 0x01) {
		// Odd number of digits. Ignore last digit to see whether parsing can
		// proceed regardless and highlight error later.
		hexStrLen -= 1;
	}

	// Only decode valid hex digits to binary
	data = QByteArray::fromHex(str.left(hexStrLen).toUtf8());

	// Parse BER encoded data and update number of valid characters
	parseBerData(data.constData(), data.size(), &validBytes);
	berStrLen = validBytes * 2;
}

void EmvHighlighter::highlightBlock(const QString& text)
{
	// QSyntaxHighlighter is designed to process one block at a time with very
	// little context about other blocks. This is not ideal for EMV parsing
	// but appears to be the only way to apply text formatting without
	// impacting the undo/redo stack. Also, changes to later blocks may
	// invalidate EMV field lengths specified in earlier blocks and therefore
	// this implementation assumes that all blocks must be reparsed whenever
	// any block changes.

	// This implementation relies on parseBlocks() to reprocess all blocks
	// whenever the widget text changes but not to apply highlighting. However,
	// rehighlight() is used to apply highlighting without reprocessing all
	// blocks. Therefore, rehighlight should either be used after parseBlocks()
	// when the widget text changed or separately from parseBlocks() when only
	// a property changed.

	EmvTextBlockUserData* blockData = static_cast<decltype(blockData)>(currentBlockUserData());
	if (!blockData) {
		// Internal error
		qWarning("Block data missing for block number %d", currentBlock().blockNumber());
		return;
	}

	// Prepare colours/formats
	QColor invalidColor = Qt::red; // Default colour for invalid data
	QTextCharFormat nonHexFormat; // Default format for non-hex data
	nonHexFormat.setFontWeight(QFont::Bold);
	nonHexFormat.setBackground(Qt::red);

	// Determine whether invalid data is padding and update colour accordingly
	if (m_ignorePadding &&
		hexStrLen == strLen &&
		hexStrLen - berStrLen > 0
	) {
		unsigned int totalBytes = hexStrLen / 2;
		unsigned int extraBytes = (hexStrLen - berStrLen) / 2;

		// Invalid data is assumed to be padding if it is either less than 8
		// bytes when the total data length is a multiple of 8 bytes (for
		// example DES) or if it is less than 16 bytes when the total data
		// length is a multiple of 16 bytes (for example AES).

		if (
			((totalBytes & 0x7) == 0 && extraBytes < 8) ||
			((totalBytes & 0xF) == 0 && extraBytes < 16)
		) {
			// Invalid data is likely to be padding
			invalidColor = Qt::darkGray;
		}
	}

	if (berStrLen >= blockData->startPos + blockData->length) {
		// All digits are valid
		setFormat(0, currentBlock().length(), QTextCharFormat());

	} else if (berStrLen <= blockData->startPos) {
		// All digits are invalid and some may be non-hex as well
		for (int i = 0; i < text.length(); ++i) {
			if (std::isxdigit(text[i].unicode())) {
				setFormat(i, 1, invalidColor);
			} else {
				setFormat(i, 1, nonHexFormat);
			}
		}
	} else {
		// Some digits are invalid
		unsigned int digitIdx = 0;
		for (int i = 0; i < text.length(); ++i) {
			if (std::isxdigit(text[i].unicode())) {
				if (blockData->startPos + digitIdx < berStrLen) {
					// Valid digits
					setFormat(i, 1, QTextCharFormat());
				} else {
					// Invalid/padding digits
					setFormat(i, 1, invalidColor);
				}

				++digitIdx;
			} else {
				// Non-hex digits
				setFormat(i, 1, nonHexFormat);
			}
		}
	}
}
