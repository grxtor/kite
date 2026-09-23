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

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "InstanceList.h"
#include "NullInstance.h"
#include "settings/INISettingsObject.h"

namespace
{
bool writeFile(const QString& path, const QByteArray& contents,
			   const QDateTime& modified)
{
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}
	if (file.write(contents) != contents.size()) {
		return false;
	}
	// Flushed before the explicit mtime is set, same as
	// ScreenshotListModel_test.cpp's helper - a write landing after
	// setFileTime() would bump the mtime back to "now".
	if (!file.flush()) {
		return false;
	}
	const bool timeSet =
		file.setFileTime(modified, QFileDevice::FileModificationTime);
	file.close();
	return timeSet;
}

/* Same set of global settings models/InstanceDetails_test.cpp's
 * makeGlobalSettings() registers - BaseInstance's constructor (run for
 * every concrete instance InstanceList::loadInstance() creates) overrides
 * or passes through exactly these ids, and a globalSettings without one of
 * them makes registration hand back a null Setting. */
SettingsObjectPtr makeGlobalSettings(QTemporaryDir& dir)
{
	auto settings =
		std::make_shared<INISettingsObject>(dir.filePath("global.ini"));
	settings->registerSetting("PreLaunchCommand", "");
	settings->registerSetting("WrapperCommand", "");
	settings->registerSetting("PostExitCommand", "");
	settings->registerSetting("ShowConsole", true);
	settings->registerSetting("AutoCloseConsole", false);
	settings->registerSetting("ShowConsoleOnError", true);
	settings->registerSetting("LogPrePostOutput", true);
	settings->registerSetting("ConsoleMaxLines", 100000);
	settings->registerSetting("ConsoleOverflowStop", true);
	return settings;
}
} // namespace

/*
 * The first few tests exercise InstanceList::newestScreenshotUrl() - the
 * lookup CoverImageRole schedules a background scan for in data() - directly,
 * rather than through a full InstanceList: that needs a SettingsObjectPtr and
 * real BaseInstance subclasses to construct, which would make those tests
 * about instance bookkeeping InstanceList already has other coverage for,
 * not about the screenshot lookup itself. HasCrashedRole has no such
 * standalone helper - the flag lives on BaseInstance - and CoverImageRole's
 * own async scheduling/caching/invalidation needs a real row to ask data()
 * about, so those tests build the minimal real InstanceList+NullInstance
 * fixture instead (same fixture shape as models/InstanceDetails_test.cpp's),
 * waiting out the background scan with QTRY_COMPARE.
 */
class InstanceListTest : public QObject
{
	Q_OBJECT
  private slots:

	void picksNewestModifiedImage()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();

		const QDateTime base = QDateTime::currentDateTime();
		QVERIFY(writeFile(QDir(dir).filePath("older.png"), "a",
						   base.addSecs(-60)));
		const QString newerPath = QDir(dir).filePath("newer.jpg");
		QVERIFY(writeFile(newerPath, "bb", base));

