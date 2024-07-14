/**
 * @file emv-viewer-mainwindow.cpp
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

#include "emv-viewer-mainwindow.h"
#include "emvhighlighter.h"
#include "emvtreeview.h"

#include <QtCore/QStringLiteral>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QSettings>
#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>
#include <QtGui/QDesktopServices>

#include <cctype>

EmvViewerMainWindow::EmvViewerMainWindow(QWidget* parent)
: QMainWindow(parent)
{
	// Prepare timer used to bundle tree view updates. Do this before setupUi()
	// calls QMetaObject::connectSlotsByName() to ensure that auto-connect
	// works for on_updateTimer_timeout().
	updateTimer = new QTimer(this);
	updateTimer->setObjectName(QStringLiteral("updateTimer"));
	updateTimer->setSingleShot(true);

	// Setup UI widgets
	setupUi(this);
	setWindowTitle(windowTitle().append(QStringLiteral(" (") + qApp->applicationVersion() + QStringLiteral(")")));

	// Note that EmvHighlighter assumes that all blocks are processed in order
	// for every change to the text. Therefore rehighlight() must be called
	// whenever the widget text changes. See on_dataEdit_textChanged().
	highlighter = new EmvHighlighter(dataEdit->document());

	// Set initial state of checkboxes for highlighter and tree view because
	// checkboxes will only emit a stateChanged signal if loadSettings()
	// changes the value to be different from the initial state.
	highlighter->setIgnorePadding(paddingCheckBox->isChecked());
	treeView->setDecodeFields(decodeCheckBox->isChecked());

	// Display copyright, license and disclaimer notice
	descriptionText->appendHtml(QStringLiteral(
		"Copyright 2021-2024 <a href='https://github.com/leonlynch'>Leon Lynch</a><br/><br/>"
		"<a href='https://github.com/openemv/emv-utils'>This program</a> is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 3 as published by the Free Software Foundation.<br/>"
		"<a href='https://github.com/openemv/emv-utils'>This program</a> is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.<br/>"
		"See <a href='https://raw.githubusercontent.com/openemv/emv-utils/master/viewer/LICENSE.gpl'>LICENSE.gpl</a> file for more details.<br/><br/>"
		"<a href='https://github.com/openemv/emv-utils'>This program</a> uses various libraries including:<br/>"
		"- <a href='https://github.com/openemv/emv-utils'>emv-utils</a> (licensed under <a href='https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html'>LGPL v2.1</a>)<br/>"
		"- <a href='https://www.qt.io'>Qt</a> (licensed under <a href='https://www.gnu.org/licenses/lgpl-3.0.html'>LGPL v3</a>)<br/>"
		"<br/>"
		"EMV\xAE is a registered trademark in the U.S. and other countries and an unregistered trademark elsewhere. The EMV trademark is owned by EMVCo, LLC. "
		"This program refers to \"EMV\" only to indicate the specifications involved and does not imply any affiliation, endorsement or sponsorship by EMVCo in any way."
	));

	// Load previous UI values
	loadSettings();

	// Let description scroll to top after restoring settings and updating
	// the content
	QTimer::singleShot(0, [this]() {
		descriptionText->verticalScrollBar()->triggerAction(QScrollBar::SliderToMinimum);
	});
}

void EmvViewerMainWindow::closeEvent(QCloseEvent* event)
{
	// Save current UI values
	saveSettings();
}

void EmvViewerMainWindow::loadSettings()
{
	QSettings settings;
	QList<QCheckBox*> check_box_list = findChildren<QCheckBox*>();

	settings.beginGroup(QStringLiteral("settings"));

	// Iterate over checkboxes and load from settings
	for (auto check_box : check_box_list) {
		Qt::CheckState state;

		if (!settings.contains(check_box->objectName())) {
			// No value to load
			continue;
		}

		state = static_cast<Qt::CheckState>(settings.value(check_box->objectName()).toUInt());
		check_box->setCheckState(state);
	}

	// Load window and splitter states from settings
	restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
	splitter->restoreState(settings.value(QStringLiteral("splitterState")).toByteArray());
	if (settings.contains(QStringLiteral("splitterBottomState"))) {
		splitterBottom->restoreState(settings.value(QStringLiteral("splitterBottomState")).toByteArray());
	} else {
		// Favour tree view child if no saved state available
		splitterBottom->setSizes({99999, 1});
	}
}

void EmvViewerMainWindow::saveSettings() const
{
	QSettings settings;
	QList<QCheckBox*> check_box_list = findChildren<QCheckBox*>();

	// Start with blank settings
	settings.clear();
	settings.beginGroup(QStringLiteral("settings"));

	// Iterate over checkboxes and save to settings
	for (auto check_box : check_box_list) {
		if (!check_box->isChecked()) {
			// Don't save unchecked checkboxes
			continue;
		}

		settings.setValue(check_box->objectName(), check_box->checkState());
	}

	// Save window and splitter states
	settings.setValue(QStringLiteral("geometry"), saveGeometry());
	settings.setValue(QStringLiteral("splitterState"), splitter->saveState());
	settings.setValue(QStringLiteral("splitterBottomState"), splitterBottom->saveState());


	settings.sync();
}

void EmvViewerMainWindow::parseData()
{
	QString str;
	int validLen;
	QByteArray data;
	bool paddingIsPossible = false;
	unsigned int validBytes;

	str = dataEdit->toPlainText();
	if (str.isEmpty()) {
		treeView->clear();
		return;
	}

	// Remove all whitespace from hex string
	str = str.simplified().remove(' ');
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

	// Determine whether invalid data might be padding
	if (paddingCheckBox->isChecked() && validLen == str.length()) {
		// Input data is a valid hex string and therefore the possibility
		// exists that if BER decoding fails, the remaining data might be
		// cryptographic padding
		paddingIsPossible = true;
	}

	data = QByteArray::fromHex(str.left(validLen).toUtf8());
	validBytes = treeView->populateItems(data);
	validLen = validBytes * 2;

	if (validLen < str.length()) {
		bool isPadding;
		QString itemStr;
		QColor itemColor;

		// Determine whether invalid data is padding and prepare item details
		// accordingly
		if (paddingIsPossible &&
			data.size() - validBytes > 0 &&
			(
				((data.size() & 0x7) == 0 && data.size() - validBytes < 8) ||
				((data.size() & 0xF) == 0 && data.size() - validBytes < 16)
			)
		) {
			// Invalid data is likely to be padding
			isPadding = true;
			itemStr = QStringLiteral("Padding: ");
			itemColor = Qt::darkGray;
		} else {
			// Invalid data is either absent or unlikely to be padding
			isPadding = false;
			itemStr = QStringLiteral("Remaining invalid data: ");
			itemColor = Qt::red;
		}

		QTreeWidgetItem* item = new QTreeWidgetItem(
			treeView->invisibleRootItem(),
			QStringList(itemStr + str.right(str.length() - validLen))
		);
		item->setDisabled(isPadding);
		item->setForeground(0, itemColor);
	}
}

void EmvViewerMainWindow::on_updateTimer_timeout()
{
	parseData();
}

void EmvViewerMainWindow::on_dataEdit_textChanged()
{
	// Rehighlight when text changes. This is required because EmvHighlighter
	// assumes that all blocks are processed in order for every change to the
	// text. Note that rehighlight() will also re-trigger the textChanged()
	// signal and therefore signals must be blocked for the duration of
	// rehighlight().
	dataEdit->blockSignals(true);
	highlighter->parseBlocks();
	highlighter->rehighlight();
	dataEdit->blockSignals(false);

	// Bundle updates by restarting the timer every time the data changes
	updateTimer->start(200);
}

void EmvViewerMainWindow::on_paddingCheckBox_stateChanged(int state)
{
	// Rehighlight when padding state changes. Note that rehighlight() will
	// also trigger the textChanged() signal which will in turn update the tree
	// view item associated with invalid data or padding as well.
	highlighter->setIgnorePadding(state != Qt::Unchecked);
	highlighter->rehighlight();
}

void EmvViewerMainWindow::on_decodeCheckBox_stateChanged(int state)
{
	treeView->setDecodeFields(state != Qt::Unchecked);
}

void EmvViewerMainWindow::on_descriptionText_linkActivated(const QString& link)
{
	// Open link using external application
	QDesktopServices::openUrl(link);
}
