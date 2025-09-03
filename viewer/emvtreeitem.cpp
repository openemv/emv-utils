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
#include "emvtlvinfo.h"

#include "iso8825_ber.h"
#include "emv_tlv.h"
#include "emv_dol.h"

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QStringLiteral>
#include <QtCore/QStringList>

#include <cstddef>
#include <cstdint>

// Helper functions
static QString buildSimpleFieldString(
	QString str,
	qsizetype length,
	const std::uint8_t* value
);
static QString buildSimpleFieldString(
	unsigned int tag,
	qsizetype length,
	const std::uint8_t* value = nullptr
);
static QString buildRawValueString(
	qsizetype length,
	const std::uint8_t* value
);
static QString buildDecodedFieldString(const EmvTlvInfo& info);
static QString buildDecodedObjectString(const EmvTlvInfo& info);
static QString buildFieldString(const EmvTlvInfo& info, qsizetype length = -1);
static QTreeWidgetItem* addValueStringList(
	EmvTreeItem* item,
	const EmvTlvInfo& info
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
	unsigned int srcOffset,
	const void* ptr,
	std::size_t len
);

EmvTreeItem::EmvTreeItem(
	QTreeWidgetItem* parent,
	unsigned int srcOffset,
	unsigned int srcLength,
	const struct iso8825_tlv_t* tlv,
	bool decodeFields,
	bool decodeObjects,
	bool autoExpand
)
: QTreeWidgetItem(parent, EmvTreeItemType),
  m_srcOffset(srcOffset),
  m_srcLength(srcLength),
  m_hideWhenDecodingObject(false)
{
	setTlv(tlv);
	if (m_constructed) {
		// Always expand constructed fields
		autoExpand = true;
	}
	setExpanded(autoExpand);

	// Render the widget according to the current state
	render(decodeFields, decodeObjects);
}

EmvTreeItem::EmvTreeItem(
	QTreeWidgetItem* parent,
	unsigned int srcOffset,
	unsigned int srcLength,
	QString str,
	const void* value
)
: QTreeWidgetItem(parent, EmvTreeItemType),
  m_srcOffset(srcOffset),
  m_srcLength(srcLength),
  m_constructed(false),
  m_hideWhenDecodingObject(false)
{
	m_simpleFieldStr = m_decodedFieldStr =
		buildSimpleFieldString(str, srcLength, static_cast<const uint8_t*>(value));

	// Render the widget as-is
	render(false, false);
}

EmvTreeItem::EmvTreeItem(
	EmvTreeItem* parent,
	unsigned int srcOffset,
	unsigned int srcLength,
	const void* value
)
: QTreeWidgetItem(parent, EmvTreeItemType),
  m_srcOffset(srcOffset),
  m_srcLength(srcLength),
  m_constructed(false),
  m_hideWhenDecodingObject(false)
{
	// Reuse parent's name and description for when it is selected
	m_tagName = parent->m_tagName;
	m_tagDescription = parent->m_tagDescription;

	m_simpleFieldStr = m_decodedFieldStr =
		buildRawValueString(srcLength, static_cast<const uint8_t*>(value));

	// Render the widget as-is
	render(false, false);
}


void EmvTreeItem::deleteChildren()
{
	QList<QTreeWidgetItem*> list;

	list = takeChildren();
	while (!list.empty()) {
		delete list.takeFirst();
	}
}

void EmvTreeItem::render(bool showDecodedFields, bool showDecodedObjects)
{
	if (showDecodedFields) {
		if (showDecodedObjects) {
			if (!m_decodedObjectStr.isEmpty()) {
				setText(0, m_decodedObjectStr);
			} else {
				setText(0, m_decodedFieldStr);
			}
		} else {
			setText(0, m_decodedFieldStr);
		}
		setHidden(showDecodedObjects && m_hideWhenDecodingObject);

		// Make decoded values visible
		if (!m_constructed) {
			for (int i = 0; i < childCount(); ++i) {
				child(i)->setHidden(false);
			}
		}

	} else {
		setText(0, m_simpleFieldStr);
		setHidden(false);

		// Hide decoded values
		if (!m_constructed) {
			for (int i = 0; i < childCount(); ++i) {
				child(i)->setHidden(true);
			}
		}
	}
}

