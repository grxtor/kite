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
#include <QUrlQuery>

#include "modplatform/flame/FlameModpackModel.h"

/* The static, network-free halves of FlameModpackModel against canned
 * CurseForge replies - the same approach as ModrinthModpackModel_test.cpp,
 * and plain concatenated literals for the same moc reason given there. */
class FlameModpackModelTest : public QObject
{
	Q_OBJECT

  private slots:

	void test_ParseSearchResults()
	{
		const QByteArray json =
			"{\"data\": ["
			"  {\"id\": 285109, \"name\": \"All the Mods 9\", \"slug\": \"all-the-mods-9\","
			"   \"summary\": \"Kitchen sink.\", \"downloadCount\": 12345678.0,"
			"   \"thumbsUpCount\": 42, \"dateModified\": \"2026-09-01T10:00:00Z\","
			"   \"links\": {\"websiteUrl\": \"https://www.curseforge.com/minecraft/modpacks/all-the-mods-9\"},"
			"   \"logo\": {\"title\": \"atm.png\", \"thumbnailUrl\": \"https://media.forgecdn.net/atm.png\"},"
			"   \"authors\": [{\"name\": \"ATMTeam\", \"url\": \"\"}],"
			"   \"categories\": [{\"name\": \"Kitchen Sink\"}, {\"name\": \"Tech\"}],"
			"   \"screenshots\": [{\"url\": \"https://media.forgecdn.net/shot1.png\"}],"
			"   \"mainFileId\": 5000,"
			"   \"latestFiles\": [{\"id\": 5000, \"gameVersions\": [\"1.20.1\", \"Forge\"]}],"
			"   \"latestFilesIndexes\": [{\"gameVersion\": \"1.20.1\"}, {\"gameVersion\": \"1.20.1\"}]},"
			"  {\"id\": 1, \"name\": \"Broken\", \"authors\": [], \"mainFileId\": 9,"
			"   \"latestFiles\": []}"
			"], \"pagination\": {\"index\": 0, \"pageSize\": 25, \"resultCount\": 2, \"totalCount\": 77}}";

		int total = 0;
		const auto packs = FlameModpackModel::parseSearchResults(json, total);
		/* The pack with no usable file is dropped, as the widget list
		 * dropped it. */
		QCOMPARE(packs.size(), 1);
		QCOMPARE(total, 77);

		const auto& pack = packs.first();
		QCOMPARE(pack.base.addonId, 285109);
		QCOMPARE(pack.base.name, QString("All the Mods 9"));
		QCOMPARE(pack.base.logoUrl, QString("https://media.forgecdn.net/atm.png"));
		QCOMPARE(pack.slug, QString("all-the-mods-9"));
		QCOMPARE(pack.downloads, qint64(12345678));
		QCOMPARE(pack.thumbsUp, 42);
		QCOMPARE(pack.mainFileId, 5000);
		QCOMPARE(pack.categories, (QStringList{"Kitchen Sink", "Tech"}));
		QCOMPARE(pack.gameVersions, QStringList{"1.20.1"});
		QCOMPARE(pack.screenshots, QStringList{"https://media.forgecdn.net/shot1.png"});
	}

	void test_ParseSearchResultsBadJson()
	{
		int total = 0;
		QVERIFY(FlameModpackModel::parseSearchResults("not json", total).isEmpty());
		QCOMPARE(total, -1);
	}

	void test_SearchUrlFilters()
	{
		const QUrl url = FlameModpackModel::searchUrl("atm", 1, 25, "1.20.1", "neoforge");
		const QUrlQuery query(url);
		QCOMPARE(query.queryItemValue("gameVersion"), QString("1.20.1"));
		QCOMPARE(query.queryItemValue("modLoaderType"), QString("6"));
		QCOMPARE(query.queryItemValue("index"), QString("25"));

		const QUrlQuery any(FlameModpackModel::searchUrl("", 0, 0, "", ""));
		QVERIFY(!any.hasQueryItem("gameVersion"));
		QVERIFY(!any.hasQueryItem("modLoaderType"));
	}

	void test_ParseVersions()
	{
		const QByteArray json =
			"{\"data\": ["
			"  {\"id\": 100, \"displayName\": \"ATM9 0.2\", \"fileName\": \"atm9-0.2.zip\","
			"   \"downloadUrl\": \"https://edge.forgecdn.net/100/atm9-0.2.zip\","
			"   \"fileDate\": \"2026-01-01T00:00:00Z\", \"gameVersions\": [\"1.20.1\", \"Forge\"]},"
			"  {\"id\": 200, \"displayName\": \"ATM9 0.3\", \"fileName\": \"atm9-0.3.zip\","
			"   \"downloadUrl\": null,"
			"   \"fileDate\": \"2026-02-01T00:00:00Z\", \"gameVersions\": [\"NeoForge\", \"1.20.1\"]}"
			"]}";

		const QVariantList versions =
			FlameModpackModel::parseVersionsJson(json, "285109", 100);
		QCOMPARE(versions.size(), 2);

		/* Newest first. */
		const QVariantMap newest = versions.at(0).toMap();
		QCOMPARE(newest.value("id").toString(), QString("200"));
		QCOMPARE(newest.value("versionNumber").toString(), QString("ATM9 0.3"));
		QCOMPARE(newest.value("loaders").toStringList(), QStringList{"neoforge"});
		QCOMPARE(newest.value("gameVersions").toStringList(), QStringList{"1.20.1"});
		/* No API download: the browser address stands in. */
		QVERIFY(newest.value("downloadUrl").toString().contains("285109"));
		QVERIFY(newest.value("downloadUrl").toString().contains("200"));
		QVERIFY(!newest.value("featured").toBool());

		const QVariantMap older = versions.at(1).toMap();
		QVERIFY(older.value("featured").toBool());
		QCOMPARE(older.value("downloadUrl").toString(),
				 QString("https://edge.forgecdn.net/100/atm9-0.2.zip"));
	}

	void test_ParseBody()
	{
		QCOMPARE(FlameModpackModel::parseBody("{\"data\": \"<p>Hello</p>\"}"),
				 QString("<p>Hello</p>"));
		/* Players, frames and scripts go; text, links and pictures stay
		 * (RichBody lays the pictures out and loads only http(s) ones). */
		const QString html =
			"<p>Intro <a href=\"https://x\">site</a></p>"
			"<a href=\"https://img\"><img src=\"https://cdn/a.png\" width=\"900\"></a>"
			"<iframe src=\"https://www.youtube.com/embed/1\"></iframe>"
			"<IMG SRC=\"https://cdn/b.gif\"/><script>alert(1)</script><p>End</p>";
		QCOMPARE(FlameModpackModel::sanitizeBody(html),
				 QString("<p>Intro <a href=\"https://x\">site</a></p>"
						 "<a href=\"https://img\"><img src=\"https://cdn/a.png\" width=\"900\"></a>"
						 "<IMG SRC=\"https://cdn/b.gif\"/><p>End</p>"));
		QVERIFY(FlameModpackModel::parseBody("garbage").isEmpty());
	}
};

QTEST_GUILESS_MAIN(FlameModpackModelTest)

#include "FlameModpackModel_test.moc"
