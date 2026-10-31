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

#include <QDateTime>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "minecraft/PerformanceSetupRules.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/BundleCatalog.h"

namespace
{
	void touch(const QDir& dir, const QString& name,
			   const QByteArray& content = "x")
	{
		QFile file(dir.filePath(name));
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write(content);
	}
} // namespace

class PerformanceSetupTest : public QObject
{
	Q_OBJECT

  private slots:
	void wantsTheDefaultsInOrder()
	{
		QCOMPARE(PerformanceSetup::wantedProjects({"a", "b", "c"}, {}, {}),
				 (QStringList{"a", "b", "c"}));
	}

	void leavesOutWhatThePlayerUnticked()
	{
		QCOMPARE(PerformanceSetup::wantedProjects({"a", "b", "c"}, {"b"}, {}),
				 (QStringList{"a", "c"}));
	}

	void leavesOutWhatIsInstalled()
	{
		QCOMPARE(PerformanceSetup::wantedProjects({"a", "b", "c"}, {}, {"a", "c"}),
				 (QStringList{"b"}));
	}

	void wantsNothingWhenAllIsThere()
	{
		QVERIFY(PerformanceSetup::wantedProjects({"a", "b"}, {"a"}, {"b"})
					.isEmpty());
		QVERIFY(PerformanceSetup::wantedProjects({}, {}, {}).isEmpty());
	}

	void listsAProjectOnce()
	{
		QCOMPARE(PerformanceSetup::wantedProjects({"a", "a", ""}, {}, {}),
				 (QStringList{"a"}));
	}

	void aVanillaInstanceHasNoLoader()
	{
		QVERIFY(!PerformanceSetup::hasLoader(QString()));
	}

	void aModdedInstanceHasALoader()
	{
		for (const QString& loader : {"fabric", "forge", "quilt", "neoforge"}) {
			QVERIFY(PerformanceSetup::hasLoader(loader));
		}
	}

	void releasesFrom116OnAreSupported()
	{
		for (const QString& version :
			 {"1.16", "1.16.5", "1.20.1", "1.21.11", "26.3", "26.1.2"}) {
			QVERIFY2(PerformanceSetup::supportsVersion(version),
					 qPrintable(version));
		}
	}

	void olderVersionsAndSnapshotsAreNot()
	{
		for (const QString& version :
			 {"1.12.2", "1.8.9", "1.15.2", "1.7", "24w14a", "b1.7.3", "a1.0.4",
			  "1.21-pre1", "1.", "", "latest"}) {
			QVERIFY2(!PerformanceSetup::supportsVersion(version),
					 qPrintable(version));
		}
	}

	void theBuiltInCatalogHasDefaultsForFabric()
	{
		BundleCatalog catalog;
		QStringList defaults;
		for (const QVariant& mod : catalog.mods({"performance"}, "fabric")) {
			if (mod.toMap().value("isDefault").toBool()) {
				defaults.append(mod.toMap().value("project").toString());
			}
		}
		QVERIFY(!defaults.isEmpty());
		/* Sodium, by its Modrinth project id. */
		QVERIFY(defaults.contains("AANobbMI"));
	}

	void jarsNotFromModrinthAreTheOnesToIdentify()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QDir mods(dir.path());
		touch(mods, "recorded.jar");
		touch(mods, "recorded-off.jar.disabled");
		touch(mods, "by-hand.jar");
		touch(mods, "by-hand-off.jar.disabled");
		touch(mods, "from-curseforge.jar");
		touch(mods, "recorded-local.jar");
		touch(mods, "no-project.jar");
		touch(mods, "notes.txt");

		ModMetadataIndex index(mods);
		const auto record = [&index](const QString& name, const QString& platform,
									 const QString& projectId) {
			ModMetadataIndex::Entry entry;
			entry.fileName = name;
			entry.platform = platform;
			entry.projectId = projectId;
			QVERIFY(index.put(entry));
		};
		record("recorded.jar", "modrinth", "AANobbMI");
		record("recorded-off.jar", "modrinth", "AANobbMI");
		record("from-curseforge.jar", "curseforge", "394468");
		record("recorded-local.jar", "local", "");
		record("no-project.jar", "modrinth", "");

