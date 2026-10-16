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

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "MMCZip.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "models/ContentUpdateController.h"

/*
 * What can be tested without a MinecraftInstance or the network: the
 * update list the review dialog shows, the key filter a selection-only
 * check goes through, which recorded files the dependency check asks
 * about, and forgetting origins on a real folder. The checks, version
 * lookups and downloads themselves all ask a provider.
 */
class ContentUpdateControllerTest : public QObject
{
	Q_OBJECT

  private:
	ModUpdateCheckTask::UpdateInfo update(const QString& name,
										  const QString& file,
										  const QString& platform) const
	{
		ModUpdateCheckTask::UpdateInfo u;
		u.name = name;
		u.currentFileName = file;
		u.platform = platform;
		u.newVersionId = "new-" + name;
		u.item.projectId = "proj-" + name;
		u.item.fileName = name.toLower() + "-2.jar";
		u.item.name = name;
		return u;
	}

  private slots:
	void listSortsTicksAndHandsOutItems()
	{
		ContentUpdateList list;
		QSignalSpy counted(&list, &ContentUpdateList::countChanged);
		auto sodium = update("Sodium", "sodium.jar", "modrinth");
		sodium.currentVersionNumber = "0.5.0";
		sodium.newVersionNumber = "0.6.5";
		list.setUpdates({sodium, update("Lithium", "lithium.jar", "curseforge")});

		QCOMPARE(counted.count(), 1);
		QCOMPARE(list.count(), 2);
		QCOMPARE(list.checkedCount(), 2);
		/* By name. */
		QCOMPARE(list.index(0).data(ContentUpdateList::NameRole).toString(),
				 QStringLiteral("Lithium"));
		const QModelIndex row = list.index(1);
		QCOMPARE(row.data(ContentUpdateList::CurrentVersionRole).toString(),
				 QStringLiteral("0.5.0"));
		QCOMPARE(row.data(ContentUpdateList::NewVersionRole).toString(),
				 QStringLiteral("0.6.5"));
		QCOMPARE(row.data(ContentUpdateList::ProviderRole).toString(),
				 QStringLiteral("Modrinth"));
		/* No readable numbers: the file names stand in. */
		QCOMPARE(list.index(0).data(ContentUpdateList::NewVersionRole).toString(),
				 QStringLiteral("lithium-2.jar"));

		list.setChecked(0, false);
		QCOMPARE(list.checkedCount(), 1);
		QCOMPARE(list.checkedItems().size(), 1);
		QCOMPARE(list.checkedItems().first().fileName, QStringLiteral("sodium-2.jar"));

		list.setAllChecked(true);
		QCOMPARE(list.checkedItems().size(), 2);
		list.clear();
		QCOMPARE(list.count(), 0);
	}

	void changelogLinks()
	{
		const auto modrinth = update("Sodium", "sodium.jar", "modrinth");
		QCOMPARE(ContentUpdateList::changelogUrl(modrinth),
				 QStringLiteral("https://modrinth.com/mod/proj-Sodium/version/new-Sodium"));
		const auto curseforge = update("JEI", "jei.jar", "curseforge");
		QVERIFY(ContentUpdateList::changelogUrl(curseforge).contains("proj-JEI"));
		QVERIFY(ContentUpdateList::changelogUrl(update("X", "x.jar", "local"))
					.isEmpty());
	}

	void onlyForKeepsTheAskedFiles()
	{
		const QList<ModUpdateCheckTask::UpdateInfo> all = {
			update("Sodium", "sodium.jar", "modrinth"),
			update("Lithium", "lithium.jar.disabled", "modrinth"),
		};
		QCOMPARE(ContentUpdateController::onlyFor(all, {}).size(), 2);
		const auto picked =
			ContentUpdateController::onlyFor(all, {"lithium.jar"});
		QCOMPARE(picked.size(), 1);
		QCOMPARE(picked.first().name, QStringLiteral("Lithium"));
		QVERIFY(ContentUpdateController::onlyFor(all, {"none.jar"}).isEmpty());
	}

