/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "modplatform/modrinth/ModrinthModpackModel.h"

/* Both functions under test are static and touch neither the network nor
 * LauncherContext, so they run on canned bytes. */
class ModrinthModpackModelTest : public QObject
{
	Q_OBJECT

  private slots:

	void test_ParseSearchResults()
	{
		/* Plain string literals, concatenated: moc's lexer mishandles a
		 * multi-line raw string (R"(...)") in a Q_OBJECT class. */
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
			"      \"categories\": [\"adventure\", \"fabric\"],"
			"      \"display_categories\": [\"adventure\"],"
			"      \"featured_gallery\": \"https://cdn.modrinth.com/data/abc12345/gallery/hero.png\","
			"      \"gallery\": [\"https://cdn.modrinth.com/data/abc12345/gallery/hero.png\","
			"                    \"https://cdn.modrinth.com/data/abc12345/gallery/second.png\"],"
			"      \"color\": 8383968"
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

		/* The third hit has no "title", which loadIndexedPack() requires: it is
		 * skipped without aborting the page. */
		QCOMPARE(packs.size(), 2);
		/* Read from the JSON's "total_hits", not the page's hit count. */
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
		QCOMPARE(full.displayCategories, QStringList({"adventure"}));
		QCOMPARE(full.featuredGalleryUrl,
				 QString("https://cdn.modrinth.com/data/abc12345/gallery/"
						 "hero.png"));
		QCOMPARE(full.galleryUrls,
				 QStringList({"https://cdn.modrinth.com/data/abc12345/"
							  "gallery/hero.png",
							  "https://cdn.modrinth.com/data/abc12345/"
							  "gallery/second.png"}));
		QCOMPARE(full.color, 8383968);

		const Modrinth::IndexedPack& minimal = packs.at(1);
		QCOMPARE(minimal.projectId, QString("def67890"));
		QCOMPARE(minimal.name, QString("Minimal Pack"));
		QCOMPARE(minimal.author, QString());
		QCOMPARE(minimal.downloads, 0);
		QCOMPARE(minimal.follows, 0);
		QVERIFY(minimal.gameVersions.isEmpty());
		QVERIFY(minimal.categories.isEmpty());
		/* No "display_categories", "featured_gallery", "gallery" or "color": each
		 * must default cleanly. */
		QVERIFY(minimal.displayCategories.isEmpty());
		QVERIFY(minimal.featuredGalleryUrl.isEmpty());
		QVERIFY(minimal.galleryUrls.isEmpty());
		QCOMPARE(minimal.color, -1);
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

		/* No "primary" flag but a single file: picked anyway. */
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

	/* The pack page's Links card: the project's own page first, then only what
	 * the project states. */
	void test_ParseLinks()
	{
		const QJsonObject project = QJsonDocument::fromJson(
			"{\"slug\":\"fabulously-optimized\","
			"\"source_url\":\"https://github.com/example/fo\","
			"\"issues_url\":null,\"wiki_url\":\"\","
			"\"discord_url\":\"https://discord.gg/example\"}").object();
		const QVariantList links = ModrinthModpackModel::parseLinks(project);
		QCOMPARE(links.size(), 3);
		QCOMPARE(links.at(0).toMap().value("kind").toString(),
				 QString("website"));
		QVERIFY(links.at(0).toMap().value("url").toString().endsWith(
			"/modpack/fabulously-optimized"));
		QCOMPARE(links.at(1).toMap().value("kind").toString(),
				 QString("source"));
		QCOMPARE(links.at(2).toMap().value("url").toString(),
				 QString("https://discord.gg/example"));
		QVERIFY(ModrinthModpackModel::parseLinks(QJsonObject()).isEmpty());
	}

	/* "What's inside": a version's dependencies name projects; a bare file
	 * name (a jar in the overrides) names none. */
	void test_ParseVersionDependencies()
	{
		const QByteArray json =
			"["
			" {\"id\":\"ver1\",\"dependencies\":["
			"   {\"project_id\":\"AANobbMI\",\"dependency_type\":\"embedded\"},"
			"   {\"project_id\":\"P7dR8mSH\",\"dependency_type\":\"embedded\"},"
			"   {\"project_id\":\"AANobbMI\",\"dependency_type\":\"embedded\"},"
			"   {\"project_id\":null,\"file_name\":\"private.jar\"}]},"
			" {\"id\":\"ver2\",\"dependencies\":[]},"
			" {\"id\":\"ver3\"}"
			"]";
		const auto deps = ModrinthModpackModel::parseVersionDependencies(json);
		QCOMPARE(deps.size(), 1);
		QCOMPARE(deps.value("ver1"), (QStringList{"AANobbMI", "P7dR8mSH"}));
		QVERIFY(ModrinthModpackModel::parseVersionDependencies("nope")
					.isEmpty());
	}

	void test_ParseContentsJson()
	{
		const QByteArray json =
			"["
			" {\"id\":\"AANobbMI\",\"slug\":\"sodium\",\"title\":\"Sodium\","
			"  \"description\":\"Fast.\",\"project_type\":\"mod\","
			"  \"icon_url\":\"https://cdn.modrinth.com/sodium.png\"},"
			" {\"id\":\"P7dR8mSH\",\"slug\":\"fabric-api\","
			"  \"title\":\"fabric API\",\"project_type\":\"mod\"},"
			" {\"id\":\"untitled\"}"
			"]";
		const QVariantList contents =
			ModrinthModpackModel::parseContentsJson(json);
		QCOMPARE(contents.size(), 2);
		/* By title, whatever the case. */
		QCOMPARE(contents.at(0).toMap().value("title").toString(),
				 QString("fabric API"));
		const QVariantMap sodium = contents.at(1).toMap();
		QCOMPARE(sodium.value("description").toString(), QString("Fast."));
		QCOMPARE(sodium.value("kind").toString(), QString("mod"));
		QCOMPARE(sodium.value("iconUrl").toString(),
				 QString("https://cdn.modrinth.com/sodium.png"));
		QVERIFY(sodium.value("url").toString().endsWith("sodium"));
	}
};

QTEST_GUILESS_MAIN(ModrinthModpackModelTest)

#include "ModrinthModpackModel_test.moc"
