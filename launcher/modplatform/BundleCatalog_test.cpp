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

#include <QSet>
#include <QSignalSpy>
#include <QTest>

#include "modplatform/BundleCatalog.h"

namespace
{
	/* Two bundles sharing "shared"; "forgeonly" is offered to Forge alone;
	 * the preset names a bundle and an options set. */
	const QByteArray kCatalog = R"({
		"formatVersion": 1,
		"bundles": [
			{
				"id": "speed",
				"name": { "en": "Speed", "tr": "Hiz" },
				"categories": [ { "id": "core", "name": { "en": "Core", "tr": "Cekirdek" } } ],
				"mods": [
					{ "project": "shared", "name": "Shared", "category": "core", "default": true,
					  "summary": { "en": "English only" } },
					{ "project": "forgeonly", "name": "Forge Only", "category": "core", "default": false,
					  "loaders": ["forge"], "serverSafe": false, "requires": ["shared"],
					  "summary": { "en": "Forge", "tr": "Forge tr" } }
				]
			},
			{
				"id": "pvp",
				"name": { "tr": "Sadece Turkce" },
				"categories": [],
				"mods": [
					{ "project": "shared", "name": "Shared again", "category": "none", "default": false },
					{ "project": "forgeonly", "name": "Forge Only", "category": "none", "default": true,
					  "loaders": ["forge"] }
				]
			}
		],
		"presets": [
			{ "id": "fast", "name": { "en": "Fast", "tr": "Hizli" }, "summary": { "en": "Quick" },
			  "minecraft": "1.8.9", "loader": "forge", "bundles": ["speed", "pvp"], "options": "fps" }
		],
		"options": { "fps": { "modern": { "maxFps": "260" }, "legacy": {} } }
	})";

	QStringList projectsOf(const QVariantList& mods)
	{
		QStringList projects;
		for (const QVariant& mod : mods) {
			projects.append(mod.toMap().value("project").toString());
		}
		return projects;
	}
} // namespace

class BundleCatalogTest : public QObject
{
	Q_OBJECT

  private slots:
	void loadsAFixture()
	{
		BundleCatalog catalog;
		QSignalSpy changed(&catalog, &BundleCatalog::changed);
		QString error;
		QVERIFY2(catalog.loadFromJson(kCatalog, &error), qPrintable(error));
		QCOMPARE(changed.count(), 1);

		const QVariantList presets = catalog.presets();
		QCOMPARE(presets.size(), 1);
		const QVariantMap preset = presets.first().toMap();
		QCOMPARE(preset.value("id").toString(), QString("fast"));
		QCOMPARE(preset.value("minecraft").toString(), QString("1.8.9"));
		QCOMPARE(preset.value("loader").toString(), QString("forge"));
		QCOMPARE(preset.value("bundles").toStringList(),
				 (QStringList{"speed", "pvp"}));
		QCOMPARE(preset.value("options").toString(), QString("fps"));
	}

	void textFollowsTheLanguage()
	{
		BundleCatalog catalog;
		QVERIFY(catalog.loadFromJson(kCatalog));

		catalog.setLanguage("tr");
		QCOMPARE(catalog.presets().first().toMap().value("name").toString(),
				 QString("Hizli"));
		catalog.setLanguage("tr_TR");
		QCOMPARE(catalog.presets().first().toMap().value("name").toString(),
				 QString("Hizli"));
		catalog.setLanguage("de");
		QCOMPARE(catalog.presets().first().toMap().value("name").toString(),
				 QString("Fast"));
		catalog.setLanguage("en_US");
		QCOMPARE(catalog.presets().first().toMap().value("name").toString(),
				 QString("Fast"));

		/* Turkish asked for, only English written: English. */
		catalog.setLanguage("tr");
		QCOMPARE(catalog.presets().first().toMap().value("summary").toString(),
				 QString("Quick"));
		const QVariantList mods = catalog.mods({"speed"}, "fabric");
		QCOMPARE(mods.first().toMap().value("summary").toString(),
				 QString("English only"));

		/* English asked for, only Turkish written: whatever there is. */
		catalog.setLanguage("en");
		QCOMPARE(catalog.mods({"pvp"}, "forge").first().toMap()
					 .value("bundleName").toString(),
				 QString("Sadece Turkce"));
	}

