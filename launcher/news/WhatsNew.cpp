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

#include "WhatsNew.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVariantMap>

#include "BuildConfig.h"
#include "Version.h"
#include "core/LauncherContext.h"
#include "meta/MetaCatalog.h"

namespace
{
	constexpr int kFormatVersion = 1;
	const QString kBundledNotes = QStringLiteral(":/kite/whatsnew.json");
	const QString kLanguageSetting = QStringLiteral("Language");
	const QString kGitHubPrefix = QStringLiteral("https://github.com/");
} // namespace

QList<WhatsNew::Release> WhatsNew::parse(const QByteArray& json, QString* error)
{
	QJsonParseError parseError;
	const QJsonObject root = QJsonDocument::fromJson(json, &parseError).object();
	const auto fail = [error](const QString& why) {
		if (error) {
			*error = why;
		}
		return QList<Release>();
	};
	if (parseError.error != QJsonParseError::NoError) {
		return fail(parseError.errorString());
	}
	if (root.value("formatVersion").toInt() != kFormatVersion) {
		return fail(QStringLiteral("Unsupported notes format."));
	}
	QList<Release> releases;
	for (const QJsonValue& value : root.value("releases").toArray()) {
		const QJsonObject release = value.toObject();
		const QString version = release.value("version").toString().trimmed();
		const QJsonArray items = release.value("items").toArray();
		if (version.isEmpty() || items.isEmpty()) {
			return fail(QStringLiteral("A release needs a version and items."));
		}
		releases.append({version, items});
	}
	return releases;
}

int WhatsNew::notesFor(const QList<Release>& releases, const QString& current)
{
	if (current.isEmpty()) {
		return -1;
	}
	const Version running(current);
	int best = -1;
	for (int i = 0; i < releases.size(); ++i) {
		const Version notes(releases.at(i).version);
		if (notes <= running &&
			(best < 0 || Version(releases.at(best).version) < notes)) {
			best = i;
		}
	}
	return best;
}

bool WhatsNew::isNew(const QString& notesVersion, const QString& seenVersion,
					 bool freshInstall)
{
	if (freshInstall || notesVersion.isEmpty()) {
		return false;
	}
	return seenVersion.isEmpty() || Version(seenVersion) < Version(notesVersion);
}

bool WhatsNew::isFreshInstall(const QString& seenVersion, const QString& language)
{
	return seenVersion.isEmpty() && language.isEmpty();
}

WhatsNew::WhatsNew(QObject* parent) : QObject(parent)
{
	m_current = BuildConfig.VERSION_STR;
	m_settings = LAUNCHER ? LAUNCHER->settings() : nullptr;
	QFile bundled(kBundledNotes);
	setUp(bundled.open(QIODevice::ReadOnly) ? bundled.readAll() : QByteArray());
}

WhatsNew::WhatsNew(const QByteArray& json, const QString& current,
				   SettingsObjectPtr settings, QObject* parent)
	: QObject(parent), m_current(current), m_settings(std::move(settings))
{
	setUp(json);
}

void WhatsNew::setUp(const QByteArray& json)
{
	m_releases = parse(json);
	m_shown = notesFor(m_releases, m_current);
	if (!m_settings) {
		return;
	}
	const QString seen = m_settings->get(kSeenSetting).toString();
	m_freshInstall =
		isFreshInstall(seen, m_settings->get(kLanguageSetting).toString());
	if (m_freshInstall) {
		/* Everything is new to them: the next update is the first to tell. */
		m_settings->set(kSeenSetting, m_current);
		return;
	}
	m_pending = m_shown >= 0 &&
				isNew(m_releases.at(m_shown).version, seen, m_freshInstall);
}

QString WhatsNew::version() const
{
	return m_shown >= 0 ? m_releases.at(m_shown).version : QString();
}

QVariantList WhatsNew::items() const
{
	QVariantList result;
	if (m_shown < 0) {
		return result;
	}
	const QString language =
		m_language.isNull() ? MetaCatalog::selectedLanguage() : m_language;
	for (const QJsonValue& value : m_releases.at(m_shown).items) {
		const QJsonObject item = value.toObject();
		result.append(QVariantMap{
			{"icon", item.value("icon").toString()},
			{"title", MetaCatalog::localized(item.value("title"), language)},
			{"text", MetaCatalog::localized(item.value("text"), language)},
		});
	}
	return result;
}

QString WhatsNew::releaseUrl() const
{
	const QString repo = BuildConfig.KITE_GIT;
	if (m_shown < 0 || !repo.startsWith(kGitHubPrefix)) {
		return QString();
	}
	return repo + QStringLiteral("/releases/tag/v") + version();
}

void WhatsNew::setLanguage(const QString& language)
{
	m_language = language;
	emit changed();
}

void WhatsNew::markSeen()
{
	if (m_settings) {
		m_settings->set(kSeenSetting, m_current);
	}
	if (m_pending) {
		m_pending = false;
		emit changed();
	}
}

void WhatsNew::debugShowNewest()
{
	int newest = -1;
	for (int i = 0; i < m_releases.size(); ++i) {
		if (newest < 0 ||
			Version(m_releases.at(newest).version) < Version(m_releases.at(i).version)) {
			newest = i;
		}
	}
	m_shown = newest;
	emit changed();
}
