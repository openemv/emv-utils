/**
 * @file emv-viewer-mainwindow.cpp
 * @brief Main window of EMV Viewer
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

#include "emv-viewer-mainwindow.h"
#include "emvhighlighter.h"
#include "emvtreeview.h"
#include "emvtreeitem.h"

#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QSettings>
#include <QtCore/QString>
#include <QtCore/QStringLiteral>
#include <QtCore/QTimer>
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QStatusBar>
#include <QtGui/QClipboard>
#include <QtGui/QDesktopServices>

static constexpr int STATUS_MESSAGE_TIMEOUT_MS = 2000; // Milliseconds

EmvViewerMainWindow::EmvViewerMainWindow(
	QWidget* parent,
	QString overrideData,
	int overrideDecodeCheckBoxState
)
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
	setWindowIcon(QIcon(":icons/openemv_emv_utils_512x512.png"));
	setWindowTitle(windowTitle().append(QStringLiteral(" (") + qApp->applicationVersion() + QStringLiteral(")")));

	// Note that EmvHighlighter assumes that all blocks are processed in order
	// for every change to the text. Therefore rehighlight() must be called
	// whenever the widget text changes. See on_dataEdit_textChanged().
	highlighter = new EmvHighlighter(dataEdit->document());

	// Set initial state of checkboxes for highlighter and tree view because
	// checkboxes will only emit a stateChanged signal if loadSettings()
	// changes the value to be different from the initial state.
	highlighter->setEmphasiseTags(tagsCheckBox->isChecked());
	highlighter->setIgnorePadding(paddingCheckBox->isChecked());
	treeView->setIgnorePadding(paddingCheckBox->isChecked());
	treeView->setDecodeFields(decodeFieldsCheckBox->isChecked());
	treeView->setDecodeObjects(decodeObjectsCheckBox->isChecked());
	treeView->setCopyButtonEnabled(true);

	// Load previous UI values
	loadSettings();

	// Load values from command line options
	if (!overrideData.isEmpty()) {
		dataEdit->setPlainText(overrideData);
	}
	if (overrideDecodeCheckBoxState > -1) {
		decodeFieldsCheckBox->setCheckState(static_cast<Qt::CheckState>(overrideDecodeCheckBoxState));
	}

	// Default to showing legal text in description widget
	displayLegal();
}

void EmvViewerMainWindow::closeEvent(QCloseEvent* event)
{
	// Save current UI values
	saveSettings();
	event->accept();
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

	// Load window and bottom splitter states from settings
	restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());
	if (settings.contains(QStringLiteral("splitterBottomState"))) {
		splitterBottom->restoreState(settings.value(QStringLiteral("splitterBottomState")).toByteArray());
	} else {
		// Favour tree view child if no saved state available
		splitterBottom->setSizes({99999, 1});
	}

	// Load input data and main splitter state
	if (rememberCheckBox->isChecked()) {
		dataEdit->setPlainText(settings.value(dataEdit->objectName()).toString());
		splitter->restoreState(settings.value(QStringLiteral("splitterState")).toByteArray());
	} else {
		// Favour bottom child if no saved state available
		splitter->setSizes({1, 99999});
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

	// Save window and bottom splitter states
	settings.setValue(QStringLiteral("geometry"), saveGeometry());
	settings.setValue(QStringLiteral("splitterBottomState"), splitterBottom->saveState());

	// Save input data and main splitter state
	if (rememberCheckBox->isChecked()) {
		settings.setValue(dataEdit->objectName(), dataEdit->toPlainText());
		settings.setValue(QStringLiteral("splitterState"), splitter->saveState());
	}

	settings.sync();
}

void EmvViewerMainWindow::displayLegal()
{
	// Display copyright, license and disclaimer notice
	descriptionText->clear();
	descriptionText->appendHtml(QStringLiteral(
		"Copyright 2021-2025 <a href='https://github.com/leonlynch'>Leon Lynch</a><br/><br/>"
		"<a href='https://github.com/openemv/emv-utils'>This program</a> is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License version 3 as published by the Free Software Foundation.<br/>"
		"<a href='https://github.com/openemv/emv-utils'>This program</a> is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.<br/>"
		"See <a href='https://raw.githubusercontent.com/openemv/emv-utils/master/viewer/LICENSE.gpl'>LICENSE.gpl</a> file for more details.<br/><br/>"
		"<a href='https://github.com/openemv/emv-utils'>This program</a> uses various libraries including:<br/>"
		"- <a href='https://github.com/openemv/emv-utils'>emv-utils</a> (licensed under <a href='https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html'>LGPL v2.1</a>)<br/>"
		"- <a href='https://github.com/Mbed-TLS/mbedtls'>MbedTLS</a> (licensed under <a href='http://www.apache.org/licenses/LICENSE-2.0'>Apache License v2</a>)<br/>"
		"- <a href='https://www.qt.io'>Qt</a> (licensed under <a href='https://www.gnu.org/licenses/lgpl-3.0.html'>LGPL v3</a>)<br/>"
		"<br/>"
		"EMV\xAE is a registered trademark in the U.S. and other countries and an unregistered trademark elsewhere. The EMV trademark is owned by EMVCo, LLC. "
		"This program refers to \"EMV\" only to indicate the specifications involved and does not imply any affiliation, endorsement or sponsorship by EMVCo in any way."
	));

	// Let description scroll to top after updating content
	QTimer::singleShot(0, [this]() {
		descriptionText->verticalScrollBar()->triggerAction(QScrollBar::SliderToMinimum);
	});
}

void EmvViewerMainWindow::updateTreeView()
{
	QString str;

	str = dataEdit->toPlainText();
	if (str.isEmpty()) {
		treeView->clear();
		return;
	}
	treeView->populateItems(str);
}

void EmvViewerMainWindow::on_updateTimer_timeout()
{
	updateTreeView();
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

void EmvViewerMainWindow::on_tagsCheckBox_stateChanged(int state)
{
	// Rehighlight when emphasis state changes. Note that rehighlight() will
	// also re-trigger the textChanged() signal and therefore signals must be
	// blocked for the duration of rehighlight().
	dataEdit->blockSignals(true);
	highlighter->setEmphasiseTags(state != Qt::Unchecked);
	highlighter->rehighlight();
	dataEdit->blockSignals(false);
}

void EmvViewerMainWindow::on_paddingCheckBox_stateChanged(int state)
{
	// Rehighlight when padding state changes. Note that rehighlight() will
	// also trigger the textChanged() signal which will in turn update the tree
	// view item associated with invalid data or padding as well.
	highlighter->setIgnorePadding(state != Qt::Unchecked);
	highlighter->rehighlight();

	// Note that tree view data must be reparsed when padding state changes
	treeView->setIgnorePadding(state != Qt::Unchecked);
	updateTreeView();
}

void EmvViewerMainWindow::on_decodeFieldsCheckBox_stateChanged(int state)
{
	treeView->setDecodeFields(state != Qt::Unchecked);
}

void EmvViewerMainWindow::on_decodeObjectsCheckBox_stateChanged(int state)
{
	treeView->setDecodeObjects(state != Qt::Unchecked);
}

void EmvViewerMainWindow::on_treeView_populateItemsCompleted(
	unsigned int validBytes,
	unsigned int fieldCount,
	unsigned int invalidChars
)
{
	QString msg;

	if (validBytes == 0 && invalidChars == 0) {
		// Empty input
		return;
	}

	if (invalidChars > 0) {
		msg = tr("Parsed %1 bytes: found %2 fields and %3 invalid characters")
			.arg(validBytes)
			.arg(fieldCount)
			.arg(invalidChars);
	} else {
		msg = tr("Parsed %1 bytes: found %2 fields")
			.arg(validBytes)
			.arg(fieldCount);
	}

	QMainWindow::statusBar()->showMessage(msg);
}

void EmvViewerMainWindow::on_treeView_itemPressed(QTreeWidgetItem* item, int column)
{
	if (item->type() == EmvTreeItemType) {
		EmvTreeItem* etItem = reinterpret_cast<EmvTreeItem*>(item);

		// Highlight selected item in input data. Note that rehighlight() will
		// also trigger the textChanged() signal and therefore signals must be
		// blocked for the duration of rehighlight().
		dataEdit->blockSignals(true);
		highlighter->setSelection(
			etItem->srcOffset() * 2,
			etItem->srcLength() * 2
		);
		highlighter->rehighlight();
		dataEdit->blockSignals(false);

		// Show description of selected item if it has a name.
		// Otherwise show legal text.
		descriptionText->clear();
		if (!etItem->tagName().isEmpty()) {
			descriptionText->appendHtml(
				QStringLiteral("<b>") +
				etItem->tagName() +
				QStringLiteral("</b><br/><br/>") +
				etItem->tagDescription().toHtmlEscaped().replace('\n', QStringLiteral("<br/>"))
			);

			// Let description scroll to top after updating content
			QTimer::singleShot(0, [this]() {
				descriptionText->verticalScrollBar()->triggerAction(QScrollBar::SliderToMinimum);
			});

			return;
		}
	}

	displayLegal();
}

void EmvViewerMainWindow::on_treeView_itemCopyClicked(QTreeWidgetItem* item)
{
	if (!item) {
		return;
	}

	QString str = treeView->toClipboardText(item, QStringLiteral("  "), 0);
	QApplication::clipboard()->setText(str);

	QMainWindow::statusBar()->showMessage(tr("Copied selected item to clipboard"), STATUS_MESSAGE_TIMEOUT_MS);
}

void EmvViewerMainWindow::on_actionCopyAll_triggered()
{
	QString str = treeView->toClipboardText(QStringLiteral("  "), 0);
	QApplication::clipboard()->setText(str);

	QMainWindow::statusBar()->showMessage(tr("Copied all items to clipboard"), STATUS_MESSAGE_TIMEOUT_MS);
}

void EmvViewerMainWindow::on_descriptionText_linkActivated(const QString& link)
{
	// Open link using external application
	QDesktopServices::openUrl(link);
}
