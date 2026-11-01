/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 Project Tick
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ProfileUtils.h"
#include "minecraft/VersionFilterData.h"
#include "minecraft/OneSixVersionFormat.h"
#include "Json.h"
#include <QDebug>

#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSaveFile>

namespace ProfileUtils
{

	static const int currentOrderFileVersion = 1;

	bool readOverrideOrders(QString path, PatchOrder& order)
	{
		QFile orderFile(path);
		if (!orderFile.exists()) {
			qWarning() << "Order file doesn't exist. Ignoring.";
			return false;
		}
		if (!orderFile.open(QFile::ReadOnly)) {
			qCritical() << "Couldn't open" << orderFile.fileName()
						<< " for reading:" << orderFile.errorString();
			qWarning() << "Ignoring overriden order";
			return false;
		}

		// and it's valid JSON
		QJsonParseError error;
		QJsonDocument doc =
			QJsonDocument::fromJson(orderFile.readAll(), &error);
		if (error.error != QJsonParseError::NoError) {
			qCritical() << "Couldn't parse" << orderFile.fileName() << ":"
						<< error.errorString();
			qWarning() << "Ignoring overriden order";
			return false;
		}

		// and then read it and process it if all above is true.
		try {
			auto obj = Json::requireObject(doc);
			// check order file version.
			auto version = Json::requireInteger(obj.value("version"));
			if (version != currentOrderFileVersion) {
				throw JSONValidationError(
					QObject::tr("Invalid order file version, expected %1")
						.arg(currentOrderFileVersion));
			}
			auto orderArray = Json::requireArray(obj.value("order"));
			for (auto item : orderArray) {
				order.append(Json::requireString(item));
			}
		} catch (const JSONValidationError& err) {
			qCritical() << "Couldn't parse" << orderFile.fileName()
						<< ": bad file format";
			qWarning() << "Ignoring overriden order";
			order.clear();
			return false;
		}
		return true;
	}

	static VersionFilePtr createErrorVersionFile(QString fileId, QString error)
	{
		auto outError = std::make_shared<VersionFile>();
		outError->uid = outError->name = fileId;
		outError->addProblem(ProblemSeverity::Error, error);
		return outError;
	}

	static VersionFilePtr guardedParseJson(const QJsonDocument& doc,
										   const QString& fileId,
										   const QString& filepath,
										   const bool& requireOrder)
	{
		try {
			return OneSixVersionFormat::versionFileFromJson(doc, filepath,
															requireOrder);
		} catch (const Exception& e) {
			return createErrorVersionFile(fileId, e.cause());
		}
	}

	VersionFilePtr parseJsonFile(const QFileInfo& fileInfo,
								 const bool requireOrder)
	{
		QFile file(fileInfo.absoluteFilePath());
		if (!file.open(QFile::ReadOnly)) {
			auto errorStr =
				QObject::tr("Unable to open the version file %1: %2.")
					.arg(fileInfo.fileName(), file.errorString());
			return createErrorVersionFile(fileInfo.completeBaseName(),
										  errorStr);
		}
		QJsonParseError error;
		auto data = file.readAll();
		QJsonDocument doc = QJsonDocument::fromJson(data, &error);
		file.close();
		if (error.error != QJsonParseError::NoError) {
			int line = 1;
			int column = 0;
			for (int i = 0; i < error.offset; i++) {
				if (data[i] == '\n') {
					line++;
					column = 0;
					continue;
				}
				column++;
			}
			auto errorStr = QObject::tr("Unable to process the version file "
										"%1: %2 at line %3 column %4.")
								.arg(fileInfo.fileName(), error.errorString())
								.arg(line)
								.arg(column);
			return createErrorVersionFile(fileInfo.completeBaseName(),
										  errorStr);
		}
		return guardedParseJson(doc, fileInfo.completeBaseName(),
								fileInfo.absoluteFilePath(), requireOrder);
	}

	bool saveJsonFile(const QJsonDocument doc, const QString& filename)
	{
		auto data = doc.toJson();
		QSaveFile jsonFile(filename);
		if (!jsonFile.open(QIODevice::WriteOnly)) {
			jsonFile.cancelWriting();
			qWarning() << "Couldn't open" << filename << "for writing";
			return false;
		}
		jsonFile.write(data);
		if (!jsonFile.commit()) {
			qWarning() << "Couldn't save" << filename;
			return false;
		}
		return true;
	}

	void removeLwjglFromPatch(VersionFilePtr patch)
	{
		auto filter = [](QList<LibraryPtr>& libs) {
			QList<LibraryPtr> filteredLibs;
			for (auto lib : libs) {
				if (!g_VersionFilterData.lwjglWhitelist.contains(
						lib->artifactPrefix())) {
					filteredLibs.append(lib);
				}
			}
			libs = filteredLibs;
		};
		filter(patch->libraries);
	}
} // namespace ProfileUtils
