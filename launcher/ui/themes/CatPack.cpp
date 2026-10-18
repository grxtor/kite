/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include "CatPack.h"
#include "Exception.h"

#include <QDate>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// ===================== BasicCatPack =====================

BasicCatPack::BasicCatPack(const QString& id, const QString& name)
	: m_id(id), m_name(name)
{
}

QString BasicCatPack::id()
{
	return m_id;
}

QString BasicCatPack::name()
{
	return m_name;
}

QString BasicCatPack::path()
{
	return pathOn(QDate::currentDate());
}

QString BasicCatPack::pathOn(const QDate& now)
{
	int month = now.month();
	int day = now.day();

	// Christmas: Dec 21 - Dec 29
	if ((month == 12 && day >= 21 && day <= 29))
		return QString(":/backgrounds/%1-xmas").arg(m_id);

	// Spooky: Oct 27 - Nov 2
	if ((month == 10 && day >= 27) || (month == 11 && day <= 2))
		return QString(":/backgrounds/%1-spooky").arg(m_id);

	// Birthday: Oct 28 - Nov 5
	if ((month == 10 && day >= 28) || (month == 11 && day <= 5))
		return QString(":/backgrounds/%1-bday").arg(m_id);

	return QString(":/backgrounds/%1").arg(m_id);
}

// ===================== FileCatPack =====================

FileCatPack::FileCatPack(const QFileInfo& fileInfo) : m_fileInfo(fileInfo) {}

QString FileCatPack::id()
{
	return m_fileInfo.baseName();
}

QString FileCatPack::name()
{
	return m_fileInfo.baseName();
}

QString FileCatPack::path()
{
	return m_fileInfo.absoluteFilePath();
}

// ===================== JsonCatPack =====================

JsonCatPack::JsonCatPack(const QFileInfo& manifestInfo)
{
	QFile file(manifestInfo.absoluteFilePath());
	if (!file.open(QFile::ReadOnly))
		throw Exception(QString("Could not open catpack manifest: %1")
							.arg(manifestInfo.absoluteFilePath()));

	QJsonParseError parseError;
	auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError)
		throw Exception(QString("catpack.json parse error: %1")
							.arg(parseError.errorString()));

	auto root = doc.object();
	m_id = manifestInfo.dir().dirName();
	m_name = root.value("name").toString(m_id);
	m_defaultPath = QDir(manifestInfo.absolutePath())
						.absoluteFilePath(root.value("default").toString());

	auto variants = root.value("variants").toArray();
	for (const auto& val : variants) {
		auto obj = val.toObject();
		JsonCatPackVariant v;
		v.path = QDir(manifestInfo.absolutePath())
					 .absoluteFilePath(obj.value("path").toString());

		auto startObj = obj.value("startTime").toObject();
		v.startMonth = startObj.value("month").toInt();
		v.startDay = startObj.value("day").toInt();

		auto endObj = obj.value("endTime").toObject();
		v.endMonth = endObj.value("month").toInt();
		v.endDay = endObj.value("day").toInt();

		m_variants.append(v);
	}
}

QString JsonCatPack::id()
{
	return m_id;
}

QString JsonCatPack::name()
{
	return m_name;
}

QString JsonCatPack::path()
{
	return pathOn(QDate::currentDate());
}

QString JsonCatPack::pathOn(const QDate& now)
{
	for (const auto& v : m_variants) {
		bool inRange = false;
		if (v.startMonth <= v.endMonth) {
			// Same year range: e.g., Mar 1 - Jun 30
			QDate start(now.year(), v.startMonth, v.startDay);
			QDate end(now.year(), v.endMonth, v.endDay);
			inRange = (now >= start && now <= end);
		} else {
			// Wraps around year boundary: e.g., Dec 20 - Jan 5
			QDate startThisYear(now.year(), v.startMonth, v.startDay);
			QDate endNextYear(now.year() + 1, v.endMonth, v.endDay);
			QDate startLastYear(now.year() - 1, v.startMonth, v.startDay);
			QDate endThisYear(now.year(), v.endMonth, v.endDay);

			inRange = (now >= startThisYear && now <= endNextYear) ||
					  (now >= startLastYear && now <= endThisYear);
		}

		if (inRange)
			return v.path;
	}

	return m_defaultPath;
}

// ===================== discovery =====================

std::vector<std::unique_ptr<CatPack>>
discoverCatPacks(const QDir& folder, const QStringList& imageFilters)
{
	std::vector<std::unique_ptr<CatPack>> packs;
	QStringList seen;
	auto add = [&](std::unique_ptr<CatPack> pack) {
		const QString id = pack->id();
		if (seen.contains(id)) {
			qWarning() << "CatPack" << id << "not added to prevent id duplication";
			return;
		}
		seen << id;
		packs.push_back(std::move(pack));
	};

	const QList<std::pair<QString, QString>> defaultCats{
		{"kitteh", QObject::tr("Background Cat (from MultiMC)")},
		{"rory", QObject::tr("Rory ID 11 (drawn by Ashtaka)")},
		{"rory-flat",
		 QObject::tr("Rory ID 11 (flat edition, drawn by Ashtaka)")},
		{"teawie", QObject::tr("Teawie (drawn by SympathyTea)")}};
	for (const auto& [id, name] : defaultCats) {
		add(std::make_unique<BasicCatPack>(id, name));
	}

	auto loadFiles = [&](const QDir& dir) {
		QDirIterator it(dir.absolutePath(), imageFilters, QDir::Files);
		while (it.hasNext()) {
			add(std::make_unique<FileCatPack>(QFileInfo(it.next())));
		}
	};

	// Pictures in the folder's root.
	loadFiles(folder);

	// Sub-folders: a catpack.json, or just pictures.
	QDirIterator directories(folder.path(), QDir::Dirs | QDir::NoDotAndDotDot);
	while (directories.hasNext()) {
		QDir dir(directories.next());
		QFileInfo manifest(dir.absoluteFilePath("catpack.json"));
		if (manifest.isFile()) {
			try {
				add(std::make_unique<JsonCatPack>(manifest));
			} catch (const Exception& e) {
				qWarning() << "Couldn't load catpack json:" << e.cause();
			}
		} else {
			loadFiles(dir);
		}
	}
	return packs;
}
