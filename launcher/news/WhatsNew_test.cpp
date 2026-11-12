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
			{ "version": "1.3.1", "items": [
				{ "icon": "sliders", "title": { "en": "Your bar", "tr": "Senin barın" },
				  "text": { "en": "Drag it.", "tr": "Sürükle." } } ] },
			{ "version": "1.3.0", "items": [
				{ "icon": "server", "title": { "en": "Servers", "tr": "Sunucular" },
				  "text": { "en": "In the top bar.", "tr": "Üst barda." } },
				{ "icon": "users", "title": { "en": "Friends" }, "text": { "en": "A new panel." } } ] },
			{ "version": "1.4.0", "items": [
				{ "icon": "zap", "title": { "en": "Faster" }, "text": { "en": "Everything." } } ] }
		]
	})";

	/* The head of a launcher log, as Application writes it. */
	QByteArray logOf(const QString& version)
	{
		return QStringLiteral("     0.000 D: <> Log initialized. (Application::initLogging:822)\n"
							  "     0.000 I: Kite , (c) 2026 grxtor (Application::setupPaths:843)\n"
							  "     0.000 I: Version                    :  %1 (Application::setupPaths:845)\n")
			.arg(version)
			.toUtf8();
	}

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

	QStringList versionsOf(const QVariantList& releases)
	{
		QStringList versions;
		for (const QVariant& release : releases) {
			versions.append(release.toMap().value("version").toString());
		}
		return versions;
	}
} // namespace

class WhatsNewTest : public QObject
{
	Q_OBJECT

  private slots:
	void theLatestNotesAreTheNewestAtOrBelowTheVersion()
	{
		const auto releases = WhatsNew::parse(kNotes);
		QCOMPARE(releases.size(), 3);
		QCOMPARE(WhatsNew::notesFor(releases, "1.3.0"), 1);
		QCOMPARE(WhatsNew::notesFor(releases, "1.3.2"), 0);
		QCOMPARE(WhatsNew::notesFor(releases, "1.10.0"), 2);
		QCOMPARE(WhatsNew::notesFor(releases, "1.2.0"), -1);
		QCOMPARE(WhatsNew::notesFor(releases, QString()), -1);
	}

	void theNotesSinceAVersionAreNewestFirstUpToThisOne()
	{
		const auto releases = WhatsNew::parse(kNotes);
		const auto versions = [&releases](const QList<int>& indexes) {
			QStringList out;
			for (int index : indexes) {
				out.append(releases.at(index).version);
			}
			return out;
		};
		QCOMPARE(versions(WhatsNew::notesSince(releases, "1.3.0", "1.3.1")), QStringList({"1.3.1"}));
		QCOMPARE(versions(WhatsNew::notesSince(releases, "1.2.0", "1.3.1")), QStringList({"1.3.1", "1.3.0"}));
		// Never past the running version.
		QCOMPARE(versions(WhatsNew::notesSince(releases, "1.2.0", "1.3.0")), QStringList({"1.3.0"}));
		QCOMPARE(versions(WhatsNew::notesSince(releases, QString(), "1.3.2")), QStringList({"1.3.1", "1.3.0"}));
		QVERIFY(WhatsNew::notesSince(releases, "1.3.1", "1.3.1").isEmpty());
		QVERIFY(WhatsNew::notesSince(releases, "1.3.0", QString()).isEmpty());
	}

	void notesAreNewOnlyAfterAnUpdate()
	{
		QVERIFY(WhatsNew::isNew("1.3.0", "1.2.0", false));
		QVERIFY(WhatsNew::isNew("1.3.0", QString(), false));
		QVERIFY(!WhatsNew::isNew("1.3.0", "1.3.0", false));
		QVERIFY(!WhatsNew::isNew("1.3.0", "1.3.1", false));
		QVERIFY(!WhatsNew::isNew("1.3.0", "1.2.0", true));
		QVERIFY(!WhatsNew::isNew(QString(), "1.2.0", false));
	}

	void aFreshInstallHasNeitherSeenNotesNorALanguage()
	{
		QVERIFY(WhatsNew::isFreshInstall(QString(), QString()));
		QVERIFY(!WhatsNew::isFreshInstall(QString(), "tr_TR"));
		QVERIFY(!WhatsNew::isFreshInstall("1.2.0", QString()));
	}

