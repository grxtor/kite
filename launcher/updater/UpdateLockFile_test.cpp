/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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
#include <QTemporaryDir>
#include <QTest>

#include "updater/UpdateLockFile.h"

/*! The lock file is the only channel between the two install passes. The second pass writes its own
 * stage and pid over the first pass's, and the first pass treats seeing that as proof the update
 * was taken over; if either field stopped surviving a write and a read, every hand-off would look
 * like one that never happened. */
class UpdateLockFileTest : public QObject
{
	Q_OBJECT

  private slots:
	//! Everything written comes back, the handshake fields included.
	void test_roundTrip()
	{
		QTemporaryDir dataDir;
		QVERIFY(dataDir.isValid());

		const QString path = UpdateLockFile::lockPath(dataDir.path());

		UpdateLockFile::Contents written;
		// Second resolution: the file carries ISO 8601 without milliseconds.
		written.timestamp =
			QDateTime::fromSecsSinceEpoch(QDateTime::currentSecsSinceEpoch());
		written.from = QStringLiteral("10.0.0");
		written.to = QStringLiteral("v11.0.0");
		written.target = QStringLiteral("/opt/Kite");
		written.dataPath = dataDir.path();
		written.stage = 1;
		written.pid = 4711;

		QVERIFY(UpdateLockFile::write(path, written));
		QVERIFY(QFile::exists(path));

		UpdateLockFile::Contents read;
		QVERIFY(UpdateLockFile::read(path, &read));

		QCOMPARE(read.timestamp, written.timestamp);
		QCOMPARE(read.from, written.from);
		QCOMPARE(read.to, written.to);
		QCOMPARE(read.target, written.target);
		QCOMPARE(read.dataPath, written.dataPath);
		QCOMPARE(read.stage, written.stage);
		QCOMPARE(read.pid, written.pid);
	}

	/*! A second pass's claim is distinguishable from a first pass's lock: the comparison the first
	 * pass makes before it reports a hand-off. */
	void test_installStageClaimIsRecognisable()
	{
		QTemporaryDir dataDir;
		QVERIFY(dataDir.isValid());

		const QString path = UpdateLockFile::lockPath(dataDir.path());

		UpdateLockFile::Contents firstPass;
		firstPass.timestamp = QDateTime::currentDateTime();
		firstPass.from = QStringLiteral("10.0.0");
		firstPass.to = QStringLiteral("v11.0.0");
		firstPass.stage = 1;
		firstPass.pid = 100;
		QVERIFY(UpdateLockFile::write(path, firstPass));

		UpdateLockFile::Contents beforeTakeover;
		QVERIFY(UpdateLockFile::read(path, &beforeTakeover));
		QVERIFY(beforeTakeover.stage != UpdateLockFile::kInstallStage);

		// The second pass's first action, keeping the fields that describe the update.
		UpdateLockFile::Contents secondPass = beforeTakeover;
		secondPass.stage = UpdateLockFile::kInstallStage;
		secondPass.pid = 200;
		QVERIFY(UpdateLockFile::write(path, secondPass));

		UpdateLockFile::Contents afterTakeover;
		QVERIFY(UpdateLockFile::read(path, &afterTakeover));
		QCOMPARE(afterTakeover.stage, UpdateLockFile::kInstallStage);
		QCOMPARE(afterTakeover.pid, 200);
		QVERIFY(afterTakeover.pid != firstPass.pid);
		// The claim must not cost the description of the update (what the launcher shows if the
		// lock is left behind).
		QCOMPARE(afterTakeover.from, firstPass.from);
		QCOMPARE(afterTakeover.to, firstPass.to);
	}

	/*! A lock from an updater that predates the handshake still reads: an absent stage and pid come
	 * back as unstated, not as a claim, or a first pass would report a hand-off that never
	 * happened. */
	void test_lockWithoutHandshakeFields()
	{
		QTemporaryDir dataDir;
		QVERIFY(dataDir.isValid());

		const QString path = UpdateLockFile::lockPath(dataDir.path());

		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
		file.write("TIMESTAMP=2026-01-02T03:04:05\n"
				   "FROM=9.0.0\n"
				   "TO=v10.0.0\n"
				   "TARGET=/opt/Kite\n"
				   "DATA_PATH=/home/someone/.local/share/Kite\n"
				   // A field a later updater might add; an older reader must ignore it, not refuse the file.
				   "SOMETHING_NEW=42\n");
		file.close();

		UpdateLockFile::Contents read;
		QVERIFY(UpdateLockFile::read(path, &read));
		QCOMPARE(read.from, QStringLiteral("9.0.0"));
		QCOMPARE(read.to, QStringLiteral("v10.0.0"));
		QCOMPARE(read.stage, 0);
		QCOMPARE(read.pid, 0);
		QVERIFY(read.stage != UpdateLockFile::kInstallStage);
	}

	//! A file that is not there is reported as not there.
	void test_missingLock()
	{
		QTemporaryDir dataDir;
		QVERIFY(dataDir.isValid());

		UpdateLockFile::Contents read;
		read.stage = 2;
		read.pid = 999;

		QVERIFY(!UpdateLockFile::read(
			UpdateLockFile::lockPath(dataDir.path()), &read));

		// read() clears what it was given, so a failed read cannot leave another lock's fields.
		QCOMPARE(read.stage, 0);
		QCOMPARE(read.pid, 0);
	}

	/*! The packaging templates (Scoop, AUR) drop updates-managed-by.txt into the installation; Kite
	 * then only announces releases. Anything but a plain word is ignored rather than shown. */
	void test_managedBy_data()
	{
		QTest::addColumn<QByteArray>("contents");
		QTest::addColumn<QString>("expected");

		QTest::newRow("scoop") << QByteArray("scoop\n") << QStringLiteral("scoop");
		QTest::newRow("windows line end") << QByteArray("scoop\r\n") << QStringLiteral("scoop");
		QTest::newRow("pacman, spaces") << QByteArray("  pacman  ") << QStringLiteral("pacman");
		QTest::newRow("empty") << QByteArray() << QString();
		QTest::newRow("markup") << QByteArray("<b>x</b>") << QString();
		QTest::newRow("sentence") << QByteArray("my package manager") << QString();
		QTest::newRow("too long") << QByteArray(40, 'a') << QString();
	}

	void test_managedBy()
	{
		QFETCH(QByteArray, contents);
		QFETCH(QString, expected);

		QTemporaryDir root;
		QVERIFY(root.isValid());
		QCOMPARE(UpdateLockFile::managedBy(root.path()), QString());

		QFile file(QDir(root.path()).absoluteFilePath(
			QLatin1String(UpdateLockFile::kManagedByFileName)));
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write(contents);
		file.close();
		QCOMPARE(UpdateLockFile::managedBy(root.path()), expected);
	}
};

QTEST_GUILESS_MAIN(UpdateLockFileTest)

#include "UpdateLockFile_test.moc"