	void modsAreFilteredByLoader()
	{
		BundleCatalog catalog;
		QVERIFY(catalog.loadFromJson(kCatalog));
		catalog.setLanguage("en");

		QCOMPARE(projectsOf(catalog.mods({"speed"}, "forge")),
				 (QStringList{"shared", "forgeonly"}));
		QCOMPARE(projectsOf(catalog.mods({"speed"}, "fabric")),
				 (QStringList{"shared"}));
		QVERIFY(catalog.mods({"speed"}, QString()).isEmpty());
		QVERIFY(catalog.mods({"nonexistent"}, "forge").isEmpty());
	}

	void modsCarryTheirDetails()
	{
		BundleCatalog catalog;
		QVERIFY(catalog.loadFromJson(kCatalog));
		catalog.setLanguage("tr");

		const QVariantList mods = catalog.mods({"speed"}, "forge");
		const QVariantMap shared = mods.at(0).toMap();
		QCOMPARE(shared.value("bundle").toString(), QString("speed"));
		QCOMPARE(shared.value("bundleName").toString(), QString("Hiz"));
		QCOMPARE(shared.value("category").toString(), QString("core"));
		QCOMPARE(shared.value("categoryName").toString(), QString("Cekirdek"));
		QVERIFY(shared.value("isDefault").toBool());
		QVERIFY(shared.value("serverSafe").toBool());
		QVERIFY(shared.value("requires").toStringList().isEmpty());

		const QVariantMap forgeOnly = mods.at(1).toMap();
		QVERIFY(!forgeOnly.value("isDefault").toBool());
		QVERIFY(!forgeOnly.value("serverSafe").toBool());
		QCOMPARE(forgeOnly.value("requires").toStringList(), QStringList{"shared"});
	}

	void aProjectInTwoBundlesIsListedOnce()
	{
		BundleCatalog catalog;
		QVERIFY(catalog.loadFromJson(kCatalog));
		catalog.setLanguage("en");

		const QVariantList mods = catalog.mods({"speed", "pvp"}, "forge");
		QCOMPARE(projectsOf(mods), (QStringList{"shared", "forgeonly"}));
		/* The first bundle wins. */
		QCOMPARE(mods.first().toMap().value("bundle").toString(), QString("speed"));
		QCOMPARE(mods.last().toMap().value("bundle").toString(), QString("speed"));
	}

	void aMissedModDoesNotHideItsTwin()
	{
		/* The Forge-only copy is dropped for Fabric; it must not count as
		 * listed, or a later bundle's copy for every loader would vanish. */
		BundleCatalog catalog;
		QVERIFY(catalog.loadFromJson(R"({
			"formatVersion": 1, "presets": [],
			"bundles": [
				{ "id": "a", "mods": [ { "project": "x", "name": "X", "loaders": ["forge"] } ] },
				{ "id": "b", "mods": [ { "project": "x", "name": "X" } ] }
			] })"));
		const QVariantList mods = catalog.mods({"a", "b"}, "fabric");
		QCOMPARE(mods.size(), 1);
		QCOMPARE(mods.first().toMap().value("bundle").toString(), QString("b"));
	}

	void badJsonKeepsWhatWasLoaded()
	{
		BundleCatalog catalog;
		QVERIFY(catalog.loadFromJson(kCatalog));
		QSignalSpy changed(&catalog, &BundleCatalog::changed);

		QString error;
		QVERIFY(!catalog.loadFromJson("{ not json", &error));
		QVERIFY(!error.isEmpty());
		QVERIFY(!catalog.loadFromJson("[]", &error));
		QCOMPARE(changed.count(), 0);
		QCOMPARE(catalog.presets().size(), 1);
		QCOMPARE(catalog.presets().first().toMap().value("id").toString(),
				 QString("fast"));
	}

