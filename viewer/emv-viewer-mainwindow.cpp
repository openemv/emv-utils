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

#include <QtCore/QStringLiteral>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QSettings>
#include <QtCore/QTimer>
#include <QtCore/QStringListModel> // TODO: remove/replace
#include <QtWidgets/QApplication>
#include <QtWidgets/QScrollBar>
#include <QtGui/QDesktopServices>

EmvViewerMainWindow::EmvViewerMainWindow(QWidget* parent)
: QMainWindow(parent)
{
	// Prepare timer used to bundle tree view updates. Do this before setupUi()
	// calls QMetaObject::connectSlotsByName() to ensure that auto-connect
	// works for on_modelUpdateTimer_timeout().
	modelUpdateTimer = new QTimer(this);
	modelUpdateTimer->setObjectName(QStringLiteral("modelUpdateTimer"));
	modelUpdateTimer->setSingleShot(true);

	// Setup UI widgets
	setupUi(this);
	setWindowTitle(windowTitle().append(QStringLiteral(" (") + qApp->applicationVersion() + QStringLiteral(")")));

	// Note that EmvHighlighter assumes that all blocks are processed in order
	// for every change to the text. Therefore rehighlight() must be called
	// whenever the widget text changes. See on_dataEdit_textChanged().
	highlighter = new EmvHighlighter(dataEdit->document());

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

	// Prepare model used for tree view
	model = new QStringListModel(treeView);
	treeView->setModel(model);
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
	// Test model update bundling by appending rows to a simple string list
	// model. This will be replaced by the appropriate model in future.
	if (model->insertRow(model->rowCount())) {
		QModelIndex index = model->index(model->rowCount() - 1, 0);
		model->setData(index, "asdf");
	}
}

void EmvViewerMainWindow::on_dataEdit_textChanged()
{
	// Rehighlight when text changes. This is required because EmvHighlighter
	// assumes that all blocks are processed in order for every change to the
	// text. Note that rehighlight() will also re-trigger the textChanged()
	// signal and therefore signals must be blocked for the duration of
	// rehighlight().
	dataEdit->blockSignals(true);
	highlighter->rehighlight();
	dataEdit->blockSignals(false);

	// Bundle updates by restarting the timer every time the data changes
	modelUpdateTimer->start(300);
}

void EmvViewerMainWindow::on_descriptionText_linkActivated(const QString& link)
{
	// Open link using external application
	QDesktopServices::openUrl(link);
}