		/* A copy from CurseForge, or recorded as local, is no Modrinth
		 * project the setup knows of: only the hash can tell. */
		QCOMPARE(PerformanceSetup::jarsNotFromModrinth(mods, index),
				 (QStringList{"by-hand-off.jar.disabled", "by-hand.jar",
							  "from-curseforge.jar", "no-project.jar",
							  "recorded-local.jar"}));
	}

	void hashesAreSha1()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		touch(QDir(dir.path()), "a.jar", "abc");
		/* The well-known SHA-1 of "abc". */
		QCOMPARE(PerformanceSetup::sha1sOf(QDir(dir.path()), {"a.jar", "missing.jar"}),
				 (QStringList{"a9993e364706816aba3e25717850c26c9cd0d89d"}));
	}

	void theLookupBodyNamesTheAlgorithm()
	{
		const QJsonObject body = QJsonDocument::fromJson(
									 PerformanceSetup::hashLookupBody({"h1", "h2"}))
									 .object();
		QCOMPARE(body.value("algorithm").toString(), QString("sha1"));
		QCOMPARE(body.value("hashes").toVariant().toStringList(),
				 (QStringList{"h1", "h2"}));
	}

	void theReplyNamesProjects()
	{
		const QByteArray reply = R"({
			"h1": { "id": "v1", "project_id": "AANobbMI" },
			"h2": { "id": "v2", "project_id": "gvQqBUqZ" },
			"h3": { "id": "v3", "project_id": "AANobbMI" }
		})";
		QCOMPARE(PerformanceSetup::projectsInHashReply(reply).size(), 2);
		QVERIFY(PerformanceSetup::projectsInHashReply(reply).contains("AANobbMI"));
		QVERIFY(PerformanceSetup::projectsInHashReply(reply).contains("gvQqBUqZ"));
	}

	void aBadReplyNamesNothing()
	{
		QVERIFY(PerformanceSetup::projectsInHashReply("").isEmpty());
		QVERIFY(PerformanceSetup::projectsInHashReply("not json").isEmpty());
		QVERIFY(PerformanceSetup::projectsInHashReply("[1,2]").isEmpty());
		QVERIFY(PerformanceSetup::projectsInHashReply("{}").isEmpty());
	}

	void defaultsAreTheCatalogModsMarkedDefault()
	{
		const QVariantList mods = {
			QVariantMap{{"project", "a"}, {"isDefault", true}},
			QVariantMap{{"project", "b"}, {"isDefault", false}},
			QVariantMap{{"project", "c"}},
			QVariantMap{{"project", "d"}, {"isDefault", true}},
		};
		QCOMPARE(PerformanceSetup::defaultProjects(mods), (QStringList{"a", "d"}));
		QVERIFY(PerformanceSetup::defaultProjects({}).isEmpty());
	}

	void theMatchKeyNamesTheVersionAndTheLoader()
	{
		QCOMPARE(PerformanceSetup::matchKey("1.21.5", "fabric"),
				 QString("1.21.5 fabric"));
		QVERIFY(PerformanceSetup::matchKey("1.21.5", "fabric") !=
				PerformanceSetup::matchKey("1.21.4", "fabric"));
		QVERIFY(PerformanceSetup::matchKey("1.21.5", "fabric") !=
				PerformanceSetup::matchKey("1.21.5", "neoforge"));
	}

	void rematchesTheCatalogModsAndTheirDependencies()
	{
		const auto entry = [](const QString& file, const QString& platform,
							  const QString& project, bool dependency = false) {
			ModMetadataIndex::Entry e;
			e.fileName = file;
			e.platform = platform;
			e.projectId = project;
			e.isDependency = dependency;
			return e;
		};
		const QList<ModMetadataIndex::Entry> entries = {
			entry("sodium.jar", "modrinth", "AANobbMI"),
			entry("fabric-api.jar", "modrinth", "P7dR8mSH", true),
			entry("hers.jar", "modrinth", "other"),
			entry("sodium-cf.jar", "curseforge", "AANobbMI"),
			entry("lib-cf.jar", "curseforge", "123", true),
			entry("by-hand.jar", "", ""),
			entry("by-hand-dep.jar", "modrinth", "", true),
		};

		QHash<QString, QString> expected;
		expected.insert("AANobbMI", "sodium.jar");
		expected.insert("P7dR8mSH", "fabric-api.jar");
		QCOMPARE(PerformanceSetup::filesToRematch(entries, {"AANobbMI", "gvQqBUqZ"}),
				 expected);
		QVERIFY(PerformanceSetup::filesToRematch({}, {"AANobbMI"}).isEmpty());
		/* Without a catalog mod, only the dependency of one remains. */
		QCOMPARE(PerformanceSetup::filesToRematch(entries, {}).size(), 1);
	}

	void aRemovedDefaultIsExcluded()
	{
		QCOMPARE(PerformanceSetup::withRemoved({"x"}, {"a", "b"}, {"a", "c"}),
				 (QStringList{"x", "a"}));
	}

	void aRemovedModOutsideTheDefaultsIsNot()
	{
		QCOMPARE(PerformanceSetup::withRemoved({"x"}, {"mine", ""}, {"a", "c"}),
				 (QStringList{"x"}));
		QVERIFY(PerformanceSetup::withRemoved({}, {}, {"a"}).isEmpty());
	}

	void anExcludedProjectIsListedOnce()
	{
		QCOMPARE(PerformanceSetup::withRemoved({"a", "x"}, {"a", "c", "c"},
											   {"a", "c"}),
				 (QStringList{"a", "x", "c"}));
	}

	void anIncludedProjectLeavesWhenRemoved()
	{
		QCOMPARE(PerformanceSetup::withoutRemoved({"a", "b", "c"}, {"b", "x"}),
				 (QStringList{"a", "c"}));
		QCOMPARE(PerformanceSetup::withoutRemoved({"a"}, {}), (QStringList{"a"}));
		QVERIFY(PerformanceSetup::withoutRemoved({}, {"a"}).isEmpty());
	}

	void theProjectsInBothAreKeptInOrder()
	{
		QCOMPARE(PerformanceSetup::commonProjects({"c", "a", "b"}, {"b", "c", "z"}),
				 (QStringList{"c", "b"}));
		QVERIFY(PerformanceSetup::commonProjects({"a"}, {}).isEmpty());
		QVERIFY(PerformanceSetup::commonProjects({}, {"a"}).isEmpty());
	}

	void installedNamesAreTheOnesTheFolderHas()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QDir mods(dir.path());
		touch(mods, "sodium.jar");
		touch(mods, "unnamed.jar");
		touch(mods, "from-curseforge.jar");

		ModMetadataIndex index(mods);
		const auto record = [&index](const QString& file, const QString& platform,
									 const QString& project, const QString& name) {
			ModMetadataIndex::Entry entry;
			entry.fileName = file;
			entry.platform = platform;
			entry.projectId = project;
			entry.name = name;
			QVERIFY(index.put(entry));
		};
		record("sodium.jar", "modrinth", "AANobbMI", "Sodium");
		record("unnamed.jar", "modrinth", "gvQqBUqZ", "");
		record("from-curseforge.jar", "curseforge", "394468", "Lithium");

		/* The name it was installed under, else the file; a project the
		 * folder does not have, or has from CurseForge, is left out. */
		QCOMPARE(PerformanceSetup::installedNames(
					 index, {"gvQqBUqZ", "gone", "394468", "AANobbMI"}),
				 (QStringList{"unnamed.jar", "Sodium"}));
		QVERIFY(PerformanceSetup::installedNames(index, {}).isEmpty());
	}

	void aMissingRecordIsKeptForADay()
	{
		const QDateTime then = QDateTime::fromSecsSinceEpoch(1'700'000'000, Qt::UTC);
		const QString record =
			PerformanceSetup::missingRecord("1.19 fabric", then, {"a", "b"});

		const QStringList projects = {"a", "b"};
		QCOMPARE(PerformanceSetup::recentlyMissing(record, "1.19 fabric", then),
				 projects);
		QCOMPARE(PerformanceSetup::recentlyMissing(record, "1.19 fabric",
												   then.addSecs(23 * 60 * 60)),
				 projects);
		QVERIFY(PerformanceSetup::recentlyMissing(record, "1.19 fabric",
												  then.addDays(1))
					.isEmpty());
		QVERIFY(PerformanceSetup::recentlyMissing(record, "1.19 fabric",
												  then.addDays(30))
					.isEmpty());
	}

	void aMissingRecordIsForOneVersionAndLoader()
	{
		const QDateTime then = QDateTime::fromSecsSinceEpoch(1'700'000'000, Qt::UTC);
		const QString record =
			PerformanceSetup::missingRecord("1.19 fabric", then, {"a"});

		QVERIFY(PerformanceSetup::recentlyMissing(record, "1.19.2 fabric", then)
					.isEmpty());
		QVERIFY(PerformanceSetup::recentlyMissing(record, "1.19 neoforge", then)
					.isEmpty());
	}

	void aMissingRecordFromTheFutureIsNotTrusted()
	{
		const QDateTime then = QDateTime::fromSecsSinceEpoch(1'700'000'000, Qt::UTC);
		const QString record =
			PerformanceSetup::missingRecord("1.19 fabric", then, {"a"});

		/* The clock was set back since it was written. */
		QVERIFY(PerformanceSetup::recentlyMissing(record, "1.19 fabric",
												  then.addSecs(-1))
					.isEmpty());
	}

	void aGarbledMissingRecordMissesNothing()
	{
		const QDateTime now = QDateTime::fromSecsSinceEpoch(1'700'000'000, Qt::UTC);
		for (const QString& record :
			 {"", "garbage", "1.19 fabric", "1.19 fabric|1700000000",
			  "1.19 fabric|soon|a", "1.19 fabric||a", "1.19 fabric|1700000000|a|b",
			  "|1700000000|a"}) {
			QVERIFY2(PerformanceSetup::recentlyMissing(record, "1.19 fabric", now)
						 .isEmpty(),
					 qPrintable(record));
		}
	}

	void aRecordOfNothingMissingNamesNoProject()
	{
		const QDateTime now = QDateTime::fromSecsSinceEpoch(1'700'000'000, Qt::UTC);
		QVERIFY(PerformanceSetup::recentlyMissing(
					PerformanceSetup::missingRecord("1.19 fabric", now, {}),
					"1.19 fabric", now)
					.isEmpty());
	}

	void projectListsRoundTrip()
	{
		const QStringList projects = {"AANobbMI", "gvQqBUqZ"};
		QCOMPARE(PerformanceSetup::parseProjects(
					 PerformanceSetup::joinProjects(projects)),
				 projects);
		QVERIFY(PerformanceSetup::parseProjects("").isEmpty());
		QVERIFY(PerformanceSetup::joinProjects({}).isEmpty());
		QCOMPARE(PerformanceSetup::parseProjects("a,,b,"),
				 (QStringList{"a", "b"}));
	}
};

QTEST_GUILESS_MAIN(PerformanceSetupTest)

#include "PerformanceSetupTask_test.moc"
