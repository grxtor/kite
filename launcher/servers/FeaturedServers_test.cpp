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

#include <QSignalSpy>
#include <QTest>

#include "servers/FeaturedServers.h"

namespace
{
	const QByteArray kList = R"({
		"formatVersion": 1,
		"servers": [
			{ "id": "big", "name": "Big Network", "address": "play.big.example",
			  "note": { "en": "Mini games", "tr": "Mini oyunlar" }, "minVersion": "1.8" },
			{ "id": "old", "name": "Old Days", "address": "old.example:25570",
			  "note": { "en": "English only" }, "minVersion": "1.7.10", "maxVersion": "1.12" }
		]
	})";
} // namespace

class FeaturedServersTest : public QObject
{
	Q_OBJECT

  private slots:
	void theBuiltInListIsHypixelThenCubeCraft()
	{
		FeaturedServers featured;
		const QVariantList servers = featured.servers();
		QCOMPARE(servers.size(), 2);
		const QVariantMap hypixel = servers.at(0).toMap();
		QCOMPARE(hypixel.value("id").toString(), QString("hypixel"));
		QCOMPARE(hypixel.value("address").toString(), QString("mc.hypixel.net"));
		QCOMPARE(hypixel.value("minVersion").toString(), QString("1.8"));
		QCOMPARE(hypixel.value("maxVersion").toString(), QString());
		const QVariantMap cubeCraft = servers.at(1).toMap();
		QCOMPARE(cubeCraft.value("id").toString(), QString("cubecraft"));
		QCOMPARE(cubeCraft.value("address").toString(), QString("play.cubecraft.net"));
	}

	void loadsAList()
	{
		FeaturedServers featured;
		featured.setLanguage("en_US");
		QSignalSpy changed(&featured, &FeaturedServers::changed);
		QString error;
		QVERIFY2(featured.loadFromJson(kList, &error), qPrintable(error));
		QCOMPARE(changed.count(), 1);
		const QVariantList servers = featured.servers();
		QCOMPARE(servers.size(), 2);
		const QVariantMap big = servers.at(0).toMap();
		QCOMPARE(big.value("name").toString(), QString("Big Network"));
		QCOMPARE(big.value("address").toString(), QString("play.big.example"));
		QCOMPARE(big.value("note").toString(), QString("Mini games"));
		const QVariantMap old = servers.at(1).toMap();
		QCOMPARE(old.value("minVersion").toString(), QString("1.7.10"));
		QCOMPARE(old.value("maxVersion").toString(), QString("1.12"));
	}

	void notesFollowTheLanguage()
	{
		FeaturedServers featured;
		QVERIFY(featured.loadFromJson(kList));
		featured.setLanguage("tr_TR");
		QCOMPARE(featured.servers().at(0).toMap().value("note").toString(), QString("Mini oyunlar"));
		// No Turkish: the English one.
		QCOMPARE(featured.servers().at(1).toMap().value("note").toString(), QString("English only"));
	}

	void refusesABrokenListAndKeepsTheOldOne()
	{
		FeaturedServers featured;
		QVERIFY(featured.loadFromJson(kList));
		const QList<QByteArray> broken = {
			"not json",
			R"({"formatVersion": 2, "servers": []})",
			R"({"formatVersion": 1})",
			R"({"formatVersion": 1, "servers": [{"id": "a", "address": "a.example"}]})",
			R"({"formatVersion": 1, "servers": [{"id": "a", "name": "A"}]})",
			R"({"formatVersion": 1, "servers": [{"id": "a", "name": "A", "address": "a.example"},
			                                    {"id": "a", "name": "B", "address": "b.example"}]})",
		};
		for (const QByteArray& json : broken) {
			QString error;
			QVERIFY2(!featured.loadFromJson(json, &error), json.constData());
			QVERIFY(!error.isEmpty());
		}
		QCOMPARE(featured.servers().size(), 2);
	}

	void versionsFitTheRange()
	{
		QVERIFY(FeaturedServers::versionFits("1.21.4", "1.8", ""));
		QVERIFY(FeaturedServers::versionFits("1.8.9", "1.8", ""));
		QVERIFY(FeaturedServers::versionFits("26.3", "1.8", ""));
		QVERIFY(!FeaturedServers::versionFits("1.7.10", "1.8", ""));
		QVERIFY(FeaturedServers::versionFits("1.12.2", "1.7.10", "1.12"));
		QVERIFY(FeaturedServers::versionFits("1.12", "1.7.10", "1.12"));
		QVERIFY(!FeaturedServers::versionFits("1.13", "1.7.10", "1.12"));
		QVERIFY(!FeaturedServers::versionFits("1.20.6", "1.21", ""));
		QVERIFY(FeaturedServers::versionFits("1.0", "", ""));
		// An instance whose version is not known yet fits nothing.
		QVERIFY(!FeaturedServers::versionFits("", "", ""));
	}
};

QTEST_GUILESS_MAIN(FeaturedServersTest)

#include "FeaturedServers_test.moc"
