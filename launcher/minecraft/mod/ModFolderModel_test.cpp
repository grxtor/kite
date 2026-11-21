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
#include <QSignalSpy>
#include <QTemporaryDir>
#include "TestUtil.h"

#include "FileSystem.h"
#include "minecraft/mod/ModFolderModel.h"

class ModFolderModelTest : public QObject
{
	Q_OBJECT

  private slots:
	// test for GH-1178 - install a folder with files to a mod list
	void test_1178()
	{
		// source
		QString source = QFINDTESTDATA("data/test_folder");

		// sanity check
		QVERIFY(!source.endsWith('/'));

		auto verify = [](QString path) {
			QDir target_dir(FS::PathCombine(path, "test_folder"));
			QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
			QVERIFY(target_dir.entryList().contains("assets"));
		};

		// 1. test with no trailing /
		{
			QString folder = source;
			QTemporaryDir tempDir;
			ModFolderModel m(tempDir.path());
			m.installMod(folder);
			verify(tempDir.path());
		}

		// 2. test with trailing /
		{
			QString folder = source + '/';
			QTemporaryDir tempDir;
			ModFolderModel m(tempDir.path());
			m.installMod(folder);
			verify(tempDir.path());
		}
	}

	// Removed mods go to the trash with their sidecars, and come back as they
	// were: the enabled one enabled, the disabled one still ".disabled".
	void test_deleteGoesToTrashAndComesBack()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		// Never the real trash: a folder of the test's own stands in.
		FS::setTrashFolderForTests(tempDir.filePath("trash"));
		const QString mods = tempDir.filePath("mods");
		QVERIFY(QDir().mkpath(mods));
		const QString enabled = FS::PathCombine(mods, "sodium.jar");
		const QString disabled = FS::PathCombine(mods, "lithium.jar.disabled");
		const QString kept = FS::PathCombine(mods, "kept.jar");
		for (const QString& path : {enabled, disabled, kept}) {
			QFile file(path);
			QVERIFY(file.open(QIODevice::WriteOnly));
			file.write("not a real jar");
		}

		ModFolderModel model(mods);
		ModMetadataIndex::Entry entry;
		entry.fileName = "sodium.jar";
		entry.platform = "modrinth";
		entry.projectId = "AANobbMI";
		entry.versionId = "abc123";
		entry.name = "Sodium";
		entry.slug = "sodium";
		QVERIFY(model.metadataIndex()->put(entry));
		const QString sidecar =
			model.metadataIndex()->sidecarPathFor("sodium.jar");
		QVERIFY(QFileInfo::exists(sidecar));
		QFile sidecarFile(sidecar);
		QVERIFY(sidecarFile.open(QIODevice::ReadOnly));
		const QByteArray sidecarBytes = sidecarFile.readAll();
		sidecarFile.close();

		QSignalSpy loaded(&model, &ModFolderModel::updateFinished);
		QVERIFY(model.update());
		QVERIFY(loaded.wait());
		QCOMPARE(model.size(), size_t(3));

		QModelIndexList doomed;
		for (int row = 0; row < int(model.size()); ++row) {
			if (model.at(size_t(row)).filename().fileName() != "kept.jar") {
				doomed.append(model.index(row, 0));
			}
		}
		QCOMPARE(doomed.size(), 2);

		QList<FS::Trashed> trashed;
		QVERIFY(model.deleteMods(doomed, &trashed));
		QVERIFY(!QFileInfo::exists(enabled));
		QVERIFY(!QFileInfo::exists(disabled));
		QVERIFY(!QFileInfo::exists(sidecar));
		QVERIFY(QFileInfo::exists(kept));
		// Two mods and the one sidecar.
		QCOMPARE(trashed.size(), 3);
		QVERIFY(!model.metadataIndex()->contains("sodium.jar"));

		for (const FS::Trashed& item : std::as_const(trashed)) {
			QVERIFY(FS::restoreTrashed(item));
		}
		QVERIFY(QFileInfo::exists(enabled));
		QVERIFY(QFileInfo::exists(disabled));
		QVERIFY(!QFileInfo::exists(FS::PathCombine(mods, "lithium.jar")));
		QVERIFY(sidecarFile.open(QIODevice::ReadOnly));
		QCOMPARE(sidecarFile.readAll(), sidecarBytes);
		sidecarFile.close();

		model.metadataIndex()->load();
		const auto restored = model.metadataIndex()->get("sodium.jar");
		QCOMPARE(restored.platform, QString("modrinth"));
		QCOMPARE(restored.projectId, QString("AANobbMI"));

		QVERIFY(model.update());
		QVERIFY(loaded.wait());
		QCOMPARE(model.size(), size_t(3));
		for (const Mod& mod : model.allMods()) {
			QCOMPARE(mod.enabled(),
					 mod.filename().fileName() != "lithium.jar.disabled");
		}

		FS::setTrashFolderForTests(QString());
	}
};

QTEST_GUILESS_MAIN(ModFolderModelTest)

#include "ModFolderModel_test.moc"