		QCOMPARE(InstanceList::newestScreenshotUrl(dir),
				 QUrl::fromLocalFile(QFileInfo(newerPath).absoluteFilePath())
					 .toString());
	}

	void ignoresNonImageFiles()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();

		QVERIFY(writeFile(QDir(dir).filePath("notes.txt"), "c",
						   QDateTime::currentDateTime()));

		QCOMPARE(InstanceList::newestScreenshotUrl(dir), QString());
	}

	void matchesExtensionsCaseInsensitively()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();

		const QString path = QDir(dir).filePath("shot.JPEG");
		QVERIFY(writeFile(path, "x", QDateTime::currentDateTime()));

		QCOMPARE(
			InstanceList::newestScreenshotUrl(dir),
			QUrl::fromLocalFile(QFileInfo(path).absoluteFilePath()).toString());
	}

	void emptyForMissingOrEmptyDirectory()
	{
		QCOMPARE(InstanceList::newestScreenshotUrl(QString()), QString());

		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		QCOMPARE(InstanceList::newestScreenshotUrl(
					 QDir(tempDir.path()).filePath("does-not-exist")),
				 QString());
		QCOMPARE(InstanceList::newestScreenshotUrl(tempDir.path()), QString());
	}

	void hasCrashedRoleReflectsInstanceState()
	{
		QTemporaryDir globalDir;
		QVERIFY(globalDir.isValid());
		QTemporaryDir instsDir;
		QVERIFY(instsDir.isValid());
		SettingsObjectPtr globalSettings = makeGlobalSettings(globalDir);

		const QString instRoot = QDir(instsDir.path()).filePath("testinst");
		QVERIFY(QDir().mkpath(instRoot));
		/* Unrecognized InstanceType, same as a folder InstanceList itself
		 * cannot make sense of: loadInstance() falls back to NullInstance
		 * for anything that is not OneSix/Nostalgia/Legacy, which is the
		 * one concrete BaseInstance simple enough to construct here. */
		QVERIFY(writeFile(QDir(instRoot).filePath("instance.cfg"),
						   "InstanceType=NullTest\n",
						   QDateTime::currentDateTime()));

		InstanceList list(globalSettings, {instsDir.path()});
		QCOMPARE(list.loadList(), InstanceList::NoError);

		InstancePtr inst = list.getInstanceById("testinst");
		QVERIFY(inst);
		const QModelIndex idx = list.getInstanceIndexById("testinst");
		QVERIFY(idx.isValid());

		QCOMPARE(list.roleNames().value(InstanceList::HasCrashedRole),
				 QByteArray("hasCrashed"));
		QCOMPARE(list.data(idx, InstanceList::HasCrashedRole).toBool(),
				 false);

		QSignalSpy dataChangedSpy(&list, &InstanceList::dataChanged);
		inst->setCrashed(true);
		QVERIFY(!dataChangedSpy.isEmpty());
		QCOMPARE(list.data(idx, InstanceList::HasCrashedRole).toBool(), true);

		dataChangedSpy.clear();
		inst->setCrashed(false);
		QVERIFY(!dataChangedSpy.isEmpty());
		QCOMPARE(list.data(idx, InstanceList::HasCrashedRole).toBool(),
				 false);
	}

	void coverImageRoleScansAsynchronouslyAndCachesResult()
	{
		QTemporaryDir globalDir;
		QVERIFY(globalDir.isValid());
		QTemporaryDir instsDir;
		QVERIFY(instsDir.isValid());
		SettingsObjectPtr globalSettings = makeGlobalSettings(globalDir);

		/* Canonicalized before anything is built from it: InstanceList
		 * itself canonicalizes every configured instance root (resolving
		 * a symlink like macOS's /var -> /private/var), so building
		 * expectedUrl from the raw, un-resolved QTemporaryDir path would
		 * compare two spellings of the same file and never match. */
		const QString instsRoot =
			QFileInfo(instsDir.path()).canonicalFilePath();
		const QString instRoot = QDir(instsRoot).filePath("testinst");
		QVERIFY(QDir().mkpath(instRoot));
		QVERIFY(writeFile(QDir(instRoot).filePath("instance.cfg"),
						   "InstanceType=NullTest\n",
						   QDateTime::currentDateTime()));
		const QString screenshotsDir =
			QDir(instRoot).filePath("screenshots");
		QVERIFY(QDir().mkpath(screenshotsDir));
		const QString shotPath = QDir(screenshotsDir).filePath("shot.png");
		QVERIFY(writeFile(shotPath, "x", QDateTime::currentDateTime()));
		const QString expectedUrl =
			QUrl::fromLocalFile(QFileInfo(shotPath).absoluteFilePath())
				.toString();

		InstanceList list(globalSettings, {instsDir.path()});
		QCOMPARE(list.loadList(), InstanceList::NoError);
		const QModelIndex idx = list.getInstanceIndexById("testinst");
		QVERIFY(idx.isValid());

		// Nothing cached yet - data() must answer immediately (empty)
		// rather than block on the directory scan, and repeated asks before
		// the scan comes back must not queue a second one (no direct probe
		// for that here, but a duplicate scan finishing later would still
		// only re-publish the same, correct URL below).
		QCOMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
				 QString());
		QCOMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
				 QString());

		QTRY_COMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
					 expectedUrl);
	}

	void coverImageRoleRescansAfterInstanceStops()
	{
		QTemporaryDir globalDir;
		QVERIFY(globalDir.isValid());
		QTemporaryDir instsDir;
		QVERIFY(instsDir.isValid());
		SettingsObjectPtr globalSettings = makeGlobalSettings(globalDir);

		// See coverImageRoleScansAsynchronouslyAndCachesResult() for why
		// this is canonicalized first.
		const QString instsRoot =
			QFileInfo(instsDir.path()).canonicalFilePath();
		const QString instRoot = QDir(instsRoot).filePath("testinst");
		QVERIFY(QDir().mkpath(instRoot));
		QVERIFY(writeFile(QDir(instRoot).filePath("instance.cfg"),
						   "InstanceType=NullTest\n",
						   QDateTime::currentDateTime()));
		const QString screenshotsDir =
			QDir(instRoot).filePath("screenshots");
		QVERIFY(QDir().mkpath(screenshotsDir));
		const QString shotPath = QDir(screenshotsDir).filePath("shot.png");
		QVERIFY(writeFile(shotPath, "x", QDateTime::currentDateTime()));
		const QString expectedUrl =
			QUrl::fromLocalFile(QFileInfo(shotPath).absoluteFilePath())
				.toString();

		InstanceList list(globalSettings, {instsDir.path()});
		QCOMPARE(list.loadList(), InstanceList::NoError);
		InstancePtr inst = list.getInstanceById("testinst");
		QVERIFY(inst);
		const QModelIndex idx = list.getInstanceIndexById("testinst");
		QVERIFY(idx.isValid());

		// Let the first scan land and populate the cache.
		QCOMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
				 QString());
		QTRY_COMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
					 expectedUrl);

		// A play session ending is exactly when a new screenshot tends to
		// appear, so the cached cover is dropped - immediately, not only
		// once a fresh scan happens to finish.
		inst->setRunning(true);
		inst->setRunning(false);
		QCOMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
				 QString());

		// data() above already asked again, which schedules a fresh scan;
		// it eventually lands the same (still correct) URL.
		QTRY_COMPARE(list.data(idx, InstanceList::CoverImageRole).toString(),
					 expectedUrl);
	}
};

QTEST_GUILESS_MAIN(InstanceListTest)

#include "InstanceList_test.moc"
