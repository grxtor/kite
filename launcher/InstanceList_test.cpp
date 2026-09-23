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
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "InstanceList.h"

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
} // namespace

/*
 * Exercises InstanceList::newestScreenshotUrl() - the lookup CoverImageRole
 * caches per instance id in data() - directly, rather than through a full
 * InstanceList: that needs a SettingsObjectPtr and real BaseInstance
 * subclasses to construct, which would make this test about instance
 * bookkeeping InstanceList already has other coverage for, not about the
 * screenshot lookup itself.
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
};

QTEST_GUILESS_MAIN(InstanceListTest)

#include "InstanceList_test.moc"
