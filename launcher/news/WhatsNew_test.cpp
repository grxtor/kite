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

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "news/WhatsNew.h"
#include "settings/INISettingsObject.h"

namespace
{
	const QByteArray kNotes = R"({
		"formatVersion": 1,
		"releases": [
			{ "version": "1.3.0", "items": [
				{ "icon": "server", "title": { "en": "Servers", "tr": "Sunucular" },
				  "text": { "en": "In the top bar.", "tr": "Üst barda." } },
				{ "icon": "users", "title": { "en": "Friends" }, "text": { "en": "A new panel." } } ] },
			{ "version": "1.4.0", "items": [
				{ "icon": "zap", "title": { "en": "Faster" }, "text": { "en": "Everything." } } ] }
		]
	})";

	SettingsObjectPtr settingsIn(const QTemporaryDir& dir, const QString& seen,
								 const QString& language)
	{
		auto settings = std::make_shared<INISettingsObject>(dir.filePath("kite.cfg"));
		settings->registerSetting(WhatsNew::kSeenSetting, QString());
		settings->registerSetting("Language", QString());
		if (!seen.isEmpty()) {
			settings->set(WhatsNew::kSeenSetting, seen);
		}
		if (!language.isEmpty()) {
			settings->set("Language", language);
		}
		return settings;
	}
} // namespace

class WhatsNewTest : public QObject
{
	Q_OBJECT

  private slots:
	void theNotesAreTheNewestAtOrBelowTheVersion()
	{
		const auto releases = WhatsNew::parse(kNotes);
		QCOMPARE(releases.size(), 2);
		QCOMPARE(WhatsNew::notesFor(releases, "1.3.0"), 0);
		// A hotfix shows its release's notes.
		QCOMPARE(WhatsNew::notesFor(releases, "1.3.2"), 0);
		QCOMPARE(WhatsNew::notesFor(releases, "1.4.0"), 1);
		QCOMPARE(WhatsNew::notesFor(releases, "1.10.0"), 1);
		// Older than every note, or no version: none.
		QCOMPARE(WhatsNew::notesFor(releases, "1.2.0"), -1);
		QCOMPARE(WhatsNew::notesFor(releases, QString()), -1);
	}

	void notesAreNewOnlyAfterAnUpdate()
	{
		QVERIFY(WhatsNew::isNew("1.3.0", "1.2.0", false));
		// From before Kite remembered what was seen.
		QVERIFY(WhatsNew::isNew("1.3.0", QString(), false));
		QVERIFY(!WhatsNew::isNew("1.3.0", "1.3.0", false));
		// Seen on 1.3.0, now on the 1.3.1 hotfix: the same notes, nothing new.
		QVERIFY(!WhatsNew::isNew("1.3.0", "1.3.1", false));
		// Back on an older Kite.
		QVERIFY(!WhatsNew::isNew("1.3.0", "1.4.0", false));
		QVERIFY(!WhatsNew::isNew("1.3.0", "1.2.0", true));
		QVERIFY(!WhatsNew::isNew(QString(), "1.2.0", false));
	}

	void aFreshInstallHasNeitherSeenNotesNorALanguage()
	{
		QVERIFY(WhatsNew::isFreshInstall(QString(), QString()));
		QVERIFY(!WhatsNew::isFreshInstall(QString(), "tr_TR"));
		QVERIFY(!WhatsNew::isFreshInstall("1.2.0", QString()));
	}

	void anUpdateShowsTheNotesOnce()
	{
		QTemporaryDir dir;
		{
			WhatsNew notes(kNotes, "1.3.1", settingsIn(dir, "1.2.0", "tr_TR"));
			notes.setLanguage("tr_TR");
			QVERIFY(notes.pending());
			QVERIFY(!notes.freshInstall());
			QCOMPARE(notes.version(), QString("1.3.0"));
			const QVariantList items = notes.items();
			QCOMPARE(items.size(), 2);
			const QVariantMap first = items.first().toMap();
			QCOMPARE(first.value("icon").toString(), QString("server"));
			QCOMPARE(first.value("title").toString(), QString("Sunucular"));
			QCOMPARE(first.value("text").toString(), QString("Üst barda."));
			// No Turkish: English.
			QCOMPARE(items.at(1).toMap().value("title").toString(), QString("Friends"));
			QCOMPARE(notes.releaseUrl(), QString("https://github.com/grxtor/kite/releases/tag/v1.3.0"));

			QSignalSpy changed(&notes, &WhatsNew::changed);
			notes.markSeen();
			QVERIFY(!notes.pending());
			QCOMPARE(changed.count(), 1);
		}
		// The next start of the same version: nothing new.
		WhatsNew again(kNotes, "1.3.1", settingsIn(dir, QString(), QString()));
		QVERIFY(!again.pending());
		// The notes stay reachable from Settings.
		QCOMPARE(again.version(), QString("1.3.0"));
	}

