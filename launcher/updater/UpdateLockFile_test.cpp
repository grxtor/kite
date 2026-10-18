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

/*!
 * The lock file, which is the only thing the two passes of an install can
 * say anything to each other with.
 *
 * The stage and the process id are the load-bearing part: the second pass
 * writes its own over the first pass's, and the first pass treats seeing that
 * as proof the update was taken over. If either field stopped surviving a
 * write and a read, every hand-off would look like one that never happened --
 * or, far worse if the comparison were ever loosened, one that never happened
 * would look like a success. That is the failure this file exists to keep
 * from coming back.
 */
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
		// Second resolution: ISO 8601 without milliseconds is what the file
		// carries, so comparing anything finer would fail for no reason.
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

	/*!
	 * A second pass's claim is distinguishable from a first pass's lock.
	 *
	 * This is exactly the comparison the first pass makes before it reports
	 * an update as handed over.
	 */
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

		// What the second pass does as its first action, keeping the fields
		// that describe the update.
		UpdateLockFile::Contents secondPass = beforeTakeover;
		secondPass.stage = UpdateLockFile::kInstallStage;
		secondPass.pid = 200;
		QVERIFY(UpdateLockFile::write(path, secondPass));

		UpdateLockFile::Contents afterTakeover;
		QVERIFY(UpdateLockFile::read(path, &afterTakeover));
		QCOMPARE(afterTakeover.stage, UpdateLockFile::kInstallStage);
		QCOMPARE(afterTakeover.pid, 200);
		QVERIFY(afterTakeover.pid != firstPass.pid);
		// The claim must not cost the description of the update, which is
		// what the launcher shows if this lock is ever left behind.
		QCOMPARE(afterTakeover.from, firstPass.from);
		QCOMPARE(afterTakeover.to, firstPass.to);
	}

	/*!
	 * A lock from an updater that predates the handshake still reads.
	 *
	 * Its stage and pid are absent, which has to come back as "unstated"
	 * rather than as a claim: treating a missing stage as the install stage
	 * would make a first pass report a hand-off that never happened.
	 */
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
				   // A field a later updater might add; an older reader has
				   // to ignore it rather than refuse the file.
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

		// Read clears what it was given, so a failed read cannot leave a
		// caller looking at another lock's fields.
		QCOMPARE(read.stage, 0);
		QCOMPARE(read.pid, 0);
	}
};

QTEST_GUILESS_MAIN(UpdateLockFileTest)

#include "UpdateLockFile_test.moc"
