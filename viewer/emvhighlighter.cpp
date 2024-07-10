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

void EmvHighlighter::highlightBlock(const QString& text)
{
	// QSyntaxHighlighter is designed to process one block at a time with very
	// little context about other blocks. This is not ideal for EMV parsing
	// but appears to be the only way to apply text formatting without
	// impacting the undo/redo stack. Therefore, this implementation processes
	// all blocks and reparses all of the EMV data when updating each block.
	// This implementation also assumes that rehighlight() is called whenever
	// the widget text changes because changes to later blocks may invalidate
	// EMV field lengths specified in earlier blocks.

	// NOTE: If anyone knows a better way, please let me know.

	QTextDocument* doc = document();
	int strLen = 0;
	unsigned int validLen;
	QString str;
	bool paddingIsPossible = false;
	QByteArray data;
	std::size_t validBytes = 0;
	QColor invalidColor;

	// Concatenate all blocks without whitespace and compute start position
	// and length of current block within concatenated string
	for (QTextBlock block = doc->begin(); block != doc->end(); block = block.next()) {
		QString blockStr = block.text().simplified().remove(' ');
		if (currentBlock() == block) {
			block.setUserData(new EmvTextBlockUserData(strLen, blockStr.length()));
		}

		strLen += blockStr.length();
		str += blockStr;
	}
	if (strLen != str.length()) {
		// Internal error
		qWarning("strLen=%d; str.length()=%d", strLen, (int)str.length());
		strLen = str.length();
	}
	validLen = strLen;

	// Ensure that hex string contains only hex digits
	for (unsigned int i = 0; i < validLen; ++i) {
		if (!std::isxdigit(str[i].unicode())) {
			// Only parse up to invalid digit
			validLen = i;
			break;
		}
	}

	// Ensure that hex string has even number of digits
	if (validLen & 0x01) {
		// Odd number of digits. Ignore last digit to see whether parsing can
		// proceed regardless and highlight error later.
		validLen -= 1;
	}

	// Determine whether invalid data might be padding
	if (m_ignorePadding && validLen == (unsigned int)strLen) {
		// Input data is a valid hex string and therefore the possibility
		// exists that if BER decoding fails, the remaining data might be
		// cryptographic padding
		paddingIsPossible = true;
	}

	// Only decode valid hex digits to binary
	data = QByteArray::fromHex(str.left(validLen).toUtf8());

	// Parse BER encoded data and update number of valid characters
	parseBerData(data.constData(), data.size(), &validBytes);
	validLen = validBytes * 2;

	// Determine whether invalid data is padding and prepare colour accordingly
	if (paddingIsPossible &&
		data.size() - validBytes > 0 &&
		(
			((data.size() & 0x7) == 0 && data.size() - validBytes < 8) ||
			((data.size() & 0xF) == 0 && data.size() - validBytes < 16)
		)
	) {
		// Invalid data is likely to be padding
		invalidColor = Qt::darkGray;
	} else {
		// Invalid data is either absent or unlikely to be padding
		invalidColor = Qt::red;
	}

	EmvTextBlockUserData* blockData = static_cast<decltype(blockData)>(currentBlockUserData());
	if (!blockData) {
		// Internal error
		qWarning("Block data missing for block number %d", currentBlock().blockNumber());
		return;
	}

	if (validLen >= blockData->startPos + blockData->length) {
		// All digits are valid
		setFormat(0, currentBlock().length(), QTextCharFormat());

	} else if (validLen <= blockData->startPos) {
		// All digits are invalid
		setFormat(0, currentBlock().length(), invalidColor);
	} else {
		// Some digits are invalid
		unsigned int digitIdx = 0;
		for (int i = 0; i < text.length(); ++i) {
			if (blockData->startPos + digitIdx < validLen) {
				setFormat(i, 1, QTextCharFormat());
			} else {
				setFormat(i, 1, invalidColor);
			}

			if (std::isxdigit(text[i].unicode())) {
				++digitIdx;
			}
		}
	}
}
