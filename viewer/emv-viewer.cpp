/**
 * @file emv-viewer.cpp
 * @brief Simple EMV data viewer using Qt
 *
 * Copyright 2024-2025 Leon Lynch
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
#include <QtCore/QByteArray>
#include <QtCore/QCommandLineParser>
#include <QtCore/QString>
#include <QtCore/QStringLiteral>

#include "emv_viewer_config.h"
#include "emv_strings.h"
#include "emv-viewer-mainwindow.h"

#include <cstddef>
#include <cstdio>

#ifdef _WIN32
// For _setmode
#include <fcntl.h>
#include <io.h>
#endif

static QString readHexStringFromStdin()
{
	QByteArray data;

#ifdef _WIN32
	_setmode(_fileno(stdin), _O_BINARY);
#endif

	// Read stdin using std::fread() instead of std::cin to avoid conflicts
	// between different C++ runtimes when packaging for Windows.
	do {
		char buf[1024];
		std::size_t len;

		// Read next block
		len = std::fread(buf, 1, sizeof(buf), stdin);
		if (std::ferror(stdin)) {
			break;
		}

		data.append(buf, len);
	} while (!std::feof(stdin));

	// Convert binary data to uppercase hex string
	return data.toHex().toUpper().constData();
}

int main(int argc, char** argv)
{
	int r;

	QApplication app(argc, argv);
	app.setOrganizationName("OpenEMV");
	app.setOrganizationDomain("openemv.org");
	app.setApplicationName("emv-viewer");
	app.setApplicationVersion(EMV_VIEWER_VERSION_STRING);
	app.setWindowIcon(QIcon(":icons/openemv_emv_utils_512x512.png"));

	QCommandLineParser parser;
	parser.addHelpOption();
	parser.addVersionOption();
	parser.addOptions({ // NOTE: These options intentionally match emv-decode
		{ "isocodes-path", "Override directory path of iso-codes JSON files", "path" },
		{ "mcc-json" , "Override mcc-codes JSON file", "file" },
		{ "ber", "Decode ISO 8825-1 BER encoded data", "data" },
		{ "tlv", "Decode EMV TLV data", "data" },
	});
	parser.process(app);

	QString isocodes_path = parser.value("isocodes-path");
	QString mcc_path = parser.value("mcc-json");
#ifdef EMV_VIEWER_USE_RELATIVE_DATA_PATH
	if (isocodes_path.isEmpty()) {
		isocodes_path =
			app.applicationDirPath() + QStringLiteral("/") +
			QStringLiteral(EMV_VIEWER_USE_RELATIVE_DATA_PATH);
	}
	if (mcc_path.isEmpty()) {
		mcc_path =
			app.applicationDirPath() + QStringLiteral("/") +
			QStringLiteral(EMV_VIEWER_USE_RELATIVE_DATA_PATH) +
			QStringLiteral("mcc_codes.json");
	}
#endif
	r = emv_strings_init(
		isocodes_path.isEmpty() ? nullptr : qPrintable(isocodes_path),
		mcc_path.isEmpty() ? nullptr : qPrintable(mcc_path)
	);
	if (r < 0) {
		qFatal("Failed to initialise EMV strings");
		return 1;
	}
	if (r > 0) {
		qWarning("Failed to load iso-codes data or mcc-codes data; currency, country, language or MCC lookups may not be possible");
	}

	QString ber = parser.value("ber");
	if (ber.simplified() == "-") { // If option value is "-"
		ber = readHexStringFromStdin();
	}
	QString tlv = parser.value("tlv");
	if (tlv.simplified() == "-") { // If option value is "-"
		tlv = readHexStringFromStdin();
	}
	QString overrideData;
	int overrideDecodeCheckBoxState = -1;
	if (!ber.isEmpty()) {
		overrideData = ber;
		overrideDecodeCheckBoxState = Qt::Unchecked;
	}
	if (!tlv.isEmpty()) {
		overrideData = tlv;
		overrideDecodeCheckBoxState = Qt::Checked;
	}

	EmvViewerMainWindow mainwindow(
		nullptr,
		overrideData,
		overrideDecodeCheckBoxState
	);
	mainwindow.show();

	return app.exec();
}
