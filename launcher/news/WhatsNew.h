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

#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>

#include "settings/SettingsObject.h"

/* What is new since the version a player last ran, shown once after an update
 * (WhatsNewDialog.qml); Settings → About shows this version's notes again. The
 * notes are bundled (:/kite/whatsnew.json), one entry per release that has
 * them:
 *
 *   {"formatVersion": 1, "releases": [{"version": "1.3.1", "items": [
 *     {"icon": "server", "title": {"en": ..., "tr": ...},
 *      "text": {"en": ..., "tr": ...}}]}]}
 *
 * `icon` names a Lucide icon of the QML components. After an update the notes
 * shown are every release's after the one last seen, up to the running
 * version, newest first: someone coming from 1.3.0 to 1.3.1 sees 1.3.1's, from
 * 1.2.0 both. The version last seen is kept in kSeenSetting; a Kite from before
 * it existed is read off the previous run's log ("Version : 1.2.0"). A
 * brand-new install shows none: it remembers its version at once. */
class WhatsNew : public QObject
{
	Q_OBJECT

	/// The newest release shown ("1.3.1"); "" when there are no notes.
	Q_PROPERTY(QString version READ version NOTIFY changed)
	/// The version the notes shown come after ("1.2.0") while they are more
	/// than one release's; "" otherwise, or when it is not known.
	Q_PROPERTY(QString sinceVersion READ sinceVersion NOTIFY changed)
	/// The releases shown, newest first: {version, items}, each item
	/// {icon, title, text} in the launcher's language.
	Q_PROPERTY(QVariantList releases READ releases NOTIFY changed)
	/// Every item shown, newest release first.
	Q_PROPERTY(QVariantList items READ items NOTIFY changed)
	/// The notes are new to this person: Kite was updated since they last saw
	/// any. Cleared by markSeen().
	Q_PROPERTY(bool pending READ pending NOTIFY changed)
	/// The newest release's page on GitHub; "" without a source address.
	Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY changed)

  public:
	/// The version whose notes were last seen ("" before the first start).
	static constexpr const char* kSeenSetting = "WhatsNewSeenVersion";

	struct Release
	{
		QString version;
		QJsonArray items;
	};

	/// The releases of a format-1 notes file, in its order; empty, with
	/// @p error set, when it is not one.
	static QList<Release> parse(const QByteArray& json, QString* error = nullptr);
	/// Index into @p releases of the notes for Kite @p current: the newest at
	/// or below it; -1 when there are none.
	static int notesFor(const QList<Release>& releases, const QString& current);
	/// Indexes into @p releases of the notes after @p seen ("" for all) up to
	/// Kite @p current, newest first.
	static QList<int> notesSince(const QList<Release>& releases, const QString& seen,
								 const QString& current);
	/// Whether notes for @p notesVersion are new to someone who last saw the
	/// notes of @p seenVersion ("" for never). Never on a fresh install.
	static bool isNew(const QString& notesVersion, const QString& seenVersion,
					  bool freshInstall);
	/// No notes were ever seen and no language was ever picked: the first
	/// start of a new install, before onboarding.
	static bool isFreshInstall(const QString& seenVersion,
							   const QString& language);
	/// The version a launcher log says it ran ("Version : 1.2.0"), "" if none.
	static QString versionInLog(const QByteArray& log);

	/// The bundled notes, the running version, the launcher's settings and
	/// the previous run's log.
	explicit WhatsNew(QObject* parent = nullptr);
	/// For tests: @p json as the notes and Kite @p current over @p settings,
	/// which needs kSeenSetting and "Language" registered, with
	/// @p previousLog as the previous run's log.
	WhatsNew(const QByteArray& json, const QString& current,
			 SettingsObjectPtr settings, const QByteArray& previousLog = QByteArray(),
			 QObject* parent = nullptr);

	QString version() const;
	QString sinceVersion() const;
	QVariantList releases() const;
	QVariantList items() const;
	bool pending() const
	{
		return m_pending;
	}
	QString releaseUrl() const;
	/// This start was the first of a new install (see isFreshInstall()).
	bool freshInstall() const
	{
		return m_freshInstall;
	}

	/// Overrides the selected language, for tests.
	void setLanguage(const QString& language);

	/// The notes were shown: remembers the running version.
	Q_INVOKABLE void markSeen();
	/// Settings → About: this version's notes only.
	Q_INVOKABLE void showLatest();
	/// Dev only (KITE_QML_ROUTE "whatsnew[=<version>]"): every release's notes
	/// after @p seen ("" for the newest alone), whatever the running version,
	/// for review snapshots.
	Q_INVOKABLE void debugShowSince(const QString& seen);

  signals:
	void changed();

  private:
	void setUp(const QByteArray& json, const QByteArray& previousLog);
	QVariantList itemsOf(const Release& release) const;

	QList<Release> m_releases;
	/// What the dialog shows, newest first.
	QList<int> m_shown;
	/// The version last seen when this start began, or "" if not known.
	QString m_since;
	QString m_current;
	QString m_language;
	SettingsObjectPtr m_settings;
	bool m_pending = false;
	bool m_freshInstall = false;
};
