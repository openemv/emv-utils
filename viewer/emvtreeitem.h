/**
 * @file emvtreeitem.h
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

#ifndef EMV_TREE_ITEM_H
#define EMV_TREE_ITEM_H

#include <QtWidgets/QTreeWidgetItem>

// Forward declarations
struct iso8825_tlv_t;

class EmvTreeItem : public QTreeWidgetItem
{
public:
	EmvTreeItem(
		QTreeWidgetItem* parent,
		const struct iso8825_tlv_t* tlv,
		bool autoExpand = true
	);
};

#endif