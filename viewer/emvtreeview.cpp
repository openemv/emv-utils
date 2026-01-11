/**
 * @file emvtreeview.cpp
 * @brief QTreeWidget derivative for viewing EMV data
 *
 * Copyright 2024-2026 Leon Lynch
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

#include <QtCore/QSize>
#include <QtCore/QTimer>
#include <QtGui/QFontMetrics>
#include <QtGui/QIcon>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTreeWidgetItemIterator>

#include <cctype>

class EmvTreeItemButton : public QPushButton
{
public:
	EmvTreeItemButton(QWidget* parent)
	: QPushButton(parent)
	{
		setFlat(true);
	}

	virtual QSize sizeHint() const override {
		// Override sizeHint with font height to prevent single row items
		// from expanding when the button is added
		int textHeight = fontMetrics().height();
		return QSize(16, textHeight);
	}
};

class EmvTreeItemCopyButton : public EmvTreeItemButton {
public:
	EmvTreeItemCopyButton(QWidget* parent)
	: EmvTreeItemButton(parent) {

		QIcon icon = QIcon::fromTheme(QStringLiteral("edit-copy"));
		if (!icon.isNull()) {
			setIcon(icon);
		} else {
			// Use Unicode clipboard symbol as text when theme icon is not
			// available
			setText(QStringLiteral("\u2398"));
		}

		setToolTip(tr("Copy selected field to clipboard"));
	}
};

EmvTreeView::EmvTreeView(QWidget* parent)
: QTreeWidget(parent)
{
	// Defer header/column configuration until after UI file has been processed
	// and columns exist
	QTimer::singleShot(0, this, [this]() {
		header()->setStretchLastSection(false);
		header()->setSectionResizeMode(0, QHeaderView::Stretch);
		header()->setSectionResizeMode(1, QHeaderView::Fixed);
		setColumnWidth(1, 16);
	});
}

void EmvTreeView::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
	QTreeWidget::currentChanged(current, previous);

	// Clicking different columns in the same row selects different indexes of
	// the same item
	if (current.isValid() && previous.isValid() &&
		itemFromIndex(current) == itemFromIndex(previous)
	) {
		return;
	}

	// Remove button from previous selected item
	if (previous.isValid()) {
		QTreeWidgetItem* previousItem = itemFromIndex(previous);
		if (previousItem) {
			QWidget* oldWidget = itemWidget(previousItem, 1);
			if (oldWidget) {
				// Removing the widget will also delete it although the Qt
				// documentation does not mention this. It is likely because
				// setItemWidget() takes ownership and then removeItemWidget()
				// releases that ownership
				removeItemWidget(previousItem, 1);
			}
		}
	}

	// Add button to current selected item
	if (current.isValid() && m_copyButtonEnabled) {
		QTreeWidgetItem* currentItem = itemFromIndex(current);
		if (currentItem) {
			EmvTreeItemCopyButton* button = new EmvTreeItemCopyButton(this);
			connect(button, &QPushButton::clicked, this, [this, currentItem]() {
				emit itemCopyClicked(currentItem);
			});
			setItemWidget(currentItem, 1, button);
		}
	}
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
			len - validBytes > 0 &&
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

unsigned int EmvTreeView::populateItems(const QString& dataStr)
{
	QString str;
	int validLen;
	QByteArray data;
	unsigned int validBytes;

	if (dataStr.isEmpty()) {
		clear();
		return 0;
	}

	// Remove all whitespace from hex string
	str = dataStr.simplified().remove(' ');
	validLen = str.length();

	// Ensure that hex string contains only hex digits
	for (int i = 0; i < validLen; ++i) {
		if (!std::isxdigit(str[i].unicode())) {
			// Only parse up to invalid digit
			validLen = i;
			break;
		}
	}

	// Ensure that hex string has even number of digits
	if (validLen & 0x01) {
		// Odd number of digits. Ignore last digit to see whether parsing can
		// proceed regardless and indicate invalid data later.
		validLen -= 1;
	}

	data = QByteArray::fromHex(str.left(validLen).toUtf8());
	validBytes = populateItems(data);
	validLen = validBytes * 2;

	if (validLen < str.length()) {
		// Remaining data is invalid and unlikely to be padding
		QTreeWidgetItem* item = new QTreeWidgetItem(
			invisibleRootItem(),
			QStringList(
				QStringLiteral("Remaining invalid data: ") +
				str.right(str.length() - validLen)
			)
		);
		item->setDisabled(true);
		item->setForeground(0, Qt::red);
	}

	return validBytes;
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

static QString toClipboardText(
	const QTreeWidgetItem* item,
	const QString& prefix,
	unsigned int depth
)
{
	QString str;
	QString indent;

	if (!item || item->isHidden()) {
		return QString();
	}

	for (unsigned int i = 0; i < depth; ++i) {
		indent += prefix;
	}

	QString itemText = item->text(0);
	QStringList lines = itemText.split('\n');
	for (int i = 0; i < lines.size(); ++i) {
		str += indent + lines[i] + "\n";
	}

	for (int i = 0; i < item->childCount(); ++i) {
		str += ::toClipboardText(item->child(i), prefix, depth + 1);
	}

	return str;
}

QString EmvTreeView::toClipboardText(
	const QString& prefix,
	unsigned int depth
) const
{
	QString str;
	const QTreeWidgetItem* item = invisibleRootItem();

	// Children are iterated here instead of passing invisibleRootItem()
	// directly to ::toClipboardText() because the invisible root has no depth
	// and therefore the children should start at the current depth
	for (int i = 0; i < item->childCount(); ++i) {
		str += ::toClipboardText(item->child(i), prefix, depth);
	}

	return str;
}

QString EmvTreeView::toClipboardText(const QTreeWidgetItem* item, const QString& prefix, unsigned int depth) const
{
	QString str;
	QString indent;

	if (!item) {
		return toClipboardText(prefix, depth);
	}

	return ::toClipboardText(item, prefix, depth);
}
