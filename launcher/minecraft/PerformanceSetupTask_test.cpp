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

	void aVanillaInstanceGetsFabric()
	{
		QVERIFY(PerformanceSetup::needsLoader(QString()));
		QCOMPARE(PerformanceSetup::targetLoader(QString()), QString("fabric"));
	}

	void aModdedInstanceKeepsItsLoader()
	{
		for (const QString& loader : {"fabric", "forge", "quilt", "neoforge"}) {
			QVERIFY(!PerformanceSetup::needsLoader(loader));
			QCOMPARE(PerformanceSetup::targetLoader(loader), loader);
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

	void jarsWithoutARecordAreLoose()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QDir mods(dir.path());
		touch(mods, "recorded.jar");
		touch(mods, "recorded-off.jar.disabled");
		touch(mods, "by-hand.jar");
		touch(mods, "by-hand-off.jar.disabled");
		touch(mods, "notes.txt");

		ModMetadataIndex index(mods);
		for (const QString& name : {"recorded.jar", "recorded-off.jar"}) {
			ModMetadataIndex::Entry entry;
			entry.fileName = name;
			entry.platform = "modrinth";
			entry.projectId = "AANobbMI";
			QVERIFY(index.put(entry));
		}

		QCOMPARE(PerformanceSetup::looseJars(mods, index),
				 (QStringList{"by-hand-off.jar.disabled", "by-hand.jar"}));
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

	void exclusionsRoundTrip()
	{
		const QStringList projects = {"AANobbMI", "gvQqBUqZ"};
		QCOMPARE(PerformanceSetup::parseExcluded(
					 PerformanceSetup::joinExcluded(projects)),
				 projects);
		QVERIFY(PerformanceSetup::parseExcluded("").isEmpty());
		QVERIFY(PerformanceSetup::joinExcluded({}).isEmpty());
		QCOMPARE(PerformanceSetup::parseExcluded("a,,b,"),
				 (QStringList{"a", "b"}));
	}
};

QTEST_GUILESS_MAIN(PerformanceSetupTest)

#include "PerformanceSetupTask_test.moc"
