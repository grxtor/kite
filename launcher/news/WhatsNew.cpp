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
#include <QRegularExpression>
#include <QVariantMap>

#include <algorithm>

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
	/// The start of a log holds its version line; no need to read the rest.
	constexpr qint64 kLogHeadBytes = 8 * 1024;
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

QList<int> WhatsNew::notesSince(const QList<Release>& releases, const QString& seen,
								const QString& current)
{
	QList<int> shown;
	if (current.isEmpty()) {
		return shown;
	}
	const Version running(current);
	for (int i = 0; i < releases.size(); ++i) {
		const Version notes(releases.at(i).version);
		if (notes <= running && (seen.isEmpty() || Version(seen) < notes)) {
			shown.append(i);
		}
	}
	std::sort(shown.begin(), shown.end(), [&releases](int a, int b) {
		return Version(releases.at(b).version) < Version(releases.at(a).version);
	});
	return shown;
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

QString WhatsNew::versionInLog(const QByteArray& log)
{
	static const QRegularExpression line(
		QStringLiteral(R"(\bVersion\s+:\s+(\d+\.\d+\.\d+)\b)"));
	const QRegularExpressionMatch match = line.match(QString::fromUtf8(log));
	return match.hasMatch() ? match.captured(1) : QString();
}

WhatsNew::WhatsNew(QObject* parent) : QObject(parent)
{
	m_current = BuildConfig.VERSION_STR;
	m_settings = LAUNCHER ? LAUNCHER->settings() : nullptr;
	QFile bundled(kBundledNotes);
	/* The log rotated at this start: -1 is the previous run's, in the data folder. */
	QFile previous(BuildConfig.KITE_NAME + QStringLiteral("-1.log"));
	setUp(bundled.open(QIODevice::ReadOnly) ? bundled.readAll() : QByteArray(),
		  previous.open(QIODevice::ReadOnly) ? previous.read(kLogHeadBytes) : QByteArray());
}

WhatsNew::WhatsNew(const QByteArray& json, const QString& current,
				   SettingsObjectPtr settings, const QByteArray& previousLog,
				   QObject* parent)
	: QObject(parent), m_current(current), m_settings(std::move(settings))
{
	setUp(json, previousLog);
}

void WhatsNew::setUp(const QByteArray& json, const QByteArray& previousLog)
{
	m_releases = parse(json);
	const int latest = notesFor(m_releases, m_current);
	if (latest >= 0) {
		m_shown = {latest};
	}
	if (!m_settings) {
		return;
	}
	QString seen = m_settings->get(kSeenSetting).toString();
	m_freshInstall =
		isFreshInstall(seen, m_settings->get(kLanguageSetting).toString());
	if (m_freshInstall) {
		/* Everything is new to them: the next update is the first to tell. */
		m_settings->set(kSeenSetting, m_current);
		return;
	}
	if (seen.isEmpty()) {
		/* A Kite from before the notes: the version it ran last, kept at once
		 * so a start that ends before the notes are read still knows it. */
		const QString before = versionInLog(previousLog);
		if (!before.isEmpty() && Version(before) < Version(m_current)) {
			seen = before;
			m_settings->set(kSeenSetting, seen);
		}
	}
	const QList<int> since = notesSince(m_releases, seen, m_current);
	m_pending = !since.isEmpty();
	if (m_pending) {
		m_shown = since;
		m_since = seen;
	}
}

QVariantList WhatsNew::itemsOf(const Release& release) const
{
	const QString language =
		m_language.isNull() ? MetaCatalog::selectedLanguage() : m_language;
	QVariantList result;
	for (const QJsonValue& value : release.items) {
		const QJsonObject item = value.toObject();
		result.append(QVariantMap{
			{"icon", item.value("icon").toString()},
			{"title", MetaCatalog::localized(item.value("title"), language)},
			{"text", MetaCatalog::localized(item.value("text"), language)},
		});
	}
	return result;
}

QString WhatsNew::version() const
{
	return m_shown.isEmpty() ? QString() : m_releases.at(m_shown.first()).version;
}

QString WhatsNew::sinceVersion() const
{
	return m_shown.size() > 1 ? m_since : QString();
}

QVariantList WhatsNew::releases() const
{
	QVariantList result;
	for (int index : m_shown) {
		const Release& release = m_releases.at(index);
		result.append(QVariantMap{{"version", release.version}, {"items", itemsOf(release)}});
	}
	return result;
}

QVariantList WhatsNew::items() const
{
	QVariantList result;
	for (int index : m_shown) {
		result.append(itemsOf(m_releases.at(index)));
	}
	return result;
}

QString WhatsNew::releaseUrl() const
{
	const QString repo = BuildConfig.KITE_GIT;
	if (m_shown.isEmpty() || !repo.startsWith(kGitHubPrefix)) {
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

void WhatsNew::showLatest()
{
	const int latest = notesFor(m_releases, m_current);
	m_shown = latest >= 0 ? QList<int>{latest} : QList<int>();
	emit changed();
}

void WhatsNew::debugShowSince(const QString& seen)
{
	QString newest;
	for (const Release& release : m_releases) {
		if (newest.isEmpty() || Version(newest) < Version(release.version)) {
			newest = release.version;
		}
	}
	m_shown = notesSince(m_releases, seen, newest);
	if (seen.isEmpty() && !m_shown.isEmpty()) {
		m_shown = {m_shown.first()};
	}
	m_since = seen;
	emit changed();
}
