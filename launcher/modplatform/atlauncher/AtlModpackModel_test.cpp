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

#include "meta/Version.h"
#include "modplatform/atlauncher/AtlModpackModel.h"

/* The static, network-free halves of AtlModpackModel: the catalogue and
 * its filters, the local search and sorts, the version list, and which
 * loader versions an open-loader pack is offered. */
class AtlModpackModelTest : public QObject
{
	Q_OBJECT

  private:
	static QJsonObject pack(int id, int position, const QString& name,
							const QString& type, bool system,
							const QJsonArray& versions)
	{
		return QJsonObject{{"id", id},
						   {"position", position},
						   {"name", name},
						   {"type", type},
						   {"system", system},
						   {"description", name + " description"},
						   {"versions", versions}};
	}
	static QJsonArray versions(const QList<QPair<QString, QString>>& list)
	{
		QJsonArray array;
		for (const auto& v : list) {
			array.append(QJsonObject{{"version", v.first}, {"minecraft", v.second}});
		}
		return array;
	}
	static QList<ATLauncher::IndexedPack> catalogue()
	{
		const QJsonArray all = {
			pack(1, 5, "Sky Factory", "public", false, versions({{"4.2", "1.12.2"}})),
			pack(2, 1, "Vanilla", "public", true, versions({{"1", "1.20.1"}})),
			pack(3, 2, "Secret", "private", false, versions({{"1", "1.20.1"}})),
			pack(4, 3, "Empty", "public", false, versions({})),
			pack(5, 2, "Allomancy Pack", "public", false,
				 versions({{"2.0", "1.20.1"}, {"1.0", "1.19.2"}})),
			QJsonObject{{"id", 6}},
		};
		return AtlModpackModel::parseCatalogue(QJsonDocument(all).toJson());
	}

  private slots:
	void test_CatalogueKeepsWhatAtlPageListed()
	{
		const auto packs = catalogue();
		/* Public, not "system", with a version; the broken entry is
		 * skipped rather than losing the list. */
		QCOMPARE(packs.size(), 2);
		QCOMPARE(packs.at(0).name, QString("Sky Factory"));
		QCOMPARE(packs.at(1).name, QString("Allomancy Pack"));
		QVERIFY(AtlModpackModel::logoUrl(packs.at(0)).endsWith("launcher/images/skyfactory.png"));
	}

	void test_FilterAndSort()
	{
		const auto packs = catalogue();
		/* ATLauncher's own order: lower position first. */
		QCOMPARE(AtlModpackModel::filterAndSort(packs, "", "", "popularity"),
				 (QList<int>{1, 0}));
		QCOMPARE(AtlModpackModel::filterAndSort(packs, "", "", "name"),
				 (QList<int>{1, 0}));
		QCOMPARE(AtlModpackModel::filterAndSort(packs, "", "", "gameVersion"),
				 (QList<int>{1, 0}));
		QCOMPARE(AtlModpackModel::filterAndSort(packs, "factory", "", "popularity"),
				 QList<int>{0});
		QCOMPARE(AtlModpackModel::filterAndSort(packs, "", "1.19.2", "popularity"),
				 QList<int>{1});
		QVERIFY(AtlModpackModel::filterAndSort(packs, "nothing", "", "name").isEmpty());
	}

	void test_VersionEntries()
	{
		const auto packs = catalogue();
		const QVariantList entries = AtlModpackModel::versionEntries(packs.at(1));
		QCOMPARE(entries.size(), 2);
		QCOMPARE(entries.at(0).toMap().value("id").toString(), QString("2.0"));
		QCOMPARE(entries.at(0).toMap().value("gameVersions").toStringList(),
				 QStringList{"1.20.1"});
		QVERIFY(entries.at(0).toMap().value("featured").toBool());
		QVERIFY(!entries.at(1).toMap().value("featured").toBool());
	}

	void test_LoaderVersionChoices()
	{
		auto make = [](const QString& version, const QString& mc, bool recommended) {
			auto v = std::make_shared<Meta::Version>("net.minecraftforge", version);
			Meta::Require req;
			req.uid = "net.minecraft";
			req.equalsVersion = mc;
			v->setRequires({req}, {});
			v->setRecommended(recommended);
			return v;
		};
		const QVector<Meta::VersionPtr> all = {
			make("47.3.0", "1.20.1", false), make("47.2.0", "1.20.1", true),
			make("43.0.0", "1.19.2", false)};

		const auto forMc = AtlModpackModel::versionChoices(all, "1.20.1", 5);
		QCOMPARE(forMc.size(), 2);
		/* Recommended first. */
		QCOMPARE(forMc.first()->version(), QString("47.2.0"));
		QCOMPARE(AtlModpackModel::versionChoices(all, "", 5).size(), 3);
		QCOMPARE(AtlModpackModel::versionChoices(all, "", 1).size(), 1);
		QVERIFY(AtlModpackModel::versionChoices(all, "1.7.10", 5).isEmpty());
	}
};

QTEST_GUILESS_MAIN(AtlModpackModelTest)

#include "AtlModpackModel_test.moc"
