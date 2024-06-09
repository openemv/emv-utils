/**
 * @file emv-viewer.cpp
 * @brief Simple EMV data viewer using Qt
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

#include <QtWidgets/QApplication>

#include "emv_viewer_config.h"
#include "emv-viewer-mainwindow.h"

int main(int argc, char** argv)
{
	QApplication app(argc, argv);
	app.setOrganizationName("OpenEMV");
	app.setOrganizationDomain("openemv.org");
	app.setApplicationName("emv-viewer");
	app.setApplicationVersion(EMV_VIEWER_VERSION_STRING);

	EmvViewerMainWindow mainwindow;
	mainwindow.show();

	return app.exec();
}
