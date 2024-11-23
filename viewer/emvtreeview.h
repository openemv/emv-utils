/**
 * @file emvtreeview.h
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

#ifndef EMV_TREE_VIEW_H
#define EMV_TREE_VIEW_H

#include <QtWidgets/QTreeWidget>

class EmvTreeView : public QTreeWidget
{
	Q_OBJECT
	Q_PROPERTY(bool ignorePadding READ ignorePadding WRITE setIgnorePadding)
	Q_PROPERTY(bool decodeFields READ decodeFields WRITE setDecodeFields)

public:
	EmvTreeView(QWidget* parent);

public slots:
	unsigned int populateItems(const QByteArray& data);
	void setIgnorePadding(bool enabled) { m_ignorePadding = enabled; }
	void setDecodeFields(bool enabled);

public:
	bool ignorePadding() const { return m_ignorePadding; }
	bool decodeFields() const { return m_decodeFields; }

private:
	bool m_ignorePadding = false;
	bool m_decodeFields = true;
};

#endif
