// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "FileSystem.h"
#include "backup/BackupManager.h"
#include "models/TrashUndo.h"

class TrashUndoTest : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

	QString makeFile(const QString& relative)
	{
		const QString path = m_dir.filePath(relative);
		FS::ensureFilePathExists(path);
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return {};
		}
		file.write(relative.toUtf8());
		return path;
	}

  private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		// Never the real trash: a folder of the test's own stands in.
		FS::setTrashFolderForTests(m_dir.filePath("trash"));
	}

	void cleanupTestCase()
	{
		FS::setTrashFolderForTests(QString());
	}

	// Several files deleted together come back with one undo.
	void test_oneUndoForTheWholeBatch()
	{
		const QString a = makeFile("batch/a.jar");
		const QString b = makeFile("batch/b.jar");

		TrashUndo undo;
		QVERIFY(undo.available());
		QVERIFY(!undo.canUndo());

		undo.begin();
		QList<FS::Trashed> trashed;
		QVERIFY(FS::trashOrDelete(a, &trashed));
		QVERIFY(FS::trashOrDelete(b, &trashed));
		undo.add(trashed);
		QVERIFY(undo.canUndo());

		QSignalSpy restored(&undo, &TrashUndo::restored);
		QVERIFY(undo.undo());
		QVERIFY(QFileInfo::exists(a));
		QVERIFY(QFileInfo::exists(b));
		QVERIFY(!undo.canUndo());
		QCOMPARE(restored.count(), 1);
		QCOMPARE(restored.first().first().toStringList(),
				 QStringList({a, b}));
	}

	// The next delete replaces the batch: undo never reaches further back.
	void test_beginForgetsThePreviousBatch()
	{
		const QString old = makeFile("replace/old.jar");
		const QString recent = makeFile("replace/recent.jar");

		TrashUndo undo;
		QList<FS::Trashed> first;
		QVERIFY(FS::trashOrDelete(old, &first));
		undo.add(first);

		undo.begin();
		QVERIFY(!undo.canUndo());
		QList<FS::Trashed> second;
		QVERIFY(FS::trashOrDelete(recent, &second));
		undo.add(second);

		QVERIFY(undo.undo());
		QVERIFY(QFileInfo::exists(recent));
		QVERIFY(!QFileInfo::exists(old));
	}

	// Nothing went to the trash (a permanent delete): nothing to undo.
	void test_permanentDeleteOffersNoUndo()
	{
		TrashUndo undo;
		undo.begin();
		undo.add(QList<FS::Trashed>());
		QVERIFY(!undo.canUndo());
	}

	// What cannot go back stays in the trash and in the batch.
	void test_blockedItemStaysForAnotherTry()
	{
		const QString free = makeFile("partial/free.jar");
		const QString blocked = makeFile("partial/blocked.jar");

		TrashUndo undo;
		QList<FS::Trashed> trashed;
		QVERIFY(FS::trashOrDelete(free, &trashed));
		QVERIFY(FS::trashOrDelete(blocked, &trashed));
		undo.add(trashed);
		QVERIFY(!makeFile("partial/blocked.jar").isEmpty());

		QVERIFY(!undo.undo());
		QVERIFY(QFileInfo::exists(free));
		QVERIFY(undo.canUndo());

		QVERIFY(QFile::remove(blocked));
		QVERIFY(undo.undo());
		QVERIFY(!undo.canUndo());
		QFile back(blocked);
		QVERIFY(back.open(QIODevice::ReadOnly));
		QCOMPARE(back.readAll(), QByteArray("partial/blocked.jar"));
	}

	// A deleted backup goes to the trash and comes back.
	void test_backupGoesToTrashAndComesBack()
	{
		const QString zip = makeFile("instance/.backups/2026-01-01_10-00-00.zip");
		BackupManager manager("instance", m_dir.filePath("instance"));
		BackupEntry entry;
		entry.fullPath = zip;

		TrashUndo undo;
		QList<FS::Trashed> trashed;
		QVERIFY(manager.deleteBackup(entry, &trashed));
		QVERIFY(!QFileInfo::exists(zip));
		undo.add(trashed);

		QVERIFY(undo.undo());
		QVERIFY(QFileInfo::exists(zip));
	}
};

QTEST_GUILESS_MAIN(TrashUndoTest)

#include "TrashUndo_test.moc"
