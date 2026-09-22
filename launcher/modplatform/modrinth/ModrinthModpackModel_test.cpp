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

#include <QTest>

#include "modplatform/modrinth/ModrinthModpackModel.h"

/* Both functions under test are static and touch neither the network nor
 * LauncherContext, exactly so they can be exercised here with canned bytes
 * - see the class comment on ModrinthModpackModel::parseSearchResults(). */
class ModrinthModpackModelTest : public QObject
{
	Q_OBJECT

  private slots:

	void test_ParseSearchResults()
	{
		/* Plain (non-raw) string literals, concatenated: moc's lexer
		 * mishandles a *multi-line* raw string literal (R"(...)") in a
		 * Q_OBJECT class - it stops finding any class at all, hence the
		 * escaped quotes here rather than the more readable R"(...)"
		 * this codebase otherwise uses for a single-line JSON literal
		 * (see PackContents_test.cpp). */
		const QByteArray json =
			"{"
			"  \"hits\": ["
			"    {"
			"      \"project_id\": \"abc12345\","
			"      \"slug\": \"vault-hunters\","
			"      \"title\": \"Vault Hunters\","
			"      \"description\": \"A modpack about vaults.\","
			"      \"author\": \"iskallia\","
			"      \"icon_url\": \"https://cdn.modrinth.com/data/abc12345/icon.png\","
			"      \"downloads\": 123456,"
			"      \"follows\": 789,"
			"      \"date_modified\": \"2026-01-01T00:00:00Z\","
			"      \"latest_version\": \"ver1\","
			"      \"versions\": [\"1.20.1\", \"1.20.2\"],"
			"      \"categories\": [\"adventure\", \"fabric\"]"
			"    },"
			"    {"
			"      \"project_id\": \"def67890\","
			"      \"slug\": \"no-optionals\","
			"      \"title\": \"Minimal Pack\""
			"    },"
			"    {"
			"      \"project_id\": \"ghijklmn\""
			"    }"
			"  ],"
			"  \"total_hits\": 50"
			"}";

		int totalHits = -1;
		const QList<Modrinth::IndexedPack> packs =
			ModrinthModpackModel::parseSearchResults(json, totalHits);

		/* The third hit has no "title", which loadIndexedPack() requires
		 * - it must be skipped rather than crashing or aborting the rest
		 * of the page. */
		QCOMPARE(packs.size(), 2);
		/* Read straight off the JSON's own "total_hits", independent of
		 * how many hits this one page happened to carry. */
		QCOMPARE(totalHits, 50);

		const Modrinth::IndexedPack& full = packs.at(0);
		QCOMPARE(full.projectId, QString("abc12345"));
		QCOMPARE(full.slug, QString("vault-hunters"));
		QCOMPARE(full.name, QString("Vault Hunters"));
		QCOMPARE(full.description, QString("A modpack about vaults."));
		QCOMPARE(full.author, QString("iskallia"));
		QCOMPARE(full.iconUrl,
				 QString("https://cdn.modrinth.com/data/abc12345/icon.png"));
		QCOMPARE(full.downloads, 123456);
		QCOMPARE(full.follows, 789);
		QCOMPARE(full.dateModified, QString("2026-01-01T00:00:00Z"));
		QCOMPARE(full.latestVersion, QString("ver1"));
		QCOMPARE(full.gameVersions,
				 QStringList({"1.20.1", "1.20.2"}));
		QCOMPARE(full.categories,
				 QStringList({"adventure", "fabric"}));

		const Modrinth::IndexedPack& minimal = packs.at(1);
		QCOMPARE(minimal.projectId, QString("def67890"));
		QCOMPARE(minimal.name, QString("Minimal Pack"));
		QCOMPARE(minimal.author, QString());
		QCOMPARE(minimal.downloads, 0);
		QCOMPARE(minimal.follows, 0);
		QVERIFY(minimal.gameVersions.isEmpty());
		QVERIFY(minimal.categories.isEmpty());
	}

	void test_ParseSearchResultsOnGarbageIsEmpty()
	{
		int totalHits = 0;
		const QList<Modrinth::IndexedPack> packs =
			ModrinthModpackModel::parseSearchResults("not json at all",
													 totalHits);
		QVERIFY(packs.isEmpty());
	}

	void test_ParseVersionsJson()
	{
		const QByteArray json =
			"["
			"  {"
			"    \"id\": \"ver1\","
			"    \"project_id\": \"abc12345\","
			"    \"name\": \"Release 1.2.3\","
			"    \"version_number\": \"1.2.3\","
			"    \"game_versions\": [\"1.20.1\"],"
			"    \"loaders\": [\"forge\"],"
			"    \"date_published\": \"2026-01-01T00:00:00Z\","
			"    \"featured\": true,"
			"    \"files\": ["
			"      {"
			"        \"primary\": true,"
			"        \"url\": \"https://cdn.modrinth.com/data/abc12345/versions/ver1/pack.mrpack\","
			"        \"size\": 1024,"
			"        \"hashes\": {\"sha1\": \"deadbeef\"}"
			"      }"
			"    ]"
			"  },"
			"  {"
			"    \"id\": \"ver2\","
			"    \"version_number\": \"1.2.2\","
			"    \"game_versions\": [\"1.19.2\", \"1.19.4\"],"
			"    \"loaders\": [\"forge\", \"neoforge\"],"
			"    \"date_published\": \"2025-06-01T00:00:00Z\","
			"    \"files\": ["
			"      {"
			"        \"url\": \"https://cdn.modrinth.com/data/abc12345/versions/ver2/pack.mrpack\""
			"      }"
			"    ]"
			"  }"
			"]";

		const QVariantList versions =
			ModrinthModpackModel::parseVersionsJson(json, "abc12345");
		QCOMPARE(versions.size(), 2);

		const QVariantMap first = versions.at(0).toMap();
		QCOMPARE(first.value("id").toString(), QString("ver1"));
		QCOMPARE(first.value("name").toString(), QString("Release 1.2.3"));
		QCOMPARE(first.value("versionNumber").toString(), QString("1.2.3"));
		QCOMPARE(first.value("gameVersions").toStringList(),
				 QStringList({"1.20.1"}));
		QCOMPARE(first.value("loaders").toStringList(),
				 QStringList({"forge"}));
		QCOMPARE(first.value("datePublished").toString(),
				 QString("2026-01-01T00:00:00Z"));
		QCOMPARE(first.value("downloadUrl").toString(),
				 QString("https://cdn.modrinth.com/data/abc12345/versions/"
						 "ver1/pack.mrpack"));
		QCOMPARE(first.value("featured").toBool(), true);

		/* No "primary" flag, but a single file - loadIndexedPackVersions()
		 * picks it anyway, same as the widget browser has always done. */
		const QVariantMap second = versions.at(1).toMap();
		QCOMPARE(second.value("id").toString(), QString("ver2"));
		QCOMPARE(second.value("gameVersions").toStringList(),
				 QStringList({"1.19.2", "1.19.4"}));
		QCOMPARE(second.value("loaders").toStringList(),
				 QStringList({"forge", "neoforge"}));
		QCOMPARE(second.value("featured").toBool(), false);
		QCOMPARE(second.value("downloadUrl").toString(),
				 QString("https://cdn.modrinth.com/data/abc12345/versions/"
						 "ver2/pack.mrpack"));
	}
};

QTEST_GUILESS_MAIN(ModrinthModpackModelTest)

#include "ModrinthModpackModel_test.moc"
