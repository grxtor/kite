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

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "models/FolderSettingsController.h"

class FolderSettingsControllerTest : public QObject
{
	Q_OBJECT

  private slots:
	void knowsTheFolderSettings();
	void warnsAboutExclamationMarks();
	void warnsAboutBorrowedFlatpakFolders();
	void acceptsOrdinaryFolders();
	void refusesDuplicateExtraFolders();
	void doesNothingWithoutALauncher();
};

void FolderSettingsControllerTest::knowsTheFolderSettings()
{
	for (const char* key : {"InstanceDir", "CentralModsDir", "IconsDir",
							"SkinsDir", "JavaDir"}) {
		QVERIFY(FolderSettingsController::isFolderKey(key));
	}
	// Only folder settings may be pointed at a folder.
	QVERIFY(!FolderSettingsController::isFolderKey("JavaPath"));
	QVERIFY(!FolderSettingsController::isFolderKey(""));
}

void FolderSettingsControllerTest::warnsAboutExclamationMarks()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	const QString bad = dir.filePath("my!games");
	QVERIFY(QDir().mkpath(bad));
	QVERIFY(!FolderSettingsController::instanceDirWarning(bad, bad, false)
				 .isEmpty());
}

void FolderSettingsControllerTest::warnsAboutBorrowedFlatpakFolders()
{
	// Only evidence under /run/user counts, and only inside a Flatpak.
	const QString raw = "/run/user/1000/doc/abc/instances";
	QVERIFY(!FolderSettingsController::instanceDirWarning(raw, "/home/u/inst",
														  true)
				 .isEmpty());
	QVERIFY(FolderSettingsController::instanceDirWarning(raw, "/home/u/inst",
														 false)
				.isEmpty());
	QVERIFY(FolderSettingsController::instanceDirWarning(
				"/home/u/instances", "/home/u/instances", true)
				.isEmpty());
}

void FolderSettingsControllerTest::acceptsOrdinaryFolders()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QVERIFY(FolderSettingsController::instanceDirWarning(dir.path(), dir.path(),
														 false)
				.isEmpty());
}

void FolderSettingsControllerTest::refusesDuplicateExtraFolders()
{
	const QStringList extra = {"/data/more", "/Data/Other"};
	QVERIFY(!FolderSettingsController::duplicateDirMessage("/data/main",
														   "/data/main", extra)
				 .isEmpty());
	QVERIFY(!FolderSettingsController::duplicateDirMessage("/data/more",
														   "/data/main", extra)
				 .isEmpty());
	// Case does not make it a different folder.
	QVERIFY(!FolderSettingsController::duplicateDirMessage("/data/other",
														   "/data/main", extra)
				 .isEmpty());
	QVERIFY(FolderSettingsController::duplicateDirMessage("/data/new",
														  "/data/main", extra)
				.isEmpty());
}

void FolderSettingsControllerTest::doesNothingWithoutALauncher()
{
	QTemporaryDir dir;
	FolderSettingsController controller;
	QSignalSpy changed(&controller,
					   &FolderSettingsController::additionalDirsChanged);
	QVERIFY(!controller.setFolder("InstanceDir", QUrl::fromLocalFile(dir.path())));
	QVERIFY(!controller.addAdditionalDir(QUrl::fromLocalFile(dir.path())));
	controller.removeAdditionalDir("/nothing");
	QVERIFY(controller.additionalDirs().isEmpty());
	QCOMPARE(changed.count(), 0);
}

QTEST_GUILESS_MAIN(FolderSettingsControllerTest)
#include "FolderSettingsController_test.moc"