	void theLogSaysWhichVersionRanBefore()
	{
		QCOMPARE(WhatsNew::versionInLog(logOf("1.2.0")), QString("1.2.0"));
		QCOMPARE(WhatsNew::versionInLog(logOf("1.3.0")), QString("1.3.0"));
		QCOMPARE(WhatsNew::versionInLog("no version here"), QString());
		QCOMPARE(WhatsNew::versionInLog(QByteArray()), QString());
	}

	void fromTheLastReleaseOnlyItsNotesShow()
	{
		QTemporaryDir dir;
		WhatsNew notes(kNotes, "1.3.1", settingsIn(dir, "1.3.0", "tr_TR"));
		notes.setLanguage("tr_TR");
		QVERIFY(notes.pending());
		QCOMPARE(versionsOf(notes.releases()), QStringList({"1.3.1"}));
		QCOMPARE(notes.version(), QString("1.3.1"));
		// One release: no "since" in the title.
		QCOMPARE(notes.sinceVersion(), QString());
		QCOMPARE(notes.items().first().toMap().value("title").toString(), QString("Senin barın"));
		QCOMPARE(notes.releaseUrl(), QString("https://github.com/grxtor/kite/releases/tag/v1.3.1"));
	}

	void fromFurtherBackEveryReleaseSinceShows()
	{
		QTemporaryDir dir;
		{
			WhatsNew notes(kNotes, "1.3.1", settingsIn(dir, "1.2.0", "en_US"));
			notes.setLanguage("en_US");
			QVERIFY(notes.pending());
			QCOMPARE(versionsOf(notes.releases()), QStringList({"1.3.1", "1.3.0"}));
			QCOMPARE(notes.sinceVersion(), QString("1.2.0"));
			QCOMPARE(notes.version(), QString("1.3.1"));
			// Newest first, then the file's order.
			QStringList titles;
			for (const QVariant& item : notes.items()) {
				titles.append(item.toMap().value("title").toString());
			}
			QCOMPARE(titles, QStringList({"Your bar", "Servers", "Friends"}));

			QSignalSpy changed(&notes, &WhatsNew::changed);
			notes.markSeen();
			QVERIFY(!notes.pending());
			QCOMPARE(changed.count(), 1);
		}
		// The next start of the same version: nothing new, and About shows this version's alone.
		WhatsNew again(kNotes, "1.3.1", settingsIn(dir, QString(), QString()));
		QVERIFY(!again.pending());
		QCOMPARE(versionsOf(again.releases()), QStringList({"1.3.1"}));
		QCOMPARE(again.sinceVersion(), QString());
	}

	void aKiteFromBeforeTheNotesIsReadOffItsLog()
	{
		// 1.3.0 had no notes setting; its log says what ran.
		QTemporaryDir dir;
		auto settings = settingsIn(dir, QString(), "tr_TR");
		WhatsNew notes(kNotes, "1.3.1", settings, logOf("1.3.0"));
		QVERIFY(notes.pending());
		QCOMPARE(versionsOf(notes.releases()), QStringList({"1.3.1"}));
		// Kept at once, so a start that ends before the dialog is read still knows.
		QCOMPARE(settings->get(WhatsNew::kSeenSetting).toString(), QString("1.3.0"));

		QTemporaryDir older;
		WhatsNew fromOld(kNotes, "1.3.1", settingsIn(older, QString(), "tr_TR"), logOf("1.2.0"));
		QCOMPARE(versionsOf(fromOld.releases()), QStringList({"1.3.1", "1.3.0"}));
		QCOMPARE(fromOld.sinceVersion(), QString("1.2.0"));
	}

	void withoutALogEveryReleaseShowsWithoutASince()
	{
		QTemporaryDir dir;
		WhatsNew notes(kNotes, "1.3.1", settingsIn(dir, QString(), "en_US"), "garbage");
		QVERIFY(notes.pending());
		QCOMPARE(versionsOf(notes.releases()), QStringList({"1.3.1", "1.3.0"}));
		QCOMPARE(notes.sinceVersion(), QString());
		// A log of this same version (the update ran once already) tells nothing either.
		QTemporaryDir same;
		auto settings = settingsIn(same, QString(), "en_US");
		WhatsNew again(kNotes, "1.3.1", settings, logOf("1.3.1"));
		QVERIFY(again.pending());
		QCOMPARE(settings->get(WhatsNew::kSeenSetting).toString(), QString());
	}

