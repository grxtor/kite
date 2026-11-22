/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 */

#include <QTest>

#include "minecraft/mod/InstalledContentKeys.h"

/* What a folder holds without sidecars, matched against a provider's slug and
 * name. Strings only: no files, no network. */
class InstalledContentKeysTest : public QObject
{
	Q_OBJECT

  private slots:

	void test_Key()
	{
		QCOMPARE(InstalledContentKeys::key("fabric-api"), QString("fabricapi"));
		QCOMPARE(InstalledContentKeys::key("Fabric API"), QString("fabricapi"));
		QCOMPARE(InstalledContentKeys::key("fabric_api"), QString("fabricapi"));
		QCOMPARE(InstalledContentKeys::key(""), QString());
	}

	void test_FileKey_data()
	{
		QTest::addColumn<QString>("fileName");
		QTest::addColumn<QString>("expected");

		QTest::newRow("plain") << "sodium-0.6.5.jar" << "sodium";
		QTest::newRow("two words") << "fabric-api-0.110.0+1.21.4.jar"
								   << "fabricapi";
		QTest::newRow("loader word") << "sodium-fabric-0.6.5+mc1.21.jar"
									 << "sodiumfabric";
		QTest::newRow("disabled") << "entityculling-1.7.2.jar.disabled"
								  << "entityculling";
		QTest::newRow("underscores")
			<< "Xaeros_Minimap_24.5.0_Fabric_1.21.jar" << "xaerosminimap";
		QTest::newRow("version in front")
			<< "[1.20.1] SecurityCraft v1.9.8.jar" << "securitycraft";
		QTest::newRow("mc version then loader") << "jei-1.20.1-forge-15.2.jar"
												<< "jei";
		QTest::newRow("resource pack") << "ComplementaryReimagined_r5.3.zip"
									   << "complementaryreimagined";
		QTest::newRow("only a version") << "1.20.1.jar" << "";
	}

	void test_FileKey()
	{
		QFETCH(QString, fileName);
		QFETCH(QString, expected);
		QCOMPARE(InstalledContentKeys::fileKey(fileName), expected);
	}

	/* A jar nobody recorded: its file name is all there is until it is read. */
	void test_MatchesByFileName()
	{
		const auto keys = InstalledContentKeys::keysOf(
			QList<InstalledContentKeys::File>{
				{"fabric-api-0.110.0.jar", "", ""},
				{"sodium-fabric-0.6.5+mc1.21.jar", "", ""},
			});
		QVERIFY(InstalledContentKeys::contains(keys, "fabric-api",
											   "Fabric API"));
		/* The trailing loader word comes off: Modrinth's slug is "sodium". */
		QVERIFY(InstalledContentKeys::contains(keys, "sodium", "Sodium"));
		QVERIFY(!InstalledContentKeys::contains(keys, "lithium", "Lithium"));
		/* A leading "fabric" is part of the name, not a loader marker. */
		QVERIFY(!InstalledContentKeys::contains(keys, "api", "API"));
	}

	/* A renamed jar still answers by the id and name in its own metadata. */
	void test_MatchesByModIdAndName()
	{
		const auto keys = InstalledContentKeys::keysOf(
			QList<InstalledContentKeys::File>{
				{"renamed.jar", "modmenu", "Mod Menu"},
				{"x.jar", "iris", "Iris"},
				{"y.jar", "", "Entity Culling (Fabric)"},
			});
		QVERIFY(InstalledContentKeys::contains(keys, "modmenu", "Mod Menu"));
		/* Slug differs from the id on neither side here; the name alone would
		 * not match "Iris Shaders". */
		QVERIFY(InstalledContentKeys::contains(keys, "iris", "Iris Shaders"));
		QVERIFY(InstalledContentKeys::contains(keys, "entityculling",
											   "Entity Culling"));
		QVERIFY(!InstalledContentKeys::contains(keys, "sodium", "Sodium"));
	}

	void test_ShortAndEmptyKeysNeverMatch()
	{
		const auto keys = InstalledContentKeys::keysOf(
			QList<InstalledContentKeys::File>{ {"a-1.0.jar", "b", "c"} });
		QVERIFY(keys.isEmpty());
		QVERIFY(!InstalledContentKeys::contains(keys, "a", "a"));
		QVERIFY(!InstalledContentKeys::contains({}, "sodium", "Sodium"));
		QVERIFY(!InstalledContentKeys::contains({ "sodium" }, "", ""));
	}
};

QTEST_GUILESS_MAIN(InstalledContentKeysTest)

#include "InstalledContentKeys_test.moc"
