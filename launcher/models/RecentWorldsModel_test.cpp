/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "models/RecentWorldsModel.h"

#include "GZip.h"
#include "InstanceList.h"
#include "NullInstance.h"
#include "TestSettings.h"

/* Exercises RecentWorldsModel::scan() directly against a temporary directory (a
 * real InstanceList of MinecraftInstances would need a full instance setup).
 * The tests near the bottom build an InstanceList with the NullInstance fixture
 * of InstanceList_test.cpp to drive the rescan triggers and debounce. */
namespace
{
	using Entry = RecentWorldsModel::Entry;
	using Source = RecentWorldsModel::InstanceWorldSource;

	/* Minimal big-endian NBT writer: just enough level.dat for World::isValid() and
	 * a LastPlayed value. A reduced copy of the writer in minecraft/World_test.cpp. */
	const quint8 TAG_END = 0;
	const quint8 TAG_LONG = 4;
	const quint8 TAG_COMPOUND = 10;

	void putU8(QByteArray& out, quint8 value)
	{
		out.append(static_cast<char>(value));
	}

	void putU16(QByteArray& out, quint16 value)
	{
		out.append(static_cast<char>((value >> 8) & 0xFF));
		out.append(static_cast<char>(value & 0xFF));
	}

	void putI64(QByteArray& out, qint64 value)
	{
		for (int shift = 56; shift >= 0; shift -= 8) {
			out.append(static_cast<char>((value >> shift) & 0xFF));
		}
	}

	void putString(QByteArray& out, const QByteArray& value)
	{
		putU16(out, static_cast<quint16>(value.size()));
		out.append(value);
	}

	void putTagHeader(QByteArray& out, quint8 type, const QByteArray& name)
	{
		putU8(out, type);
		putString(out, name);
	}

	void putLongTag(QByteArray& out, const QByteArray& name, qint64 value)
	{
		putTagHeader(out, TAG_LONG, name);
		putI64(out, value);
	}

	QByteArray makeLevelDat(qint64 lastPlayedMs)
	{
		QByteArray dataPayload;
		putLongTag(dataPayload, "LastPlayed", lastPlayedMs);
		putU8(dataPayload, TAG_END);

		QByteArray root;
		putTagHeader(root, TAG_COMPOUND, ""); // unnamed root compound
		putTagHeader(root, TAG_COMPOUND, "Data");
		root.append(dataPayload);
		putU8(root, TAG_END);
		return root;
	}

	bool writeWorld(const QString& worldPath, qint64 lastPlayedMs)
	{
		if (!QDir().mkpath(worldPath)) {
			return false;
		}
		QByteArray compressed;
		if (!GZip::zip(makeLevelDat(lastPlayedMs), compressed)) {
			return false;
		}
		QFile file(QDir(worldPath).filePath("level.dat"));
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		return file.write(compressed) == compressed.size();
	}

	Source makeSource(const QString& worldsDir,
					   const QString& instanceId = "inst")
	{
		Source source;
		source.instanceId = instanceId;
		source.instanceName = instanceId;
		source.instanceIconKey = "default";
		source.worldsDir = worldsDir;
		return source;
	}

	/// Writes an instance.cfg with an unknown InstanceType, so loadInstance() falls
	/// back to NullInstance (enough for InstanceList's row signals).
	bool writeNullInstanceCfg(const QString& instRoot)
	{
		if (!QDir().mkpath(instRoot)) {
			return false;
		}
		QFile file(QDir(instRoot).filePath("instance.cfg"));
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		return file.write("InstanceType=NullTest\n") > 0;
	}
} // namespace

class RecentWorldsModelTest : public QObject
{
	Q_OBJECT
  private slots:

	void ordersByLastPlayedNewestFirst()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString savesDir = QDir(tempDir.path()).filePath("saves");

