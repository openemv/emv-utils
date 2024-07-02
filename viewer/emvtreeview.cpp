/**
 * @file emvtreeview.cpp
 * @brief QTreeWidget derivative for viewing EMV data
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

#include "emvtreeview.h"
#include "emvtreeitem.h"

#include "iso8825_ber.h"

EmvTreeView::EmvTreeView(QWidget* parent)
: QTreeWidget(parent)
{
}

static bool parseData(
	QTreeWidgetItem* parent,
	const void* ptr,
	unsigned int len,
	unsigned int* validBytes
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
		EmvTreeItem* item = new EmvTreeItem(parent, &tlv);

		if (iso8825_ber_is_constructed(&tlv)) {
			// If the field is constructed, only consider the tag and length
			// to be valid until the value has been parsed. The fields inside
			// the value will be added when they are parsed.
			*validBytes += (r - tlv.length);

			// Recursively parse constructed fields
			valid = parseData(item, tlv.value, tlv.length, validBytes);
			if (!valid) {
				qDebug("parseBerData() failed; validBytes=%u", *validBytes);
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

unsigned int EmvTreeView::populateItems(const QByteArray& data)
{
	unsigned int validBytes = 0;

	// For now, clear the widget before repopulating it. In future, the widget
	// should be updated incrementally instead.
	clear();

	::parseData(
		invisibleRootItem(),
		data.constData(), data.size(), &validBytes
	);

	return validBytes;
}