	void wrongFormatVersionIsRejected()
	{
		BundleCatalog catalog;
		QVERIFY(catalog.loadFromJson(kCatalog));

		QByteArray other = kCatalog;
		other.replace("\"formatVersion\": 1", "\"formatVersion\": 2");
		QString error;
		QVERIFY(!catalog.loadFromJson(other, &error));
		QVERIFY(!error.isEmpty());
		QCOMPARE(catalog.presets().size(), 1);
	}

	void invalidContentIsRejected_data()
	{
		QTest::addColumn<QByteArray>("json");
		QTest::newRow("preset names a missing bundle")
			<< QByteArray(R"({ "formatVersion": 1, "bundles": [],
				"presets": [ { "id": "p", "bundles": ["ghost"] } ] })");
		QTest::newRow("mod without a project")
			<< QByteArray(R"({ "formatVersion": 1, "presets": [],
				"bundles": [ { "id": "b", "mods": [ { "name": "No project" } ] } ] })");
		QTest::newRow("mod without a name")
			<< QByteArray(R"({ "formatVersion": 1, "presets": [],
				"bundles": [ { "id": "b", "mods": [ { "project": "p" } ] } ] })");
		QTest::newRow("no presets array")
			<< QByteArray(R"({ "formatVersion": 1, "bundles": [] })");
	}
	void invalidContentIsRejected()
	{
		QFETCH(QByteArray, json);
		BundleCatalog catalog;
		const int before = catalog.presets().size();
		QString error;
		QVERIFY(!catalog.loadFromJson(json, &error));
		QVERIFY(!error.isEmpty());
		QCOMPARE(catalog.presets().size(), before);
	}

	void theBuiltInCatalogIsValid()
	{
		/* The constructor loads :/kite/bundles.json. */
		BundleCatalog catalog;
		const QVariantList presets = catalog.presets();
		QVERIFY(!presets.isEmpty());
		for (const QVariant& value : presets) {
			const QVariantMap preset = value.toMap();
			const QStringList bundles = preset.value("bundles").toStringList();
			QVERIFY(!bundles.isEmpty());
			/* Every bundle a preset names has mods for its loader. */
			QVERIFY2(!catalog.mods(bundles, preset.value("loader").toString())
						  .isEmpty(),
					 qPrintable(preset.value("id").toString()));
			QVERIFY(!preset.value("name").toString().isEmpty());
		}
	}

	void theBuiltInCatalogHasNoDuplicateRowsPerLoader()
	{
		BundleCatalog catalog;
		for (const QString& loader : {"fabric", "forge", "neoforge", "quilt"}) {
			const QStringList projects = projectsOf(
				catalog.mods({"performance", "comfort"}, loader));
			QCOMPARE(projects.size(), QSet<QString>(projects.begin(), projects.end()).size());
		}
	}

	void theBuiltInCatalogRequiresWhatItLists()
	{
		/* Picking a mod picks what it requires too, so each of those has
		 * to be in the same list. */
		BundleCatalog catalog;
		for (const QString& loader : {"fabric", "forge", "neoforge", "quilt"}) {
			const QVariantList mods = catalog.mods({"performance", "comfort"}, loader);
			const QStringList projects = projectsOf(mods);
			for (const QVariant& mod : mods) {
				const QVariantMap fields = mod.toMap();
				for (const QString& required : fields.value("requires").toStringList()) {
					QVERIFY2(projects.contains(required),
							 qPrintable(fields.value("name").toString() + " / " + loader));
				}
			}
		}
	}
};

QTEST_GUILESS_MAIN(BundleCatalogTest)

#include "BundleCatalog_test.moc"
