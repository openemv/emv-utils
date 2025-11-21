/**
 * @file emvtreeview.cpp
 * @brief QTreeWidget derivative for viewing EMV data
 *
 * Copyright 2024-2025 Leon Lynch
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

#include "emvtreeview.h"
#include "emvtreeitem.h"
#include "emvtlvinfo.h"

#include "iso8825_ber.h"

#include <QtWidgets/QTreeWidgetItemIterator>

EmvTreeView::EmvTreeView(QWidget* parent)
: QTreeWidget(parent)
{
}

static bool parseData(
	QTreeWidgetItem* parent,
	const void* ptr,
	unsigned int len,
	bool ignorePadding,
	bool decodeFields,
	bool decodeObjects,
	unsigned int* totalValidBytes
)
{
	int r;
	unsigned int validBytes = 0;
	struct iso8825_ber_itr_t itr;
	struct iso8825_tlv_t tlv;

	r = iso8825_ber_itr_init(ptr, len, &itr);
	if (r) {
		qWarning("iso8825_ber_itr_init() failed; r=%d", r);
		return false;
	}

	while ((r = iso8825_ber_itr_next(&itr, &tlv)) > 0) {
		unsigned int fieldLength = r;

		EmvTreeItem* item = new EmvTreeItem(
			parent,
			*totalValidBytes,
			fieldLength,
			&tlv,
			decodeFields,
			decodeObjects
		);

		if (iso8825_ber_is_constructed(&tlv)) {
			// If the field is constructed, only consider the tag and length
			// to be valid until the value has been parsed. The fields inside
			// the value will be added when they are parsed.
			validBytes += (fieldLength - tlv.length);
			*totalValidBytes += (fieldLength - tlv.length);

			// Recursively parse constructed fields
			bool valid;
			valid = parseData(
				item,
				tlv.value,
				tlv.length,
				ignorePadding,
				decodeFields,
				decodeObjects,
				totalValidBytes
			);
			if (!valid) {
				qDebug("parseData() failed; totalValidBytes=%u", *totalValidBytes);

				// Return here instead of breaking out to avoid repeated
				// processing of the error by recursive callers
				return false;
			}
			validBytes += tlv.length;

			// Attempt to decode field as ASN.1 object
			r = iso8825_ber_asn1_object_decode(&tlv, NULL);
			if (r > 0) {
				// For ASN.1 objects, hide the OID (first child) because its
				// value string is already reflected in the value string of the
				// current ASN.1 object.
				if (item->child(0) &&
					item->child(0)->type() == EmvTreeItemType
				) {
					EmvTreeItem* etItem = reinterpret_cast<EmvTreeItem*>(item->child(0));
					etItem->setHideWhenDecodingObject(true);
					etItem->render(decodeFields, decodeObjects);
				}
			}

		} else {
			// If the field is not constructed, consider all of the bytes to
			// be valid BER encoded data
			validBytes += fieldLength;
			*totalValidBytes += fieldLength;
		}
	}
	if (r < 0) {
		// Determine whether invalid data is padding and prepare item details
		// accordingly
		if (ignorePadding &&
			len > validBytes &&
			(
				((len & 0x7) == 0 && len - validBytes < 8) ||
				((len & 0xF) == 0 && len - validBytes < 16)
			)
		) {
			// Invalid data is likely to be padding
			EmvTreeItem* item = new EmvTreeItem(
				parent,
				*totalValidBytes,
				len - validBytes,
				"Padding",
				reinterpret_cast<const char*>(ptr) + validBytes
			);
			item->setForeground(0, Qt::darkGray);

			// If the remaining bytes appear to be padding, consider these
			// bytes to be valid
			*totalValidBytes += len - validBytes;
			validBytes = len;

		} else {
			qDebug("iso8825_ber_itr_next() failed; r=%d", r);
			return false;
		}
	}

	return true;
}

void EmvTreeView::clear()
{
	EmvTlvInfo::clearDefaultSources();
	QTreeWidget::clear();
}

unsigned int EmvTreeView::populateItems(const QByteArray& data)
{
	unsigned int totalValidBytes = 0;

	// For now, clear the widget before repopulating it. In future, the widget
	// should be updated incrementally instead.
	clear();

	// Cache all available fields for better output
	EmvTlvInfo::setDefaultSources(data);

	::parseData(
		invisibleRootItem(),
		data.constData(),
		data.size(),
		m_ignorePadding,
		m_decodeFields,
		m_decodeObjects,
		&totalValidBytes
	);

	return totalValidBytes;
}

void EmvTreeView::setDecodeFields(bool enabled)
{
	if (m_decodeFields == enabled) {
		// No change
		return;
	}
	m_decodeFields = enabled;

	// Visit all EMV children recursively and re-render them according to the
	// current state
	QTreeWidgetItemIterator itr (this);
	while (*itr) {
		QTreeWidgetItem* item = *itr;
		if (item->type() == EmvTreeItemType) {
			EmvTreeItem* etItem = reinterpret_cast<EmvTreeItem*>(item);
			etItem->render(m_decodeFields, m_decodeObjects);
		}
		++itr;
	}
}

void EmvTreeView::setDecodeObjects(bool enabled)
{
	if (m_decodeObjects == enabled) {
		// No change
		return;
	}
	m_decodeObjects = enabled;

	// Visit all EMV children recursively and re-render them according to the
	// current state
	QTreeWidgetItemIterator itr (this);
	while (*itr) {
		QTreeWidgetItem* item = *itr;
		if (item->type() == EmvTreeItemType) {
			EmvTreeItem* etItem = reinterpret_cast<EmvTreeItem*>(item);
			etItem->render(m_decodeFields, m_decodeObjects);
		}
		++itr;
	}
}
