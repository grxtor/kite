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
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include "GZip.h"
#include "models/OtherLogsModel.h"
#include "pathmatcher/RegexpMatcher.h"

namespace
{
bool writeFile(const QString& path, const QByteArray& contents)
{
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}
	return file.write(contents) == contents.size();
}

/* Same filter shape as MinecraftInstance::getLogFileMatcher(): *.log
 * (optionally .gz) and crash-*.txt, matched against paths relative to the
 * watched root - see RecursiveFileSystemWatcher::scanRecursive(). */
IPathMatcher::Ptr logFilter()
{
	return std::make_shared<RegexpMatcher>(
		QStringLiteral(R"(.*\.log(\.gz)?$|crash-.*\.txt$)"));
}
} // namespace

class OtherLogsModelTest : public QObject
{
	Q_OBJECT
  private slots:

	void listsMatchingFilesOnlyAndExposesTheNameRole()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();
		QVERIFY(writeFile(QDir(dir).filePath("latest.log"), "hello"));
		QVERIFY(QDir(dir).mkpath("crash-reports"));
		QVERIFY(writeFile(
			QDir(dir).filePath("crash-reports/crash-2026-01-01.txt"), "oops"));
		// Not matched by the filter: must not show up at all.
		QVERIFY(writeFile(QDir(dir).filePath("options.txt"), "ignored"));

		OtherLogsModel model(dir, logFilter());
		QCOMPARE(model.rowCount(), 2);

		QStringList names;
		for (int i = 0; i < model.rowCount(); ++i) {
			names.append(
				model.data(model.index(i), OtherLogsModel::NameRole).toString());
		}
		QVERIFY(names.contains(QStringLiteral("latest.log")));
		QVERIFY(names.contains(
			QStringLiteral("crash-reports/crash-2026-01-01.txt")));
	}

	void selectFileLoadsPlainTextContent()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();
		QVERIFY(writeFile(QDir(dir).filePath("latest.log"), "line one\nline two"));

		OtherLogsModel model(dir, logFilter());
		model.selectFile(QStringLiteral("latest.log"));

		QCOMPARE(model.currentFile(), QStringLiteral("latest.log"));
		QCOMPARE(model.content(), QStringLiteral("line one\nline two"));
	}

	void selectFileDecompressesGzip()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();

		const QByteArray original = "a rotated, compressed log line";
		QByteArray compressed;
		QVERIFY(GZip::zip(original, compressed));
		QVERIFY(writeFile(QDir(dir).filePath("old.log.gz"), compressed));

		OtherLogsModel model(dir, logFilter());
		model.selectFile(QStringLiteral("old.log.gz"));

		QCOMPARE(model.content(), QString::fromUtf8(original));
	}

	void selectingEmptyNameClearsContent()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();
		QVERIFY(writeFile(QDir(dir).filePath("latest.log"), "hi"));

		OtherLogsModel model(dir, logFilter());
		model.selectFile(QStringLiteral("latest.log"));
		QCOMPARE(model.content(), QStringLiteral("hi"));

		model.selectFile(QString());
		QCOMPARE(model.currentFile(), QString());
		QCOMPARE(model.content(), QString());
	}

	void deleteCurrentRemovesFileAndClearsSelection()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();
		const QString path = QDir(dir).filePath("latest.log");
		QVERIFY(writeFile(path, "gone soon"));

		OtherLogsModel model(dir, logFilter());
		model.selectFile(QStringLiteral("latest.log"));

		QVERIFY(model.deleteCurrent());
		QVERIFY(!QFileInfo::exists(path));
		QCOMPARE(model.currentFile(), QString());
		QCOMPARE(model.content(), QString());
	}

	void deleteCurrentWithNoSelectionFails()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		OtherLogsModel model(tempDir.path(), logFilter());
		QVERIFY(!model.deleteCurrent());
	}

	void deleteAllRemovesEveryListedFile()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();
		QVERIFY(writeFile(QDir(dir).filePath("a.log"), "1"));
		QVERIFY(writeFile(QDir(dir).filePath("b.log"), "2"));

		OtherLogsModel model(dir, logFilter());
		QCOMPARE(model.rowCount(), 2);

		const QStringList failed = model.deleteAll();
		QVERIFY(failed.isEmpty());
		QVERIFY(!QFileInfo::exists(QDir(dir).filePath("a.log")));
		QVERIFY(!QFileInfo::exists(QDir(dir).filePath("b.log")));
	}
};

QTEST_GUILESS_MAIN(OtherLogsModelTest)

#include "OtherLogsModel_test.moc"
