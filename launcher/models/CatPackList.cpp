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

#include "models/CatPackList.h"

#include <QDate>
#include <QDir>
#include <QImageReader>
#include <QUrl>

#include "ui/themes/CatPack.h"

CatPackList::CatPackList(QObject* parent) : QObject(parent)
{
	refresh();
}

CatPackList::~CatPackList() = default;

QString CatPackList::folder() const
{
	return QDir::current().absoluteFilePath(QStringLiteral("catpacks"));
}

QString CatPackList::toUrl(const QString& path)
{
	if (path.startsWith(QLatin1String(":/"))) {
		return QStringLiteral("qrc") + path;
	}
	return QString::fromLatin1(QUrl::fromLocalFile(path).toEncoded());
}

QVariantList CatPackList::list(const QString& folder, const QDate& date)
{
	QStringList filters;
	for (const auto& format : QImageReader::supportedImageFormats()) {
		filters.append(QStringLiteral("*.") + QString::fromLatin1(format));
	}

	QVariantList rows;
	for (const auto& pack : discoverCatPacks(QDir(folder), filters)) {
		rows.append(QVariantMap{
			{QStringLiteral("id"), pack->id()},
			{QStringLiteral("name"), pack->name()},
			{QStringLiteral("source"), toUrl(pack->pathOn(date))}});
	}
	return rows;
}

void CatPackList::refresh()
{
	m_packs = list(folder(), QDate::currentDate());
	emit packsChanged();
}

QString CatPackList::sourceFor(const QString& id) const
{
	for (const QVariant& row : m_packs) {
		const QVariantMap map = row.toMap();
		if (map.value(QStringLiteral("id")).toString() == id) {
			return map.value(QStringLiteral("source")).toString();
		}
	}
	return m_packs.isEmpty()
			   ? QString()
			   : m_packs.first().toMap().value(QStringLiteral("source")).toString();
}