		QVERIFY(writeWorld(QDir(savesDir).filePath("old"), 1000));
		QVERIFY(writeWorld(QDir(savesDir).filePath("newest"), 3000));
		QVERIFY(writeWorld(QDir(savesDir).filePath("middle"), 2000));

		const QList<Entry> result =
			RecentWorldsModel::scan({makeSource(savesDir)}, 8);

		QCOMPARE(result.size(), 3);
		QCOMPARE(result.at(0).folderName, QString("newest"));
		QCOMPARE(result.at(0).lastPlayed, Q_INT64_C(3000));
		QCOMPARE(result.at(1).folderName, QString("middle"));
		QCOMPARE(result.at(2).folderName, QString("old"));
	}

	void ordersAcrossMultipleInstances()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString savesA = QDir(tempDir.path()).filePath("a/saves");
		const QString savesB = QDir(tempDir.path()).filePath("b/saves");

		QVERIFY(writeWorld(QDir(savesA).filePath("world"), 5000));
		QVERIFY(writeWorld(QDir(savesB).filePath("world"), 9000));

		const QList<Entry> result = RecentWorldsModel::scan(
			{makeSource(savesA, "instA"), makeSource(savesB, "instB")}, 8);

		QCOMPARE(result.size(), 2);
		QCOMPARE(result.at(0).instanceId, QString("instB"));
		QCOMPARE(result.at(1).instanceId, QString("instA"));
	}

	void capsAtMaxCount()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString savesDir = QDir(tempDir.path()).filePath("saves");

		for (int i = 0; i < 5; ++i) {
			QVERIFY(writeWorld(
				QDir(savesDir).filePath(QString("world%1").arg(i)),
				1000 * (i + 1)));
		}

		const QList<Entry> result =
			RecentWorldsModel::scan({makeSource(savesDir)}, 3);

		QCOMPARE(result.size(), 3);
		QCOMPARE(result.at(0).lastPlayed, Q_INT64_C(5000));
		QCOMPARE(result.at(1).lastPlayed, Q_INT64_C(4000));
		QCOMPARE(result.at(2).lastPlayed, Q_INT64_C(3000));
	}

	void skipsFoldersWithoutLevelDat()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString savesDir = QDir(tempDir.path()).filePath("saves");

		QVERIFY(writeWorld(QDir(savesDir).filePath("real"), 1000));
		QVERIFY(QDir().mkpath(QDir(savesDir).filePath("empty-folder")));

		const QList<Entry> result =
			RecentWorldsModel::scan({makeSource(savesDir)}, 8);

		QCOMPARE(result.size(), 1);
		QCOMPARE(result.at(0).folderName, QString("real"));
	}

	void ignoresFilesNextToWorldFolders()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString savesDir = QDir(tempDir.path()).filePath("saves");

		QVERIFY(writeWorld(QDir(savesDir).filePath("real"), 1000));
		QFile stray(QDir(savesDir).filePath("session.lock"));
		QVERIFY(stray.open(QIODevice::WriteOnly));
		stray.write("x");
		stray.close();

		const QList<Entry> result =
			RecentWorldsModel::scan({makeSource(savesDir)}, 8);

		QCOMPARE(result.size(), 1);
	}

	void reportsWorldIconUrlWhenPresent()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString savesDir = QDir(tempDir.path()).filePath("saves");
		const QString worldPath = QDir(savesDir).filePath("iconworld");

		QVERIFY(writeWorld(worldPath, 1000));
		QFile icon(QDir(worldPath).filePath("icon.png"));
		QVERIFY(icon.open(QIODevice::WriteOnly));
		icon.write("not a real png, world only reads the path");
		icon.close();

		const QList<Entry> result =
			RecentWorldsModel::scan({makeSource(savesDir)}, 8);

		QCOMPARE(result.size(), 1);
		QCOMPARE(result.at(0).iconUrl,
				 QUrl::fromLocalFile(QDir(worldPath).filePath("icon.png"))
					 .toString());
	}

	void emptyIconUrlWithoutIcon()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString savesDir = QDir(tempDir.path()).filePath("saves");

		QVERIFY(writeWorld(QDir(savesDir).filePath("noicon"), 1000));

		const QList<Entry> result =
			RecentWorldsModel::scan({makeSource(savesDir)}, 8);

		QCOMPARE(result.size(), 1);
		QCOMPARE(result.at(0).iconUrl, QString());
	}

	void missingOrEmptyWorldsDirIsNotAnError()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());

		const QList<Entry> missing = RecentWorldsModel::scan(
			{makeSource(QDir(tempDir.path()).filePath("does-not-exist"))}, 8);
		QVERIFY(missing.isEmpty());

		const QList<Entry> empty =
			RecentWorldsModel::scan({makeSource(QString())}, 8);
		QVERIFY(empty.isEmpty());

		QCOMPARE(RecentWorldsModel::scan({}, 8).size(), 0);
	}

	void carriesInstanceMetadataThrough()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString savesDir = QDir(tempDir.path()).filePath("saves");
		QVERIFY(writeWorld(QDir(savesDir).filePath("world"), 1000));

		Source source = makeSource(savesDir, "the-instance");
		source.instanceName = "The Instance";
		source.instanceIconKey = "grass";

		const QList<Entry> result = RecentWorldsModel::scan({source}, 8);

		QCOMPARE(result.size(), 1);
		QCOMPARE(result.at(0).instanceId, QString("the-instance"));
		QCOMPARE(result.at(0).instanceName, QString("The Instance"));
		QCOMPARE(result.at(0).instanceIconKey, QString("grass"));
	}

	/// A null InstanceList is a valid, empty model.
	void nullInstanceListIsAnEmptyModel()
	{
		RecentWorldsModel model(nullptr);
		QCOMPARE(model.rowCount(), 0);

		QSignalSpy resetSpy(&model, &RecentWorldsModel::scanFinished);
		model.refresh();
		QVERIFY(resetSpy.wait());
		QCOMPARE(model.rowCount(), 0);
	}

	void roleNamesMatchExpectedRoles()
	{
		RecentWorldsModel model(nullptr);
		const QHash<int, QByteArray> roles = model.roleNames();
		QCOMPARE(roles.value(RecentWorldsModel::WorldNameRole),
				 QByteArray("worldName"));
		QCOMPARE(roles.value(RecentWorldsModel::FolderNameRole),
				 QByteArray("folderName"));
		QCOMPARE(roles.value(RecentWorldsModel::IconUrlRole),
				 QByteArray("iconUrl"));
		QCOMPARE(roles.value(RecentWorldsModel::LastPlayedRole),
				 QByteArray("lastPlayed"));
		QCOMPARE(roles.value(RecentWorldsModel::InstanceIdRole),
				 QByteArray("instanceId"));
		QCOMPARE(roles.value(RecentWorldsModel::InstanceNameRole),
				 QByteArray("instanceName"));
		QCOMPARE(roles.value(RecentWorldsModel::InstanceIconKeyRole),
				 QByteArray("instanceIconKey"));
	}

	/* The tests below drive a real InstanceList (NullInstance fixture) to cover
	 * scheduleRescan()'s triggers and coalescing. startScan() always hands scan() an
	 * empty source list here; the model reset each rescan produces is still observable. */

	void rowsInsertedTriggersRescan()
	{
		QTemporaryDir globalDir;
		QVERIFY(globalDir.isValid());
		QTemporaryDir instsDir;
		QVERIFY(instsDir.isValid());
		SettingsObjectPtr globalSettings = makeGlobalSettings(globalDir);

		InstanceList list(globalSettings, {instsDir.path()});
		QCOMPARE(list.loadList(), InstanceList::NoError);

		RecentWorldsModel model(&list);
		QSignalSpy resetSpy(&model, &RecentWorldsModel::scanFinished);
		QVERIFY(resetSpy.wait());
		resetSpy.clear();

		// InstanceList::add() emits a genuine rowsInserted, which scheduleRescan() is wired to.
		const QString instRoot = QDir(instsDir.path()).filePath("newinst");
		QVERIFY(writeNullInstanceCfg(instRoot));
		QCOMPARE(list.loadList(), InstanceList::NoError);

		QVERIFY(resetSpy.wait());
	}

	void stoppingARunningInstanceTriggersRescan()
	{
		QTemporaryDir globalDir;
		QVERIFY(globalDir.isValid());
		QTemporaryDir instsDir;
		QVERIFY(instsDir.isValid());
		SettingsObjectPtr globalSettings = makeGlobalSettings(globalDir);

		const QString instRoot = QDir(instsDir.path()).filePath("testinst");
		QVERIFY(writeNullInstanceCfg(instRoot));

		InstanceList list(globalSettings, {instsDir.path()});
		QCOMPARE(list.loadList(), InstanceList::NoError);
		InstancePtr inst = list.getInstanceById("testinst");
		QVERIFY(inst);

		RecentWorldsModel model(&list);
		QSignalSpy resetSpy(&model, &RecentWorldsModel::scanFinished);
		QVERIFY(resetSpy.wait());
		resetSpy.clear();

		// Starting a run alone must not schedule a rescan, only a stop.
		inst->setRunning(true);
		QVERIFY(!resetSpy.wait(500));

		inst->setRunning(false);
		QVERIFY(resetSpy.wait());
	}

	void rescanOfAnUnchangedListDoesNotResetTheModel()
	{
		QTemporaryDir globalDir;
		QVERIFY(globalDir.isValid());
		QTemporaryDir instsDir;
		QVERIFY(instsDir.isValid());
		SettingsObjectPtr globalSettings = makeGlobalSettings(globalDir);

		const QString instRoot = QDir(instsDir.path()).filePath("testinst");
		QVERIFY(writeNullInstanceCfg(instRoot));
		InstanceList list(globalSettings, {instsDir.path()});
		QCOMPARE(list.loadList(), InstanceList::NoError);

		RecentWorldsModel model(&list);
		QSignalSpy scanSpy(&model, &RecentWorldsModel::scanFinished);
		QVERIFY(scanSpy.wait());
		scanSpy.clear();

		// Nothing changed on disk: a view's scroll position must survive the rescan.
		QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
		model.refresh();
		QVERIFY(scanSpy.wait());
		QCOMPARE(resetSpy.count(), 0);
	}

	void unrelatedPropertyChangeDoesNotTriggerRescan()
	{
		QTemporaryDir globalDir;
		QVERIFY(globalDir.isValid());
		QTemporaryDir instsDir;
		QVERIFY(instsDir.isValid());
		SettingsObjectPtr globalSettings = makeGlobalSettings(globalDir);

		const QString instRoot = QDir(instsDir.path()).filePath("testinst");
		QVERIFY(writeNullInstanceCfg(instRoot));

		InstanceList list(globalSettings, {instsDir.path()});
		QCOMPARE(list.loadList(), InstanceList::NoError);
		InstancePtr inst = list.getInstanceById("testinst");
		QVERIFY(inst);

		RecentWorldsModel model(&list);
		QSignalSpy resetSpy(&model, &RecentWorldsModel::scanFinished);
		QVERIFY(resetSpy.wait());
		resetSpy.clear();

		// Rename, icon change and hasCrashed go through propertiesChanged() (dataChanged
		// with no role list), so none must schedule a rescan.
		inst->setName("Renamed");
		inst->setIconKey("grass");
		inst->setCrashed(true);
		QVERIFY(!resetSpy.wait(500));

		// Sanity check: a real running -> stopped transition still schedules one.
		inst->setRunning(true);
		inst->setRunning(false);
		QVERIFY(resetSpy.wait());
	}
};

QTEST_GUILESS_MAIN(RecentWorldsModelTest)

#include "RecentWorldsModel_test.moc"