	void anUpdateFromBeforeTheNotesShowsThem()
	{
		QTemporaryDir dir;
		WhatsNew notes(kNotes, "1.3.0", settingsIn(dir, QString(), "en_US"));
		QVERIFY(notes.pending());
		QVERIFY(!notes.freshInstall());
	}

	void aFreshInstallSkipsTheNotesAndRemembersItsVersion()
	{
		QTemporaryDir dir;
		auto settings = settingsIn(dir, QString(), QString());
		WhatsNew notes(kNotes, "1.3.0", settings);
		QVERIFY(notes.freshInstall());
		QVERIFY(!notes.pending());
		QCOMPARE(settings->get(WhatsNew::kSeenSetting).toString(), QString("1.3.0"));
		// The next update, a language picked by then, shows its notes.
		WhatsNew updated(kNotes, "1.4.0", settingsIn(dir, QString(), "en_US"));
		QVERIFY(!updated.freshInstall());
		QVERIFY(updated.pending());
		QCOMPARE(updated.version(), QString("1.4.0"));
	}

	void aVersionWithoutNotesShowsNothing()
	{
		QTemporaryDir dir;
		WhatsNew notes(kNotes, "1.2.0", settingsIn(dir, "1.1.0", "en_US"));
		QVERIFY(!notes.pending());
		QCOMPARE(notes.version(), QString());
		QVERIFY(notes.items().isEmpty());
		QCOMPARE(notes.releaseUrl(), QString());
		// Snapshots ask for the newest notes anyway.
		notes.debugShowNewest();
		QCOMPARE(notes.version(), QString("1.4.0"));
		QVERIFY(!notes.pending());
	}

	void brokenNotesShowNothing()
	{
		QString error;
		QVERIFY(WhatsNew::parse("{", &error).isEmpty());
		QVERIFY(!error.isEmpty());
		QVERIFY(WhatsNew::parse(R"({"formatVersion": 2, "releases": []})").isEmpty());
		QVERIFY(WhatsNew::parse(R"({"formatVersion": 1, "releases": [{"version": "1.3.0"}]})").isEmpty());
		QTemporaryDir dir;
		WhatsNew notes("{", "1.3.0", settingsIn(dir, "1.2.0", "en_US"));
		QVERIFY(!notes.pending());
	}

	void theBundledNotesCoverThisRelease()
	{
		QFile bundled(":/kite/whatsnew.json");
		QVERIFY(bundled.open(QIODevice::ReadOnly));
		QString error;
		const auto releases = WhatsNew::parse(bundled.readAll(), &error);
		QVERIFY2(!releases.isEmpty(), qPrintable(error));
		QTemporaryDir dir;
		QFile again(":/kite/whatsnew.json");
		QVERIFY(again.open(QIODevice::ReadOnly));
		WhatsNew notes(again.readAll(), "1.3.0", settingsIn(dir, "1.2.0", "en_US"));
		QCOMPARE(notes.version(), QString("1.3.0"));
		// Five to seven, each with an icon and both languages.
		const QVariantList items = notes.items();
		QVERIFY(items.size() >= 5 && items.size() <= 7);
		for (const QString& language : {QString("en_US"), QString("tr_TR")}) {
			notes.setLanguage(language);
			QStringList titles;
			for (const QVariant& item : notes.items()) {
				const QVariantMap map = item.toMap();
				QVERIFY(!map.value("icon").toString().isEmpty());
				QVERIFY(!map.value("title").toString().isEmpty());
				QVERIFY(!map.value("text").toString().isEmpty());
				titles.append(map.value("title").toString());
			}
			QCOMPARE(titles.size(), items.size());
		}
		notes.setLanguage("tr_TR");
		const QString turkish = notes.items().first().toMap().value("title").toString();
		notes.setLanguage("en_US");
		QVERIFY(notes.items().first().toMap().value("title").toString() != turkish);
	}
};

QTEST_GUILESS_MAIN(WhatsNewTest)

#include "WhatsNew_test.moc"
