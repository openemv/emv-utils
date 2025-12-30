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
#include <QtCore/QVector>

// Forward declarations
class QTextDocument;

class EmvHighlighter : public QSyntaxHighlighter
{
	Q_OBJECT
	Q_PROPERTY(bool emphasiseTags READ emphasiseTags WRITE setEmphasiseTags)
	Q_PROPERTY(bool ignorePadding READ ignorePadding WRITE setIgnorePadding)

public:
	explicit EmvHighlighter(QTextDocument* parent)
	: QSyntaxHighlighter(parent)
	{}

	virtual void highlightBlock(const QString& text) override;

public slots:
	void parseBlocks();
	void setEmphasiseTags(bool enabled) { m_emphasiseTags = enabled; }
	void setIgnorePadding(bool enabled) { m_ignorePadding = enabled; }
	void setSelection(int start, int count) { m_selectionStart = start; m_selectionCount = count; }
	void clearSelection() { m_selectionStart = -1; m_selectionCount = 0; }

public:
	bool emphasiseTags() const { return m_emphasiseTags; }
	bool ignorePadding() const { return m_ignorePadding; }

	struct Position {
		unsigned int offset;
		unsigned int length;
	};

private:
	bool m_emphasiseTags = false;
	bool m_ignorePadding = false;
	int m_selectionStart = -1;
	int m_selectionCount = 0;
	unsigned int strLen;
	unsigned int hexStrLen;
	unsigned int berStrLen;
	QVector<Position> tagPositions;
	QVector<Position> paddingPositions;
};

#endif