	void resolvableEntriesNeedAnOriginAndAnId()
	{
		ModMetadataIndex::Entry tracked;
		tracked.fileName = "a.jar";
		tracked.platform = "modrinth";
		tracked.projectId = "A";
		tracked.versionId = "v1";
		ModMetadataIndex::Entry byHash = tracked;
		byHash.fileName = "b.jar";
		byHash.versionId.clear();
		byHash.sha1 = "abc";
		ModMetadataIndex::Entry local;
		local.fileName = "c.jar";
		local.platform = "local";
		ModMetadataIndex::Entry noId = tracked;
		noId.fileName = "d.jar";
		noId.versionId.clear();

		const auto mods = ContentUpdateController::resolvableEntries(
			{tracked, byHash, local, noId}, "1.21.4");
		QCOMPARE(mods.size(), 2);
		QCOMPARE(mods.at(0).fileName, QStringLiteral("a.jar"));
		QCOMPARE(mods.at(1).sha1, QStringLiteral("abc"));
		QCOMPARE(mods.at(0).mcVersion, QStringLiteral("1.21.4"));
	}

	void contentTypes()
	{
		ModPlatform::ContentType type;
		QVERIFY(ContentUpdateController::contentTypeFor("shaderpacks", type));
		QCOMPARE(type, ModPlatform::ContentType::ShaderPack);
		QVERIFY(!ContentUpdateController::contentTypeFor("coremods", type));
		QVERIFY(!ContentUpdateController::contentTypeFor("texturepacks", type));
	}

	void currentVersionsComeFromTheFiles()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString jar = dir.filePath("lithium.jar");
		{
			MMCZip::ZipWriter zip(jar);
			QVERIFY(zip.open());
			const QByteArray json =
				"{\"schemaVersion\":1,\"id\":\"lithium\",\"name\":\"Lithium\","
				"\"version\":\"0.14.3\"}";
			QVERIFY(zip.addFile("fabric.mod.json", json));
			QVERIFY(zip.close());
		}
		ModFolderModel folder(dir.path());
		QSignalSpy finished(&folder, &ModFolderModel::updateFinished);
		QVERIFY(folder.update());
		QVERIFY(finished.wait(5000));
		QTRY_COMPARE_WITH_TIMEOUT(folder.at(0).version(), QStringLiteral("0.14.3"), 5000);

		auto known = update("Sodium", "sodium.jar", "modrinth");
		known.currentVersionNumber = "0.5.0";
		QList<ModUpdateCheckTask::UpdateInfo> updates = {
			update("Lithium", "lithium.jar", "modrinth"), known};
		ContentUpdateController::fillCurrentVersions(updates, folder);
		QCOMPARE(updates.at(0).currentVersionNumber, QStringLiteral("0.14.3"));
		/* A recorded number is left as it is. */
		QCOMPARE(updates.at(1).currentVersionNumber, QStringLiteral("0.5.0"));
	}

	void forgetOriginsRemovesOnlyTheAskedSidecars()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		for (const char* name : {"a.jar", "b.jar"}) {
			QFile file(dir.filePath(name));
			QVERIFY(file.open(QIODevice::WriteOnly));
			file.write("x");
		}
		ModFolderModel folder(dir.path());
		QSignalSpy finished(&folder, &ModFolderModel::updateFinished);
		QVERIFY(folder.update());
		QVERIFY(finished.wait(5000));
		QCOMPARE(folder.size(), size_t(2));

		auto index = folder.metadataIndex();
		for (const char* name : {"a.jar", "b.jar"}) {
			ModMetadataIndex::Entry entry;
			entry.fileName = name;
			entry.platform = "modrinth";
			entry.projectId = name;
			QVERIFY(index->put(entry));
		}

		QCOMPARE(ContentUpdateController::forgetOrigins(folder, {"a.jar"}), 1);
		QVERIFY(!index->contains("a.jar"));
		QVERIFY(index->contains("b.jar"));
		/* Nothing recorded, nothing asked: nothing removed. */
		QCOMPARE(ContentUpdateController::forgetOrigins(folder, {"a.jar"}), 0);
		QCOMPARE(ContentUpdateController::forgetOrigins(folder, {}), 0);
		/* The files themselves stay. */
		QVERIFY(QFile::exists(dir.filePath("a.jar")));
	}
};

QTEST_GUILESS_MAIN(ContentUpdateControllerTest)

#include "ContentUpdateController_test.moc"
