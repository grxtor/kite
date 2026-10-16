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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "modplatform/modpacksch/FtbModpackModel.h"

/* The static, network-free halves of FtbModpackModel: the catalogue index,
 * one pack's reply (built here with every field ModpacksCH::loadModpack()
 * requires), the local search and sorts, and the version list. */
class FtbModpackModelTest : public QObject
{
	Q_OBJECT

  private:
	static QJsonObject version(int id, const QString& name, const QString& type,
							   qint64 updated)
	{
		return QJsonObject{
			{"id", id},
			{"name", name},
			{"type", type},
			{"updated", updated},
			{"specs", QJsonObject{{"id", 1}, {"minimum", 4096}, {"recommended", 6144}}}};
	}

	static QByteArray pack(int id, const QString& name, int plays, int installs,
						   const QStringList& tags, const QJsonArray& versions)
	{
		QJsonArray tagArray;
		int tagId = 1;
		for (const QString& tag : tags) {
			tagArray.append(QJsonObject{{"id", tagId++}, {"name", tag}});
		}
		const QJsonArray art = {
			QJsonObject{{"id", 1}, {"url", "https://cdn.example/square.png"},
						{"type", "square"}, {"width", 64}, {"height", 64},
						{"compressed", false}, {"sha1", ""}, {"size", 1},
						{"updated", 1}},
			QJsonObject{{"id", 2}, {"url", "https://cdn.example/splash.png"},
						{"type", "splash"}, {"width", 640}, {"height", 360},
						{"compressed", false}, {"sha1", ""}, {"size", 1},
						{"updated", 1}}};
		const QJsonArray authors = {QJsonObject{{"id", 1}, {"name", "FTB Team"},
												{"type", "team"}, {"website", ""},
												{"updated", 1}}};
		const QJsonObject obj{
			{"id", id},
			{"name", name},
			{"synopsis", name + " in short"},
			{"description", "# " + name},
			{"type", "modpack"},
			{"featured", false},
			{"installs", installs},
			{"plays", plays},
			{"updated", 1700000000 + id},
			{"refreshed", 1},
			{"art", art},
			{"authors", authors},
			{"versions", versions},
			{"tags", tagArray}};
		return QJsonDocument(obj).toJson();
	}

  private slots:
	void test_ParseIndex()
	{
		QCOMPARE(FtbModpackModel::parseIndex("{\"packs\": [5, 23, 0, 7]}"),
				 (QList<int>{5, 23, 7}));
		QVERIFY(FtbModpackModel::parseIndex("nope").isEmpty());
	}

	void test_ParsePack()
	{
		ModpacksCH::Modpack parsed;
		QVERIFY(FtbModpackModel::parsePack(
			pack(23, "FTB Skies", 10, 20, {"Forge", "Skyblock"},
				 {version(100, "1.0.0", "release", 1700000000)}),
			parsed));
		QCOMPARE(parsed.id, 23);
		QCOMPARE(parsed.name, QString("FTB Skies"));
		QCOMPARE(FtbModpackModel::artUrl(parsed, "square"),
				 QString("https://cdn.example/square.png"));
		QCOMPARE(FtbModpackModel::artUrl(parsed, "splash"),
				 QString("https://cdn.example/splash.png"));
		QVERIFY(FtbModpackModel::artUrl(parsed, "logo").isEmpty());

		/* A pack with no version is skipped, as the widget list did. */
		ModpacksCH::Modpack dud;
		QVERIFY(!FtbModpackModel::parsePack(pack(1, "Dud", 0, 0, {}, {}), dud));
		/* A reply missing required fields does not parse. */
		ModpacksCH::Modpack broken;
		QVERIFY(!FtbModpackModel::parsePack("{\"id\": 3}", broken));
	}

	void test_FilterAndSort()
	{
		QList<ModpacksCH::Modpack> packs;
		const QList<QByteArray> replies = {
			pack(1, "FTB Skies", 50, 300, {"Forge", "Skyblock"},
				 {version(1, "1", "release", 1)}),
			pack(2, "FTB Evolution", 90, 100, {"NeoForge"},
				 {version(2, "1", "release", 1)}),
			pack(3, "Academy", 10, 900, {"Learning"},
				 {version(3, "1", "release", 1)}),
		};
		for (const auto& reply : replies) {
			ModpacksCH::Modpack p;
			QVERIFY(FtbModpackModel::parsePack(reply, p));
			packs.append(p);
		}

		/* Most played first by default. */
		QCOMPARE(FtbModpackModel::filterAndSort(packs, "", "", "plays"),
				 (QList<int>{1, 0, 2}));
		QCOMPARE(FtbModpackModel::filterAndSort(packs, "", "", "installs"),
				 (QList<int>{2, 0, 1}));
		QCOMPARE(FtbModpackModel::filterAndSort(packs, "", "", "name"),
				 (QList<int>{2, 1, 0}));
		/* Name, summary or tag. */
		QCOMPARE(FtbModpackModel::filterAndSort(packs, "skyblock", "", "plays"),
				 QList<int>{0});
		QCOMPARE(FtbModpackModel::filterAndSort(packs, "EVOLUTION", "", "plays"),
				 QList<int>{1});
		/* A loader rules out only packs tagged with another one. */
		QCOMPARE(FtbModpackModel::filterAndSort(packs, "", "neoforge", "plays"),
				 (QList<int>{1, 2}));
	}

	void test_VersionEntries()
	{
		ModpacksCH::Modpack p;
		QVERIFY(FtbModpackModel::parsePack(
			pack(9, "FTB Skies", 1, 1, {},
				 {version(10, "1.0.0", "release", 1000),
				  version(12, "1.2.0-beta", "beta", 3000),
				  version(11, "1.1.0", "release", 2000)}),
			p));
		const QVariantList versions = FtbModpackModel::versionEntries(p);
		QCOMPARE(versions.size(), 3);
		/* Newest first; the newest release is featured, not the beta. */
		QCOMPARE(versions.at(0).toMap().value("name").toString(), QString("1.2.0-beta"));
		QVERIFY(!versions.at(0).toMap().value("featured").toBool());
		QCOMPARE(versions.at(1).toMap().value("id").toString(), QString("11"));
		QVERIFY(versions.at(1).toMap().value("featured").toBool());
		QVERIFY(!versions.at(2).toMap().value("featured").toBool());
	}
};

QTEST_GUILESS_MAIN(FtbModpackModelTest)

#include "FtbModpackModel_test.moc"
