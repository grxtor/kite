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

#include "models/ServersListModel.h"

/* Needs neither a BaseInstance nor a LauncherContext - only a directory to
 * read/write servers.dat under, same reason OtherLogsModel_test.cpp drives
 * that model directly against a temp directory instead of a real
 * instance. */
class ServersListModelTest : public QObject
{
	Q_OBJECT

  private slots:
	void addEditMoveRemove();
	void persistsAcrossReload();
	void lockedRefusesEdits();
	void watchesDirectoryOnlyWhileLocked();
	void addressOfOutOfRange();
};

void ServersListModelTest::addEditMoveRemove()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	ServersListModel model(dir.path());
	model.startWatching();
	QCOMPARE(model.rowCount(), 0);

	const int first = model.addServer();
	QCOMPARE(first, 0);
	QCOMPARE(model.rowCount(), 1);
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Minecraft Server"));

	model.setName(0, QStringLiteral("Home"));
	model.setAddress(0, QStringLiteral("home.example.com:25565"));
	model.setAcceptTextures(0, 1);

	const int second = model.addServer();
	QCOMPARE(second, 1);
	model.setName(1, QStringLiteral("Away"));
	model.setAddress(1, QStringLiteral("away.example.com"));

	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Home"));
	QCOMPARE(
		model.data(model.index(0), ServersListModel::AddressRole).toString(),
		QStringLiteral("home.example.com:25565"));
	QCOMPARE(
		model.data(model.index(0), ServersListModel::AcceptTexturesRole).toInt(),
		1);

	// Move "Away" (row 1) up in front of "Home".
	QVERIFY(model.moveUp(1));
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Away"));
	QCOMPARE(model.data(model.index(1), ServersListModel::NameRole).toString(),
			QStringLiteral("Home"));

	QCOMPARE(model.addressOf(1), QStringLiteral("home.example.com:25565"));

	QVERIFY(model.removeServer(0));
	QCOMPARE(model.rowCount(), 1);
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Home"));
}

void ServersListModelTest::persistsAcrossReload()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	{
		ServersListModel model(dir.path());
		model.startWatching();
		model.addServer();
		model.setName(0, QStringLiteral("Persisted"));
		model.setAddress(0, QStringLiteral("persisted.example.com:1234"));
		model.setAcceptTextures(0, 2);
		model.stopWatching();
		// stopWatching() flushes the pending save synchronously - the
		// destructor below would too, but this checks that specifically.
	}

	QVERIFY(QFile::exists(dir.filePath(QStringLiteral("servers.dat"))));

	ServersListModel reloaded(dir.path());
	reloaded.startWatching();
	QCOMPARE(reloaded.rowCount(), 1);
	QCOMPARE(
		reloaded.data(reloaded.index(0), ServersListModel::NameRole).toString(),
		QStringLiteral("Persisted"));
	QCOMPARE(reloaded.data(reloaded.index(0), ServersListModel::AddressRole)
				.toString(),
			QStringLiteral("persisted.example.com:1234"));
	QCOMPARE(reloaded
				.data(reloaded.index(0),
					 ServersListModel::AcceptTexturesRole)
				.toInt(),
			2);
}

void ServersListModelTest::lockedRefusesEdits()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	ServersListModel model(dir.path());
	model.startWatching();
	model.addServer();
	model.setName(0, QStringLiteral("Original"));

	QSignalSpy lockedSpy(&model, &ServersListModel::lockedChanged);
	model.setLocked(true);
	QCOMPARE(lockedSpy.count(), 1);
	QVERIFY(model.locked());

	// Every mutation is a no-op while locked.
	QCOMPARE(model.addServer(), -1);
	model.setName(0, QStringLiteral("Changed"));
	model.setAddress(0, QStringLiteral("changed.example.com"));
	QVERIFY(!model.removeServer(0));
	QCOMPARE(model.rowCount(), 1);
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Original"));

	model.setLocked(false);
	QVERIFY(!model.locked());
	model.setName(0, QStringLiteral("Changed"));
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Changed"));
}

void ServersListModelTest::watchesDirectoryOnlyWhileLocked()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	ServersListModel model(dir.path());
	model.startWatching();
	model.addServer();
	model.setName(0, QStringLiteral("Unsaved edit"));

	QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);

	// Unlocked and observed: the user may be mid-edit, so a change in the
	// directory (here a stray file, standing in for the game or another
	// launcher touching it) must not reload the list underneath them. The
	// widget's ServersModel watched only while locked for this reason.
	{
		QFile stray(dir.filePath(QStringLiteral("stray-1.txt")));
		QVERIFY(stray.open(QIODevice::WriteOnly));
		stray.write("x");
	}
	QTest::qWait(500);
	QCOMPARE(resetSpy.count(), 0);
	QCOMPARE(model.rowCount(), 1);
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Unsaved edit"));

	// Locking (the game started) flushes the edit to disk and starts the
	// watch: from here an external change is picked up, as a positive
	// control that the watch really works in this environment.
	model.setLocked(true);
	{
		QFile stray(dir.filePath(QStringLiteral("stray-2.txt")));
		QVERIFY(stray.open(QIODevice::WriteOnly));
		stray.write("x");
	}
	QTRY_VERIFY_WITH_TIMEOUT(resetSpy.count() >= 1, 5000);
	QCOMPARE(model.rowCount(), 1);
	QCOMPARE(model.data(model.index(0), ServersListModel::NameRole).toString(),
			QStringLiteral("Unsaved edit"));

	// Unlocking (the game closed) drops the watch again.
	model.setLocked(false);
	// Let anything the engine had already queued from the locked phase
	// arrive before taking the baseline.
	QTest::qWait(300);
	const int resetsBefore = resetSpy.count();
	{
		QFile stray(dir.filePath(QStringLiteral("stray-3.txt")));
		QVERIFY(stray.open(QIODevice::WriteOnly));
		stray.write("x");
	}
	QTest::qWait(500);
	QCOMPARE(resetSpy.count(), resetsBefore);
}

void ServersListModelTest::addressOfOutOfRange()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());

	ServersListModel model(dir.path());
	model.startWatching();
	QCOMPARE(model.addressOf(-1), QString());
	QCOMPARE(model.addressOf(0), QString());
}

QTEST_GUILESS_MAIN(ServersListModelTest)
#include "ServersListModel_test.moc"
