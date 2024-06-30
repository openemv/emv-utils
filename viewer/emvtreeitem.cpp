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
#include <QtCore/QString>
#include <QtCore/QStringLiteral>
#include <QtCore/QStringList>

#include <cstddef>

// Helper functions
static QString buildFieldString(
	unsigned int tag,
	const char* name = nullptr,
	qsizetype length = -1
);
static bool valueStrIsList(const QByteArray& str);
static QTreeWidgetItem* addDescStringList(
	EmvTreeItem* item,
	const QByteArray& valueStr
);
static QTreeWidgetItem* addDescString(
	EmvTreeItem* item,
	const emv_format_t format,
	const QByteArray& valueStr
);
static QTreeWidgetItem* addDescDol(
	EmvTreeItem* item,
	const void* ptr,
	std::size_t len
);
static QTreeWidgetItem* addDescTagList(
	EmvTreeItem* item,
	const void* ptr,
	std::size_t len
);
static QTreeWidgetItem* addDescRaw(
	EmvTreeItem* item,
	const void* ptr,
	std::size_t len
);

EmvTreeItem::EmvTreeItem(
	QTreeWidgetItem* parent,
	const struct iso8825_tlv_t* tlv,
	bool autoExpand
)
: QTreeWidgetItem(parent, 8825)
{
	struct emv_tlv_t emv_tlv;
	struct emv_tlv_info_t info;
	QByteArray valueStr(2048, 0);
	QString fieldStr;

	emv_tlv.ber = *tlv;
	emv_tlv_get_info(&emv_tlv, &info, valueStr.data(), valueStr.size());
	fieldStr = buildFieldString(tlv->tag, info.tag_name);
	setText(0, fieldStr);

	if (iso8825_ber_is_constructed(tlv)) {
		// Always expand constructed fields
		autoExpand = true;
	} else {
		// Always add raw value bytes for primitive fields
		addDescRaw(this, tlv->value, tlv->length);

		if (valueStrIsList(valueStr)) {
			addDescStringList(this, valueStr);
		} else if (valueStr[0]) {
			addDescString(this, info.format, valueStr);
		} else if (info.format == EMV_FORMAT_DOL) {
			addDescDol(this, tlv->value, tlv->length);
		} else if (info.format == EMV_FORMAT_TAG_LIST) {
			addDescTagList(this, tlv->value, tlv->length);
		}
	}

	setExpanded(autoExpand);
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

static bool valueStrIsList(const QByteArray& str)
{
	if (!str[0]) {
		return false;
	}

	// If the last character is a newline, assume it's a string list
	return str[qstrnlen(str.constData(), str.size()) - 1] == '\n';
}

static QTreeWidgetItem* addDescStringList(
	EmvTreeItem* item,
	const QByteArray& valueStr
)
{
	if (!valueStr[0]) {
		return nullptr;
	}

	QTreeWidgetItem* descItem = new QTreeWidgetItem(
		item,
		QStringList(
			QString::fromUtf8(valueStr).trimmed() // Trim trailing newline
		)
	);
	descItem->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsEnabled);

	return descItem;
}

static QTreeWidgetItem* addDescString(
	EmvTreeItem* item,
	const emv_format_t format,
	const QByteArray& valueStr
)
{
	QString descStr;

	if (!valueStr[0]) {
		return nullptr;
	}

	// Use quotes for strings and parentheses for everything else
	if (format == EMV_FORMAT_A ||
		format == EMV_FORMAT_AN ||
		format == EMV_FORMAT_ANS
	) {
		descStr = QStringLiteral("\"") +
			valueStr.constData() +
			QStringLiteral("\"");
	} else {
		descStr = QString::fromUtf8(valueStr);
	}

	QTreeWidgetItem* descItem = new QTreeWidgetItem(
		item,
		QStringList(descStr)
	);
	descItem->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsEnabled);

	return descItem;
}

static QTreeWidgetItem* addDescDol(
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

		QTreeWidgetItem* descItem = new QTreeWidgetItem(
			dolItem,
			QStringList(
				buildFieldString(entry.tag, info.tag_name, entry.length)
			)
		);
		descItem->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsEnabled);
	}
	if (r < 0) {
		qDebug("emv_dol_itr_next() failed; r=%d", r);
		return nullptr;
	}

	return dolItem;
}

static QTreeWidgetItem* addDescTagList(
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

		QTreeWidgetItem* descItem = new QTreeWidgetItem(
			tlItem,
			QStringList(
				buildFieldString(tag, info.tag_name)
			)
		);
		descItem->setFlags(Qt::ItemNeverHasChildren | Qt::ItemIsEnabled);

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

static QTreeWidgetItem* addDescRaw(
	EmvTreeItem* item,
	const void* ptr,
	std::size_t len
)
{
	QTreeWidgetItem* descItem = new QTreeWidgetItem(
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
	descItem->setFlags(Qt::ItemNeverHasChildren);

	return descItem;
}
