/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 */

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "minecraft/mod/InstalledContentKeys.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/ContentProviderModel.h"
#include "modplatform/flame/FlameContentModel.h"
#include "modplatform/modrinth/ModrinthContentModel.h"

/* The result rows' roles and their "installed" flag, from canned search
 * replies (loadSearchReplyForTest()): nothing here asks a provider anything. */
class ContentProviderModelTest : public QObject
{
	Q_OBJECT

	static QByteArray modrinthReply()
	{
		return "{\"hits\":["
			   "{\"project_id\":\"P7dR8mSH\",\"slug\":\"fabric-api\","
			   "\"title\":\"Fabric API\",\"description\":\"Hooks.\","
			   "\"author\":\"modmuss50\",\"downloads\":123456789,"
			   "\"date_modified\":\"2026-10-01T10:00:00Z\","
			   "\"categories\":[\"fabric\",\"library\",\"quilt\"],"
			   "\"display_categories\":[\"library\"],"
			   "\"featured_gallery\":\"https://cdn.example/b.png\","
			   "\"gallery\":[\"https://cdn.example/a.png\","
			   "\"https://cdn.example/b.png\"]},"
			   "{\"project_id\":\"AANobbMI\",\"slug\":\"sodium\","
			   "\"title\":\"Sodium\",\"author\":\"jellysquid3\","
			   "\"categories\":[\"fabric\",\"optimization\",\"utility\","
			   "\"technology\",\"storage\"]},"
			   "{\"project_id\":\"gvQqBUqZ\",\"slug\":\"lithium\","
			   "\"title\":\"Lithium\"}"
			   "],\"total_hits\":3}";
	}

	static QVariant at(const ContentProviderModel& model, int row,
					   const QByteArray& role)
	{
		return model.data(model.index(row), model.roleNames().key(role));
	}

  private slots:

	void test_ModrinthRowRoles()
	{
		ModrinthContentModel model(ModPlatform::ContentType::Mod, {});
		model.loadSearchReplyForTest(modrinthReply());
		QCOMPARE(model.rowCount({}), 3);

		QCOMPARE(at(model, 0, "downloads").toDouble(), 123456789.0);
		QCOMPARE(at(model, 0, "updated").toString(),
				 QString("2026-10-01T10:00:00Z"));
		/* The curated list, not the one with the loaders in it. */
		QCOMPARE(at(model, 0, "categories").toStringList(),
				 QStringList{ "library" });
		/* The featured picture first, each once. */
		QCOMPARE(at(model, 0, "gallery").toStringList(),
				 (QStringList{ "https://cdn.example/b.png",
							   "https://cdn.example/a.png" }));
		QVERIFY(at(model, 0, "websiteUrl").toString().endsWith("fabric-api"));

		/* No curated list: the loaders come off and three are kept. */
		QCOMPARE(at(model, 1, "categories").toStringList(),
				 (QStringList{ "optimization", "utility", "technology" }));

		/* A hit that says none of it. */
		QCOMPARE(at(model, 2, "downloads").toDouble(), 0.0);
		QCOMPARE(at(model, 2, "updated").toString(), QString());
		QVERIFY(at(model, 2, "categories").toStringList().isEmpty());
		QVERIFY(at(model, 2, "gallery").toStringList().isEmpty());
	}

	void test_CurseForgeRowRoles()
	{
		FlameContentModel model(ModPlatform::ContentType::Mod, {});
		model.loadSearchReplyForTest(
			"{\"data\":[{\"id\":238222,\"slug\":\"jei\",\"name\":\"JEI\","
			"\"summary\":\"Items.\",\"downloadCount\":3.5e8,"
			"\"dateModified\":\"2026-09-09T00:00:00Z\","
			"\"categories\":[{\"name\":\"API and Library\"},"
			"{\"name\":\"Map and Information\"}],"
			"\"screenshots\":[{\"url\":\"https://media.example/1.png\"}],"
			"\"authors\":[{\"name\":\"mezz\"}]}],"
			"\"pagination\":{\"totalCount\":1}}");
		QCOMPARE(model.rowCount({}), 1);
		QCOMPARE(at(model, 0, "downloads").toDouble(), 350000000.0);
		QCOMPARE(at(model, 0, "updated").toString(),
				 QString("2026-09-09T00:00:00Z"));
		QCOMPARE(at(model, 0, "categories").toStringList(),
				 (QStringList{ "API and Library", "Map and Information" }));
		QCOMPARE(at(model, 0, "gallery").toStringList(),
				 QStringList{ "https://media.example/1.png" });
	}

	/* Jars dropped into mods/ by hand have no sidecar: the folder's keys flag
	 * them. Fabric API and Sodium are there, Lithium is not. */
	void test_InstalledWithoutSidecar()
	{
		ModrinthContentModel model(ModPlatform::ContentType::Mod, {});
		model.loadSearchReplyForTest(modrinthReply());
		QVERIFY(!at(model, 0, "installed").toBool());
		QVERIFY(!at(model, 1, "installed").toBool());

		QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
		model.setInstalledKeys(InstalledContentKeys::keysOf(
			QList<InstalledContentKeys::File>{
				{"fabric-api-0.110.0.jar", "", ""},
				{"sodium-fabric-0.6.5.jar", "sodium", "Sodium"},
			}));
		QCOMPARE(changed.count(), 1);
		QVERIFY(at(model, 0, "installed").toBool());
		QVERIFY(at(model, 1, "installed").toBool());
		QVERIFY(!at(model, 2, "installed").toBool());

		/* The same keys again report nothing. */
		model.refreshInstalledState();
		QCOMPARE(changed.count(), 1);

		model.setInstalledKeys({});
		QVERIFY(!at(model, 0, "installed").toBool());
	}

	/* The recorded project id wins over any name: a sidecar for Lithium under
	 * a file name that says nothing. */
	void test_InstalledByMetadataIndex()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QFile jar(QDir(dir.path()).filePath("renamed.jar"));
		QVERIFY(jar.open(QIODevice::WriteOnly));
		jar.close();

		auto index = std::make_shared<ModMetadataIndex>(QDir(dir.path()));
		ModMetadataIndex::Entry entry;
		entry.fileName = "renamed.jar";
		entry.platform = "modrinth";
		entry.projectId = "gvQqBUqZ";
		entry.name = "Something else";
		entry.slug = "lithium";
		QVERIFY(index->put(entry));

		ModrinthContentModel model(ModPlatform::ContentType::Mod, {});
		model.setInstalledIndex(index);
		model.loadSearchReplyForTest(modrinthReply());
		QVERIFY(at(model, 2, "installed").toBool());
		QVERIFY(!at(model, 1, "installed").toBool());

		/* Hidden when asked, and back once the sidecar is gone. */
		model.setHideInstalled(true);
		QCOMPARE(model.rowCount({}), 2);
		QVERIFY(index->remove("renamed.jar"));
		model.refreshInstalledState();
		QCOMPARE(model.rowCount({}), 3);
	}
};

QTEST_GUILESS_MAIN(ContentProviderModelTest)

#include "ContentProviderModel_test.moc"
