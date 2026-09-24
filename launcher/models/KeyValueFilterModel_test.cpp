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
#include <QTemporaryDir>
#include <QTest>

#include "minecraft/gameoptions/GameOptions.h"
#include "models/KeyValueFilterModel.h"

namespace
{
bool writeOptions(const QString& path, const QByteArray& contents)
{
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}
	return file.write(contents) == contents.size();
}
} // namespace

class KeyValueFilterModelTest : public QObject
{
	Q_OBJECT
  private slots:

	void emptyFilterKeepsEveryRow()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = QDir(dir.path()).filePath("options.txt");
		QVERIFY(writeOptions(path, "renderDistance:12\nfov:0\n"));

		GameOptions source(path);
		KeyValueFilterModel filter;
		filter.setSourceModel(&source);

		QCOMPARE(filter.rowCount(), 2);
	}

	void filtersByKeyCaseInsensitively()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = QDir(dir.path()).filePath("options.txt");
		QVERIFY(writeOptions(path, "renderDistance:12\nfov:0\nguiScale:2\n"));

		GameOptions source(path);
		KeyValueFilterModel filter;
		filter.setSourceModel(&source);

		filter.setFilterText("render");
		QCOMPARE(filter.rowCount(), 1);
		QCOMPARE(filter.data(filter.index(0, 0), GameOptions::KeyRole).toString(),
				 QStringLiteral("renderDistance"));

		filter.setFilterText("RENDER");
		QCOMPARE(filter.rowCount(), 1);
	}

	void filtersByValueToo()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = QDir(dir.path()).filePath("options.txt");
		QVERIFY(writeOptions(path, "renderDistance:12\nfov:70\n"));

		GameOptions source(path);
		KeyValueFilterModel filter;
		filter.setSourceModel(&source);

		filter.setFilterText("70");
		QCOMPARE(filter.rowCount(), 1);
		QCOMPARE(filter.data(filter.index(0, 0), GameOptions::ValueRole).toString(),
				 QStringLiteral("70"));
	}

	void noMatchesLeavesModelEmpty()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = QDir(dir.path()).filePath("options.txt");
		QVERIFY(writeOptions(path, "renderDistance:12\n"));

		GameOptions source(path);
		KeyValueFilterModel filter;
		filter.setSourceModel(&source);

		filter.setFilterText("doesNotExist");
		QCOMPARE(filter.rowCount(), 0);

		filter.setFilterText("");
		QCOMPARE(filter.rowCount(), 1);
	}
};

QTEST_GUILESS_MAIN(KeyValueFilterModelTest)

#include "KeyValueFilterModel_test.moc"