void EmvTreeItem::setTlv(const struct iso8825_tlv_t* tlv)
{
	// First delete existing children
	deleteChildren();

	EmvTlvInfo info(tlv);
	if (info.error()) {
		qDebug("No info for field 0x%02X", tlv->tag);
	}
	m_tagName = info.tagName();
	m_tagDescription = info.tagDescription();
	m_constructed = info.isConstructed();
	m_decodedFieldStr = buildDecodedFieldString(info);
	m_decodedObjectStr = buildDecodedObjectString(info);

	if (m_constructed) {
		// Add field length but omit raw value bytes from field strings for
		// constructed fields
		m_simpleFieldStr = buildSimpleFieldString(tlv->tag, tlv->length);
	} else {
		// Add field length and raw value bytes to simple field string for
		// primitive fields
		m_simpleFieldStr = buildSimpleFieldString(tlv->tag, tlv->length, tlv->value);

		// Add raw value bytes as first child for primitive fields that have
		// value bytes
		if (tlv->length) {
			addValueRaw(
				this,
				m_srcOffset + m_srcLength - tlv->length,
				tlv->value,
				tlv->length
			);
		}

		if (info.valueStrIsList()) {
			addValueStringList(this, info);
		} else if (info.format() == EmvFormat::DOL) {
			addValueDol(this, tlv->value, tlv->length);
		} else if (info.format() == EmvFormat::TAG_LIST) {
			addValueTagList(this, tlv->value, tlv->length);
		}
	}
}

static QString buildSimpleFieldString(
	QString str,
	qsizetype length,
	const std::uint8_t* value
)
{
	if (value) {
		return
			str +
			QString::asprintf(" : [%zu] ", static_cast<std::size_t>(length)) +
			// Create an uppercase hex string, with spaces, from the
			// field's value bytes
			QByteArray::fromRawData(
				reinterpret_cast<const char*>(value),
				length
			).toHex(' ').toUpper().constData();
	} else {
		return
			str +
			QString::asprintf(" : [%zu]", static_cast<std::size_t>(length));
	}
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

static QString buildRawValueString(
	qsizetype length,
	const std::uint8_t* value
)
{
	if (value) {
		return
			QString::asprintf("[%zu] ",
				static_cast<std::size_t>(length)
			) +
			// Create an uppercase hex string, with spaces, from the
			// field's value bytes
			QByteArray::fromRawData(
				reinterpret_cast<const char*>(value),
				length
			).toHex(' ').toUpper().constData();
	} else {
		return QString::asprintf("[%zu]", static_cast<std::size_t>(length));
	}
}

static QString buildDecodedFieldString(const EmvTlvInfo& info)
{
	if (!info.tagName().isEmpty()) {
		QString fieldStr = QString::asprintf("%02X | %s",
			info.tag(), qUtf8Printable(info.tagName())
		);
		if (!info.isConstructed() &&
			!info.valueStrIsList() &&
			!info.valueStr().isEmpty()
		) {
			if (info.formatIsString()) {
				fieldStr += QStringLiteral(" : \"") +
					info.valueStr() +
					QStringLiteral("\"");
			} else {
				fieldStr += QStringLiteral(" : ") + info.valueStr();
			}
		}
		return fieldStr;

	} else {
		return QString::asprintf("%02X", info.tag());
	}
}

static QString buildDecodedObjectString(const EmvTlvInfo& info)
{
	if (info.isConstructed() && !info.valueStr().isEmpty()) {
		// Assume that a constructed field with a value string is an object
		// of some kind
		return QString::asprintf("%02X | %s",
			info.tag(), qUtf8Printable(info.valueStr())
		);
	} else {
		// Emtry string for non-objects
		return QString();
	}
}

static QString buildFieldString(const EmvTlvInfo& info, qsizetype length)
{
	if (!info.tagName().isEmpty()) {
		if (length > -1) {
			return QString::asprintf("%02X | %s [%zu]",
				info.tag(),
				qUtf8Printable(info.tagName()),
				static_cast<std::size_t>(length)

			);
		} else {
			return QString::asprintf("%02X | %s",
				info.tag(), qUtf8Printable(info.tagName())
			);
		}
	} else {
		if (length > -1) {
			return QString::asprintf("%02X [%zu]",
				info.tag(), static_cast<std::size_t>(length)
			);
		} else {
			return QString::asprintf("%02X", info.tag());
		}
	}
}

static QTreeWidgetItem* addValueStringList(
	EmvTreeItem* item,
	const EmvTlvInfo& info
)
{
	if (info.valueStr().isEmpty()) {
		return nullptr;
	}

	QTreeWidgetItem* valueItem = new QTreeWidgetItem(
		item,
		QStringList(
			info.valueStr().trimmed() // Trim trailing newline
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
		EmvTlvInfo info(&entry);

		QTreeWidgetItem* valueItem = new QTreeWidgetItem(
			dolItem,
			QStringList(
				buildFieldString(info, entry.length)
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
		EmvTlvInfo info(tag);

		QTreeWidgetItem* valueItem = new QTreeWidgetItem(
			tlItem,
			QStringList(
				buildFieldString(info)
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
	unsigned int srcOffset,
	const void* ptr,
	std::size_t len
)
{
	EmvTreeItem* valueItem = new EmvTreeItem(
		item,
		srcOffset,
		len,
		ptr
	);
	valueItem->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsEnabled | Qt::ItemIsSelectable);

	// Use default monospace font
	QFont font = valueItem->font(0);
	font.setFamily("Monospace");
	valueItem->setFont(0, font);
	valueItem->setForeground(0, Qt::darkGray);

	return valueItem;
}
