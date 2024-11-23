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

// Qt containers may perform a deep copy when begin() is called by a range-
// based for loop, but std::as_const() is only available as of C++17 and
// qAsConst is deprecated by Qt-6.6. Therefore define a local helper template
// instead.
template<class T>
constexpr std::add_const_t<T>& to_const_ref(T& t) noexcept
{
	return t;
}
template<class T>
void to_const_ref(const T&&) = delete;

/**
 * Parse BER data and invoke callback function for each tag
 *
 * @param ptr Pointer to buffer
 * @param len Length of buffer
 * @param totalValidBytes[out] Number of BER bytes successfully parsed
 * @param tagFunc Function to invoke for each tag, using tagFunc(offset, tag)
 * @param paddingFunc Function to invoke for each instance of padding,
 *                    using paddingFunc(offset, tag)
 * @return Boolean indicating whether all bytes were parsed and valid
 */
template<
	typename TagFuncType,
	typename PaddingFuncType
>
static bool parseBerData(
	const void* ptr,
	std::size_t len,
	bool ignorePadding,
	std::size_t* totalValidBytes,
	TagFuncType tagFunc,
	PaddingFuncType paddingFunc
)
{
	int r;
	std::size_t validBytes = 0;
	struct iso8825_ber_itr_t itr;
	struct iso8825_tlv_t tlv;

	r = iso8825_ber_itr_init(ptr, len, &itr);
	if (r) {
		qWarning("iso8825_ber_itr_init() failed; r=%d", r);
		return false;
	}

	while ((r = iso8825_ber_itr_next(&itr, &tlv)) > 0) {

		// Notify caller of tag
		tagFunc(*totalValidBytes, tlv.tag);

		if (iso8825_ber_is_constructed(&tlv)) {
			// If the field is constructed, only consider the tag and length
			// to be valid until the value has been parsed. The fields inside
			// the value will be added when they are parsed.
			validBytes += (r - tlv.length);
			*totalValidBytes += (r - tlv.length);

			// Recursively parse constructed fields
			bool valid;
			valid = parseBerData(
				tlv.value,
				tlv.length,
				ignorePadding,
				totalValidBytes,
				tagFunc,
				paddingFunc
			);
			if (!valid) {
				qDebug("parseBerData() failed; totalValidBytes=%zu", *totalValidBytes);

				// Return here instead of breaking out to avoid repeated
				// processing of the error by recursive callers
				return false;
			}
			validBytes += tlv.length;

		} else {
			// If the field is not constructed, consider all of the bytes to
			// be valid BER encoded data
			validBytes += r;
			*totalValidBytes += r;
		}
	}
	if (r < 0) {
		// Determine whether invalid data is padding and notify caller
		// accordingly
		if (ignorePadding &&
			len - validBytes > 0 &&
			(
				((len & 0x7) == 0 && len - validBytes < 8) ||
				((len & 0xF) == 0 && len - validBytes < 16)
			)
		) {
			// Invalid data is likely to be padding; notify caller
			paddingFunc(*totalValidBytes, len - validBytes);

			// If the remaining bytes appear to be padding, consider these
			// bytes to be valid
			*totalValidBytes += len - validBytes;

		} else {
			qDebug("iso8825_ber_itr_next() failed; r=%d", r);
			return false;
		}
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

	// Parse BER encoded data, identify tag positions, and update number of
	// valid characters
	tagPositions.clear();
	paddingPositions.clear();
	parseBerData(data.constData(), data.size(), m_ignorePadding, &validBytes,
		[this](unsigned int offset, unsigned int tag) {
			unsigned int length;
			// Compute tag length
			if (tag <= 0xFF) {
				length = 1;
			} else if (tag <= 0xFFFF) {
				length = 2;
			} else if (tag <= 0xFFFFFF) {
				length = 3;
			} else if (tag <= 0xFFFFFFFF) {
				length = 4;
			} else {
				// Unsupported
				length = 0;
			}

			tagPositions.push_back({ offset * 2, length * 2 });
		},
		[this](unsigned int offset, unsigned length) {
			paddingPositions.push_back({ offset * 2, length * 2 });
		}
	);
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
	QTextCharFormat tagFormat;
	tagFormat.setFontWeight(QFont::Bold);
	tagFormat.setForeground(QColor(0xFF268BD2)); // Solarized blue
	QTextCharFormat paddingFormat;
	paddingFormat.setForeground(Qt::darkGray);
	QColor selectedColor(0xFF657B83); // Solarized Base00

	// Apply formatting of valid BER vs valid hex vs invalid vs padding
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

	if (m_emphasiseTags) {
		// Apply formatting of tags
		for (auto&& pos : to_const_ref(tagPositions)) {
			unsigned int digitIdx = 0;
			for (int i = 0; i < text.length(); ++i) {
				if (!std::isxdigit(text[i].unicode())) {
					// Ignore non-hex digits
					continue;
				}

				unsigned int currentIdx = blockData->startPos + digitIdx;
				if (currentIdx >= pos.offset &&
					currentIdx < (pos.offset + pos.length)
				) {
					setFormat(i, 1, tagFormat);
				}

				++digitIdx;
			}
		}

		// Apply formatting of padding
		for (auto&& pos : to_const_ref(paddingPositions)) {
			unsigned int digitIdx = 0;
			for (int i = 0; i < text.length(); ++i) {
				if (!std::isxdigit(text[i].unicode())) {
					// Ignore non-hex digits
					continue;
				}

				unsigned int currentIdx = blockData->startPos + digitIdx;
				if (currentIdx >= pos.offset &&
					currentIdx < (pos.offset + pos.length)
				) {
					setFormat(i, 1, paddingFormat);
				}

				++digitIdx;
			}
		}
	}

	if (m_selectionStart >= -1 && m_selectionCount > 0) {
		// Apply formatting of selected digits
		unsigned int digitIdx = 0;
		for (int i = 0; i < text.length(); ++i) {
			if (!std::isxdigit(text[i].unicode())) {
				// Ignore non-hex digits
				continue;
			}

			int currentIdx = blockData->startPos + digitIdx;
			if (currentIdx >= m_selectionStart &&
				currentIdx < (m_selectionStart + m_selectionCount)
			) {
				// Update format for selected digits
				// - Bold digits (eg tags) become white for good contrast
				// - Set background
				QTextCharFormat currentFormat = format(i);
				if (currentFormat.fontWeight() != QFont::Normal) {
					currentFormat.setForeground(Qt::white);
				}
				currentFormat.setBackground(selectedColor);
				setFormat(i, 1, currentFormat);
			}

			++digitIdx;
		}
	}
}
