/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include "FeaturedServers.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <algorithm>

#include "Logging.h"
#include "Version.h"
#include "core/LauncherContext.h"
#include "meta/MetaCatalog.h"
#include "servers/ServerPinger.h"

namespace
{
	constexpr int kFormatVersion = 1;
	const QString kEmbeddedList = QStringLiteral(":/kite/servers.json");
	const QString kListPath = QStringLiteral("kite/servers.json");

	QString textOf(const QJsonObject& object, const char* key)
	{
		return object.value(QLatin1String(key)).toString().trimmed();
	}

	QStringList linesOf(const QJsonObject& server)
	{
		QStringList lines;
		for (const QJsonValue& line : server.value("versions").toArray()) {
			lines.append(line.toString().trimmed());
		}
		return lines;
	}

	/// @p version is @p line or a release of it ("1.8.9" of "1.8").
	bool onLine(const QString& version, const QString& line)
	{
		return version == line || version.startsWith(line + QLatin1Char('.'));
	}

	QString problemWith(const QJsonObject& root)
	{
		if (root.value("formatVersion").toInt() != kFormatVersion) {
			return QStringLiteral("Unsupported server list format.");
		}
		if (!root.value("servers").isArray()) {
			return QStringLiteral("The list has no servers.");
		}
		QSet<QString> ids;
		for (const QJsonValue& value : root.value("servers").toArray()) {
			const QJsonObject server = value.toObject();
			const QString id = textOf(server, "id");
			if (id.isEmpty() || textOf(server, "name").isEmpty()) {
				return QStringLiteral("A server needs an id and a name.");
			}
			if (ids.contains(id)) {
				return QStringLiteral("Two servers share the id %1.").arg(id);
			}
			ids.insert(id);
			if (ServerPinger::keyOf(textOf(server, "address")).isEmpty()) {
				return QStringLiteral("The server %1 has no address.").arg(id);
			}
			const QJsonValue versions = server.value("versions");
			if (!versions.isUndefined() &&
				(!versions.isArray() || linesOf(server).contains(QString()))) {
				return QStringLiteral("The versions of %1 are not a list of "
									  "release lines.")
					.arg(id);
			}
		}
		return QString();
	}
} // namespace

FeaturedServers::FeaturedServers(QObject* parent) : QObject(parent)
{
	QFile embedded(kEmbeddedList);
	QString error;
	if (!embedded.open(QIODevice::ReadOnly) ||
		!loadFromJson(embedded.readAll(), &error)) {
		qCWarning(metaLog) << "The built-in server list is unusable:" << error;
	}
}

bool FeaturedServers::loadFromJson(const QByteArray& json, QString* error)
{
	QJsonParseError parseError;
	const auto document = QJsonDocument::fromJson(json, &parseError);
	const QString problem = parseError.error == QJsonParseError::NoError
								? problemWith(document.object())
								: parseError.errorString();
	if (!problem.isEmpty()) {
		if (error) {
			*error = problem;
		}
		return false;
	}
	m_servers = document.object().value("servers").toArray();
	emit changed();
	return true;
}

void FeaturedServers::setLanguage(const QString& language)
{
	m_language = language;
}

QString FeaturedServers::language() const
{
	return m_language.isNull() ? MetaCatalog::selectedLanguage() : m_language;
}

QVariantList FeaturedServers::servers() const
{
	QVariantList result;
	const QString shownIn = language();
	for (const QJsonValue& value : m_servers) {
		const QJsonObject server = value.toObject();
		result.append(QVariantMap{
			{"id", textOf(server, "id")},
			{"name", textOf(server, "name")},
			{"address", textOf(server, "address")},
			{"note", MetaCatalog::localized(server.value("note"), shownIn)},
			{"minVersion", textOf(server, "minVersion")},
			{"maxVersion", textOf(server, "maxVersion")},
			{"versions", linesOf(server)},
		});
	}
	return result;
}

bool FeaturedServers::versionFits(const QString& version,
								  const QString& minVersion,
								  const QString& maxVersion,
								  const QStringList& lines)
{
	if (version.isEmpty()) {
		return false;
	}
	const Version game(version);
	if (!minVersion.isEmpty() && game < Version(minVersion)) {
		return false;
	}
	if (!maxVersion.isEmpty() && Version(maxVersion) < game &&
		!onLine(version, maxVersion)) {
		return false;
	}
	return lines.isEmpty() ||
		   std::any_of(lines.cbegin(), lines.cend(), [&version](const QString& line) {
			   return onLine(version, line);
		   });
}

bool FeaturedServers::loadCached()
{
	const QByteArray json = MetaCatalog::cached(kListPath);
	QString error;
	if (json.isEmpty()) {
		return false;
	}
	if (!loadFromJson(json, &error)) {
		qCWarning(metaLog) << "Ignoring the cached server list:" << error;
		return false;
	}
	return true;
}

void FeaturedServers::refresh()
{
	if (m_refreshed || !LAUNCHER) {
		return;
	}
	m_refreshed = true;
	m_refreshJob = MetaCatalog::refresh(
		kListPath, tr("Featured servers"), this, [this](const QByteArray& json) {
			QString error;
			if (!loadFromJson(json, &error)) {
				qCWarning(metaLog) << "Ignoring the downloaded server list:" << error;
			}
		});
}
