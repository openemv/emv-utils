/**
 * @file emv-viewer-mainwindow.h
 * @brief Main window of EMV Viewer
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

#ifndef EMV_VIEWER_MAINWINDOW_H
#define EMV_VIEWER_MAINWINDOW_H

#include <QtWidgets/QMainWindow>

#include "ui_emv-viewer-mainwindow.h"

// Forward declarations
class QTimer;
class EmvHighlighter;

class EmvViewerMainWindow : public QMainWindow, private Ui::MainWindow
{
	Q_OBJECT

public:
	explicit EmvViewerMainWindow(QWidget* parent = nullptr);

protected:
	void closeEvent(QCloseEvent* event) override;

private:
	void loadSettings();
	void saveSettings() const;
	void displayLegal();

	void parseData();

private slots: // connect-by-name helper functions
	void on_updateTimer_timeout();
	void on_dataEdit_textChanged();
	void on_tagsCheckBox_stateChanged(int state);
	void on_paddingCheckBox_stateChanged(int state);
	void on_decodeCheckBox_stateChanged(int state);
	void on_treeView_itemPressed(QTreeWidgetItem* item, int column);
	void on_descriptionText_linkActivated(const QString& link);

protected:
	QTimer* updateTimer;
	EmvHighlighter* highlighter;
};

#endif
