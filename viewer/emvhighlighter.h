/**
 * @file emvhighlighter.h
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

#ifndef EMV_HIGHLIGHTER_H
#define EMV_HIGHLIGHTER_H

#include <QtGui/QSyntaxHighlighter>

// Forward declarations
class QTextDocument;

class EmvHighlighter : public QSyntaxHighlighter
{
	Q_OBJECT

public:
	explicit EmvHighlighter(QTextDocument* parent)
	: QSyntaxHighlighter(parent)
	{}

	virtual void highlightBlock(const QString& text) override;
};

#endif