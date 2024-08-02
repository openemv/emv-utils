/**
 * @file emvtreeitem.cpp
 * @brief QTreeWidgetItem derivative that represents an EMV field
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

#include "emvtreeitem.h"

#include "iso8825_ber.h"
#include "emv_tlv.h"
#include "emv_dol.h"
#include "emv_strings.h"

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringLiteral>
#include <QtCore/QStringList>

#include <cstddef>
#include <cstdint>

// Helper functions
static bool valueStrIsList(const QByteArray& str);
static QString buildSimpleFieldString(
	unsigned int tag,
	qsizetype length,
	const std::uint8_t* value = nullptr
);
static QString buildDecodedFieldString(
	unsigned int tag,
	const struct emv_tlv_info_t& info,
	const QByteArray& valueStr
);
static QString buildFieldString(
	unsigned int tag,
	const char* name = nullptr,
	qsizetype length = -1
);
static QTreeWidgetItem* addValueStringList(
	EmvTreeItem* item,
	const QByteArray& valueStr
);
static QTreeWidgetItem* addValueDol(
	EmvTreeItem* item,
	const void* ptr,
	std::size_t len
);
static QTreeWidgetItem* addValueTagList(
	EmvTreeItem* item,
	const void* ptr,
	std::size_t len
);
static QTreeWidgetItem* addValueRaw(
	EmvTreeItem* item,
	const void* ptr,
	std::size_t len
);

EmvTreeItem::EmvTreeItem(
	QTreeWidgetItem* parent,
	unsigned int srcOffset,
	unsigned int srcLength,
	const struct iso8825_tlv_t* tlv,
	bool decode,
	bool autoExpand
)
: QTreeWidgetItem(parent, EmvTreeItemType),
  m_srcOffset(srcOffset),
  m_srcLength(srcLength)
{
	setTlv(tlv, decode);
	if (m_constructed) {
		// Always expand constructed fields
		autoExpand = true;
	}
	setExpanded(autoExpand);
}

void EmvTreeItem::deleteChildren()
{
	QList<QTreeWidgetItem*> list;

	list = takeChildren();
	while (!list.empty()) {
		delete list.takeFirst();
	}
}

void EmvTreeItem::render(bool showDecoded)
{
	if (showDecoded) {
		setText(0, m_decodedFieldStr);

		// Make decoded values visible
		if (!m_constructed) {
			for (int i = 0; i < childCount(); ++i) {
				child(i)->setHidden(false);
			}
		}

	} else {
		setText(0, m_simpleFieldStr);

		// Hide decoded values
		if (!m_constructed) {
			for (int i = 0; i < childCount(); ++i) {
				child(i)->setHidden(true);
			}
		}
	}
}

void EmvTreeItem::setTlv(const struct iso8825_tlv_t* tlv, bool decode)
{
	int r;
	struct emv_tlv_t emv_tlv;
	struct emv_tlv_info_t info;
	QByteArray valueStr(2048, 0);

	// First delete existing children
	deleteChildren();

	emv_tlv.ber = *tlv;
	r = emv_tlv_get_info(&emv_tlv, &info, valueStr.data(), valueStr.size());
	if (r) {
		qDebug("emv_tlv_get_info()=%d; tag=0x%02X", r, tlv->tag);
	}
	if (info.tag_name) {
		m_tagName = info.tag_name;
	}
	if (info.tag_desc) {
		m_tagDescription = info.tag_desc;
	}
	m_constructed = iso8825_ber_is_constructed(tlv);
	m_decodedFieldStr = buildDecodedFieldString(tlv->tag, info, valueStr);

	if (m_constructed) {
		// Add field length but omit raw value bytes from field strings for
		// constructed fields
		m_simpleFieldStr = buildSimpleFieldString(tlv->tag, tlv->length);
	} else {
		// Add field length and raw value bytes to simple field string for
		// primitive fields
		m_simpleFieldStr = buildSimpleFieldString(tlv->tag, tlv->length, tlv->value);

		// Always add raw value bytes as first child for primitive fields
		addValueRaw(this, tlv->value, tlv->length);

		if (valueStrIsList(valueStr)) {
			addValueStringList(this, valueStr);
		} else if (info.format == EMV_FORMAT_DOL) {
			addValueDol(this, tlv->value, tlv->length);
		} else if (info.format == EMV_FORMAT_TAG_LIST) {
			addValueTagList(this, tlv->value, tlv->length);
		}
	}

	// Render the widget according to the current state
	render(decode);
}

static bool valueStrIsList(const QByteArray& str)
{
	if (!str[0]) {
		return false;
	}

	// If the last character is a newline, assume that it is a string list
	return str[qstrnlen(str.constData(), str.size()) - 1] == '\n';
}

static QString buildSimpleFieldString(
	unsigned int tag,
	qsizetype length,
	const std::uint8_t* value
)
{
	if (value) {
		return
			QString::asprintf("%02X : [%zu] ",
				tag, static_cast<std::size_t>(length)
			) +
			// Create an uppercase hex string, with spaces, from the
			// field's value bytes
			QByteArray::fromRawData(
				reinterpret_cast<const char*>(value),
				length
			).toHex(' ').toUpper().constData();
	} else {
		return QString::asprintf("%02X : [%zu]", tag, static_cast<std::size_t>(length));
	}
}

static QString buildDecodedFieldString(
	unsigned int tag,
	const struct emv_tlv_info_t& info,
	const QByteArray& valueStr
)
{
	if (info.tag_name) {
		QString fieldStr = QString::asprintf("%02X | %s", tag, info.tag_name);;
		if (!valueStrIsList(valueStr) && valueStr[0]) {
			if (info.format == EMV_FORMAT_A ||
				info.format == EMV_FORMAT_AN ||
				info.format == EMV_FORMAT_ANS
			) {
				fieldStr += QStringLiteral(" : \"") +
					valueStr.constData() +
					QStringLiteral("\"");
			} else {
				fieldStr += QStringLiteral(" : ") + valueStr.constData();
			}
		}
		return fieldStr;

	} else {
		return QString::asprintf("%02X", tag);
	}
}

static QString buildFieldString(
	unsigned int tag,
	const char* name,
	qsizetype length
)
{
	if (name) {
		if (length > -1) {
			return QString::asprintf("%02X | %s [%zu]", tag, name, static_cast<std::size_t>(length));
		} else {
			return QString::asprintf("%02X | %s", tag, name);
		}
	} else {
		if (length > -1) {
			return QString::asprintf("%02X [%zu]", tag, static_cast<std::size_t>(length));
		} else {
			return QString::asprintf("%02X", tag);
		}
	}
}

static QTreeWidgetItem* addValueStringList(
	EmvTreeItem* item,
	const QByteArray& valueStr
)
{
	if (!valueStr[0]) {
		return nullptr;
	}

	QTreeWidgetItem* valueItem = new QTreeWidgetItem(
		item,
		QStringList(
			// NOTE: Qt6 differs from Qt5 for when a QByteArray is passed to
			// QString::fromUtf8(), resulting in a QString with the same size
			// as the whole QByteArray, regardless of null-termination. It is
			// safe for both Qt5 and Qt6 to pass the content of QByteArray as
			// a C-string instead. For safety, it is useful to compute the size
			// using qstrnlen() to ensure that it does not exceed the size of
			// the QByteArray content.
			QString::fromUtf8(
				valueStr.constData(),
				qstrnlen(valueStr.constData(), valueStr.size() - 1)
			).trimmed() // Trim trailing newline
		)
	);
	valueItem->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsEnabled);

	return valueItem;
}

static QTreeWidgetItem* addValueDol(
	EmvTreeItem* item,
	const void* ptr,
	std::size_t len
)
{
	int r;
	struct emv_dol_itr_t itr;
	struct emv_dol_entry_t entry;

	QTreeWidgetItem* dolItem = new QTreeWidgetItem(
		item,
		QStringList(
			QStringLiteral("Data Object List:")
		)
	);
	dolItem->setExpanded(true);
	dolItem->setFlags(Qt::ItemIsEnabled);

	r = emv_dol_itr_init(ptr, len, &itr);
	if (r) {
		qWarning("emv_dol_itr_init() failed; r=%d", r);
		return nullptr;
	}

	while ((r = emv_dol_itr_next(&itr, &entry)) > 0) {
		struct emv_tlv_t emv_tlv;
		struct emv_tlv_info_t info;

		memset(&emv_tlv, 0, sizeof(emv_tlv));
		emv_tlv.tag = entry.tag;
		emv_tlv.length = entry.length;
		emv_tlv_get_info(&emv_tlv, &info, nullptr, 0);

		QTreeWidgetItem* valueItem = new QTreeWidgetItem(
			dolItem,
			QStringList(
				buildFieldString(entry.tag, info.tag_name, entry.length)
			)
		);
		valueItem->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsEnabled);
	}
	if (r < 0) {
		qDebug("emv_dol_itr_next() failed; r=%d", r);
		return nullptr;
	}

	return dolItem;
}

static QTreeWidgetItem* addValueTagList(
	EmvTreeItem* item,
	const void* ptr,
	std::size_t len
)
{
	int r;
	unsigned int tag;

	QTreeWidgetItem* tlItem = new QTreeWidgetItem(
		item,
		QStringList(
			QStringLiteral("Tag List:")
		)
	);
	tlItem->setExpanded(true);
	tlItem->setFlags(Qt::ItemIsEnabled);

	while ((r = iso8825_ber_tag_decode(ptr, len, &tag)) > 0) {
		struct emv_tlv_t emv_tlv;
		struct emv_tlv_info_t info;

		memset(&emv_tlv, 0, sizeof(emv_tlv));
		emv_tlv.tag = tag;
		emv_tlv_get_info(&emv_tlv, &info, nullptr, 0);

		QTreeWidgetItem* valueItem = new QTreeWidgetItem(
			tlItem,
			QStringList(
				buildFieldString(tag, info.tag_name)
			)
		);
		valueItem->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsEnabled);

		// Advance
		ptr = static_cast<const char*>(ptr) + r;
		len -= r;
	}
	if (r < 0) {
		qDebug("iso8825_ber_tag_decode() failed; r=%d", r);
		return nullptr;
	}

	return tlItem;
}

static QTreeWidgetItem* addValueRaw(
	EmvTreeItem* item,
	const void* ptr,
	std::size_t len
)
{
	QTreeWidgetItem* valueItem = new QTreeWidgetItem(
		item,
		QStringList(
			QString::asprintf("[%zu] ", len) +
			// Create an uppercase hex string, with spaces, from the
			// field's value bytes
			QByteArray::fromRawData(
				reinterpret_cast<const char*>(ptr),
				len
			).toHex(' ').toUpper().constData()
		)
	);
	valueItem->setFlags(Qt::ItemNeverHasChildren);

	// Use default monospace font
	QFont font = valueItem->font(0);
	font.setFamily("Monospace");
	valueItem->setFont(0, font);

	return valueItem;
}