	void aFreshInstallSkipsTheNotesAndRemembersItsVersion()
	{
		QTemporaryDir dir;
		auto settings = settingsIn(dir, QString(), QString());
		WhatsNew notes(kNotes, "1.3.0", settings, logOf("1.2.0"));
		QVERIFY(notes.freshInstall());
		QVERIFY(!notes.pending());
		QCOMPARE(settings->get(WhatsNew::kSeenSetting).toString(), QString("1.3.0"));
		WhatsNew updated(kNotes, "1.4.0", settingsIn(dir, QString(), "en_US"));
		QVERIFY(updated.pending());
		QCOMPARE(versionsOf(updated.releases()), QStringList({"1.4.0", "1.3.1"}));
		QCOMPARE(updated.sinceVersion(), QString("1.3.0"));
	}

	void aboutShowsThisVersionsNotes()
	{
		QTemporaryDir dir;
		WhatsNew notes(kNotes, "1.3.1", settingsIn(dir, "1.2.0", "en_US"));
		QCOMPARE(notes.releases().size(), 2);
		notes.showLatest();
		QCOMPARE(versionsOf(notes.releases()), QStringList({"1.3.1"}));
		QCOMPARE(notes.sinceVersion(), QString());
	}

	void aVersionWithoutNotesShowsNothing()
	{
		QTemporaryDir dir;
		WhatsNew notes(kNotes, "1.2.0", settingsIn(dir, "1.1.0", "en_US"));
		QVERIFY(!notes.pending());
		QCOMPARE(notes.version(), QString());
		QVERIFY(notes.items().isEmpty());
		QCOMPARE(notes.releaseUrl(), QString());
		// Snapshots ask for notes anyway.
		notes.debugShowSince("1.2.0");
		QCOMPARE(versionsOf(notes.releases()), QStringList({"1.4.0", "1.3.1", "1.3.0"}));
		QCOMPARE(notes.sinceVersion(), QString("1.2.0"));
		notes.debugShowSince(QString());
		QCOMPARE(versionsOf(notes.releases()), QStringList({"1.4.0"}));
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

	void theBundledNotesCoverBothReleases()
	{
		QFile bundled(":/kite/whatsnew.json");
		QVERIFY(bundled.open(QIODevice::ReadOnly));
		const QByteArray json = bundled.readAll();
		QString error;
		QVERIFY2(!WhatsNew::parse(json, &error).isEmpty(), qPrintable(error));
		QTemporaryDir dir;
		WhatsNew notes(json, "1.3.1", settingsIn(dir, "1.2.0", "en_US"));
		QCOMPARE(versionsOf(notes.releases()), QStringList({"1.3.1", "1.3.0"}));
		// The bar's editing and the mod reminder are 1.3.1's; 1.3.0's own list stays short.
		const QVariantList releases = notes.releases();
		QCOMPARE(releases.at(0).toMap().value("items").toList().size(), 2);
		const int older = int(releases.at(1).toMap().value("items").toList().size());
		QVERIFY(older >= 5 && older <= 7);
		for (const QString& language : {QString("en_US"), QString("tr_TR")}) {
			notes.setLanguage(language);
			for (const QVariant& item : notes.items()) {
				const QVariantMap map = item.toMap();
				QVERIFY(!map.value("icon").toString().isEmpty());
				QVERIFY(!map.value("title").toString().isEmpty());
				QVERIFY(!map.value("text").toString().isEmpty());
			}
		}
		notes.setLanguage("tr_TR");
		const QString turkish = notes.items().first().toMap().value("title").toString();
		notes.setLanguage("en_US");
		QVERIFY(notes.items().first().toMap().value("title").toString() != turkish);
	}
};

QTEST_GUILESS_MAIN(WhatsNewTest)

#include "WhatsNew_test.moc"
