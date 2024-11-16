/**
 * @file betterplaintextedit.h
 * @brief QPlainTextEdit derivative that emits events when links are clicked
 *
 * Copyright 2023 Leon Lynch
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

#ifndef BETTER_PLAIN_TEXT_EDIT_H
#define BETTER_PLAIN_TEXT_EDIT_H

#include <QtWidgets/QPlainTextEdit>

class BetterPlainTextEdit : public QPlainTextEdit
{
	Q_OBJECT

public:
	explicit BetterPlainTextEdit(QWidget* parent = nullptr)
	: QPlainTextEdit(parent)
	{}

	virtual void mousePressEvent(QMouseEvent* e) override
	{
		if (e->button() & Qt::LeftButton) {
			QString href = anchorAt(e->pos());
			if (!href.isEmpty()) {
				emit linkActivated(href);
			}
		}

		QPlainTextEdit::mousePressEvent(e);
	}

signals:
	void linkActivated(QString href);
};

#endif
