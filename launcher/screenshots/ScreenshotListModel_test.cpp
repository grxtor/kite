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
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "screenshots/ScreenshotListModel.h"

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
	// Flushed before the explicit mtime is set: QFile may not push
	// buffered bytes to the OS until this point, and a write landing
	// after setFileTime() would bump the mtime back to "now" and defeat
	// the whole point of stamping it -- explicit mtimes, rather than
	// relying on creation order plus a real sleep, are what keep the
	// ordering test fast and non-flaky.
	if (!file.flush()) {
		return false;
	}
	const bool timeSet =
		file.setFileTime(modified, QFileDevice::FileModificationTime);
	file.close();
	return timeSet;
}
} // namespace

class ScreenshotListModelTest : public QObject
{
	Q_OBJECT
  private slots:

	void ordersNewestModifiedFirstAndFiltersByExtension()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();

		const QDateTime base = QDateTime::currentDateTime();
		QVERIFY(writeFile(QDir(dir).filePath("older.png"), "a",
						   base.addSecs(-60)));
		QVERIFY(
			writeFile(QDir(dir).filePath("newer.jpg"), "bb", base));
		// Not an image extension: must not show up at all.
		QVERIFY(writeFile(QDir(dir).filePath("ignored.txt"), "c", base));

		ScreenshotListModel model;
		model.setDirectory(dir);

		QCOMPARE(model.count(), 2);
		QCOMPARE(model.rowCount(), 2);
		QCOMPARE(model
					 .data(model.index(0), ScreenshotListModel::NameRole)
					 .toString(),
				 QStringLiteral("newer.jpg"));
		QCOMPARE(model
					 .data(model.index(1), ScreenshotListModel::NameRole)
					 .toString(),
				 QStringLiteral("older.png"));
	}

	void rolesExposeExpectedData()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();
		const QDateTime modified = QDateTime::currentDateTime();
		const QString path = QDir(dir).filePath("shot.png");
		QVERIFY(writeFile(path, "hello", modified));
		const QString absolutePath = QFileInfo(path).absoluteFilePath();

		ScreenshotListModel model;
		model.setDirectory(dir);
		QCOMPARE(model.count(), 1);

		const QModelIndex idx = model.index(0);
		QCOMPARE(
			model.data(idx, ScreenshotListModel::NameRole).toString(),
			QStringLiteral("shot.png"));
		QCOMPARE(
			model.data(idx, ScreenshotListModel::PathRole).toString(),
			absolutePath);
		QCOMPARE(
			model.data(idx, ScreenshotListModel::UrlRole).toString(),
			QUrl::fromLocalFile(absolutePath).toString());
		QCOMPARE(
			model.data(idx, ScreenshotListModel::SizeRole).toLongLong(),
			Q_INT64_C(5));
		QVERIFY(model.data(idx, ScreenshotListModel::ModifiedRole)
					.toLongLong() > 0);
	}

	void watcherPicksUpAddedFile()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();

		ScreenshotListModel model;
		model.setDirectory(dir);
		QCOMPARE(model.count(), 0);

		QVERIFY(writeFile(QDir(dir).filePath("added.png"), "x",
						   QDateTime::currentDateTime()));

		// The watcher + debounce timer are asynchronous; QTRY_COMPARE
		// polls until the refresh has had time to fire.
		QTRY_COMPARE(model.count(), 1);
		QCOMPARE(model
					 .data(model.index(0), ScreenshotListModel::NameRole)
					 .toString(),
				 QStringLiteral("added.png"));
	}

	void removeDeletesFileAndShrinksModel()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();
		const QString path = QDir(dir).filePath("gone.png");
		QVERIFY(writeFile(path, "y", QDateTime::currentDateTime()));

		ScreenshotListModel model;
		model.setDirectory(dir);
		QCOMPARE(model.count(), 1);

		QVERIFY(model.remove(0));
		QCOMPARE(model.count(), 0);
		QVERIFY(!QFileInfo::exists(path));
	}

	void removeOutOfRangeFails()
	{
		ScreenshotListModel model;
		QVERIFY(!model.remove(0));
		QVERIFY(!model.remove(-1));
	}

	void pathAtReturnsAbsolutePathOrEmpty()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString dir = tempDir.path();
		const QString path = QDir(dir).filePath("p.png");
		QVERIFY(writeFile(path, "z", QDateTime::currentDateTime()));

		ScreenshotListModel model;
		model.setDirectory(dir);
		QCOMPARE(model.pathAt(0), QFileInfo(path).absoluteFilePath());
		QCOMPARE(model.pathAt(1), QString());
		QCOMPARE(model.pathAt(-1), QString());
	}
};

QTEST_GUILESS_MAIN(ScreenshotListModelTest)

#include "ScreenshotListModel_test.moc"
