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

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include "models/LauncherLogs.h"
#include "models/LogUploader.h"
#include "models/OtherLogsModel.h"

namespace
{
bool writeFile(const QString& path, const QByteArray& contents)
{
	QDir().mkpath(QFileInfo(path).absolutePath());
	QFile file(path);
	return file.open(QIODevice::WriteOnly) &&
		   file.write(contents) == contents.size();
}
} // namespace

class LauncherLogsTest : public QObject
{
	Q_OBJECT
  private slots:

	void patternMatchesOnlyTheLaunchersOwnNumberedLogs()
	{
		const QRegularExpression re(LauncherLogs::fileNamePattern("MeshMC"));
		QVERIFY(re.match("MeshMC-0.log").hasMatch());
		QVERIFY(re.match("MeshMC-12.log").hasMatch());
		QVERIFY(!re.match("MeshMC.log").hasMatch());
		QVERIFY(!re.match("MeshMC-0.log.bak").hasMatch());
		QVERIFY(!re.match("Other-0.log").hasMatch());
	}

	// The data folder holds every instance: their logs must not show up,
	// and neither must anything else in the folder.
	void listsOnlyTheLogsInTheFolderItself()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();
		QVERIFY(writeFile(dir + "/MeshMC-0.log", "now"));
		QVERIFY(writeFile(dir + "/MeshMC-1.log", "before"));
		QVERIFY(writeFile(dir + "/meshmc.cfg", "x"));
		QVERIFY(writeFile(dir + "/instances/a/logs/MeshMC-9.log", "nested"));

		LauncherLogs logs(dir, "MeshMC");
		auto* model = qobject_cast<OtherLogsModel*>(logs.logs());
		QVERIFY(model);
		QCOMPARE(model->rowCount(), 2);
		QCOMPARE(model->data(model->index(0, 0), OtherLogsModel::NameRole).toString(),
				 QString("MeshMC-0.log"));
		QCOMPARE(model->data(model->index(1, 0), OtherLogsModel::NameRole).toString(),
				 QString("MeshMC-1.log"));

		model->selectFile("MeshMC-1.log");
		QCOMPARE(model->content(), QString("before"));
		QVERIFY(model->deleteCurrent());
		QVERIFY(!QFile::exists(dir + "/MeshMC-1.log"));
		QVERIFY(QFile::exists(dir + "/instances/a/logs/MeshMC-9.log"));
	}

	void deleteAllLeavesTheRestOfTheFolderAlone()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();
		QVERIFY(writeFile(dir + "/MeshMC-0.log", "a"));
		QVERIFY(writeFile(dir + "/MeshMC-1.log", "b"));
		QVERIFY(writeFile(dir + "/meshmc.cfg", "keep"));

		LauncherLogs logs(dir, "MeshMC");
		auto* model = qobject_cast<OtherLogsModel*>(logs.logs());
		QVERIFY(model->deleteAll().isEmpty());
		QVERIFY(!QFile::exists(dir + "/MeshMC-0.log"));
		QVERIFY(!QFile::exists(dir + "/MeshMC-1.log"));
		QVERIFY(QFile::exists(dir + "/meshmc.cfg"));
	}

	void exposesAnUploaderAndTheFolder()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		LauncherLogs logs(tempDir.path(), "MeshMC");
		QVERIFY(qobject_cast<LogUploader*>(logs.uploader()));
		QCOMPARE(logs.folder(), tempDir.path());
	}
};

QTEST_GUILESS_MAIN(LauncherLogsTest)

#include "LauncherLogs_test.moc"
