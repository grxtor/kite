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

#include "modplatform/technic/TechnicModpackModel.h"

/* The static, network-free halves of TechnicModpackModel against canned
 * Technic replies: the search addresses, the hits, and the metadata that
 * decides between a Solder install and a single zip. */
class TechnicModpackModelTest : public QObject
{
	Q_OBJECT

  private slots:
	void test_SearchUrls()
	{
		const QUrl trending = TechnicModpackModel::searchUrl("  ");
		QVERIFY(trending.path().endsWith("/trending"));
		QVERIFY(QUrlQuery(trending).hasQueryItem("build"));
		QVERIFY(!QUrlQuery(trending).hasQueryItem("q"));

		const QUrl search = TechnicModpackModel::searchUrl("tekkit & friends");
		QVERIFY(search.path().endsWith("/search"));
		/* One parameter, "&" and all. */
		QCOMPARE(QUrlQuery(search).queryItemValue("q", QUrl::FullyDecoded),
				 QString("tekkit & friends"));

		QVERIFY(TechnicModpackModel::metadataUrl("tekkitmain").path().endsWith(
			"/modpack/tekkitmain"));
	}

	void test_ParseSearch()
	{
		const QByteArray json =
			"{\"modpacks\": ["
			"  {\"name\": \"Tekkit\", \"slug\": \"tekkitmain\","
			"   \"iconUrl\": \"https://cdn.technicpack.net/platform2/pack-icons/42.png?1\"},"
			"  {\"name\": \"Vanilla\", \"slug\": \"vanilla\", \"iconUrl\": null},"
			"  {\"name\": \"Hexxit\", \"slug\": \"hexxit\", \"iconUrl\": null}"
			"]}";
		const auto packs = TechnicModpackModel::parseSearch(json);
		QCOMPARE(packs.size(), 2);
		QCOMPARE(packs.at(0).slug, QString("tekkitmain"));
		QCOMPARE(packs.at(0).logoName, QString("42"));
		QVERIFY(packs.at(1).logoUrl.isEmpty());
		QVERIFY(TechnicModpackModel::parseSearch("broken").isEmpty());
	}

	void test_ParseMetadataZipAndSolder()
	{
		TechnicModpackModel::Pack zipPack;
		zipPack.slug = "hexxit";
		QVERIFY(TechnicModpackModel::parseMetadata(
			"{\"url\": \"https://example.org/hexxit.zip\", \"solder\": null,"
			" \"minecraft\": \"1.5.2\", \"version\": \"1.0.10\", \"user\": \"Technic\","
			" \"description\": \"<b>Adventure</b>\", \"platformUrl\": \"https://www.technicpack.net/modpack/hexxit\","
			" \"background\": {\"url\": \"https://cdn/bg.png\"}, \"downloads\": 1000}",
			zipPack));
		QVERIFY(!zipPack.isSolder);
		QCOMPARE(zipPack.url, QString("https://example.org/hexxit.zip"));
		QCOMPARE(zipPack.minecraftVersion, QString("1.5.2"));
		QCOMPARE(zipPack.author, QString("Technic"));
		QCOMPARE(zipPack.backgroundUrl, QString("https://cdn/bg.png"));
		QCOMPARE(zipPack.downloads, qint64(1000));
		QVERIFY(zipPack.metadataLoaded);

		TechnicModpackModel::Pack solderPack;
		solderPack.slug = "tekkitmain";
		QVERIFY(TechnicModpackModel::parseMetadata(
			"{\"url\": null, \"solder\": \"https://solder.technicpack.net/api/\","
			" \"minecraft\": \"1.12.2\"}",
			solderPack));
		QVERIFY(solderPack.isSolder);
		QCOMPARE(TechnicModpackModel::solderPackUrl(solderPack),
				 QString("https://solder.technicpack.net/api/modpack/tekkitmain"));

		/* Nothing to install from. */
		TechnicModpackModel::Pack empty;
		QVERIFY(!TechnicModpackModel::parseMetadata("{\"url\": null}", empty));
		QVERIFY(!empty.metadataLoaded);
	}

	void test_VersionEntries()
	{
		TechnicModpackModel::Pack pack;
		pack.version = "1.2.9";
		pack.minecraftVersion = "1.12.2";
		const QVariantList versions = TechnicModpackModel::versionEntries(pack);
		QCOMPARE(versions.size(), 1);
		QCOMPARE(versions.first().toMap().value("name").toString(), QString("1.2.9"));
		QCOMPARE(versions.first().toMap().value("gameVersions").toStringList(),
				 QStringList{"1.12.2"});
		QVERIFY(versions.first().toMap().value("featured").toBool());
	}
};

QTEST_GUILESS_MAIN(TechnicModpackModelTest)

#include "TechnicModpackModel_test.moc"
