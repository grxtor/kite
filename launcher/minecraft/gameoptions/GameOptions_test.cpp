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

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "minecraft/gameoptions/GameOptions.h"

namespace
{
	const QByteArray kOptions = "version:3955\n"
								"autoJump:false\n"
								"renderDistance:12\n"
								"fov:0.0\n"
								"someModsOwnKey:[1,2]\n"
								"key_key.jump:key.keyboard.space\n";

	void write(const QString& path, const QByteArray& contents)
	{
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
		file.write(contents);
	}

	QByteArray read(const QString& path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}

	QString valueOf(const GameOptions& options, const QString& key)
	{
		for (int row = 0; row < options.rowCount(); ++row) {
			const QModelIndex index = options.index(row);
			if (options.data(index, GameOptions::KeyRole).toString() == key) {
				return options.data(index, GameOptions::ValueRole).toString();
			}
		}
		return {};
	}
} // namespace

class GameOptionsTest : public QObject
{
	Q_OBJECT

  private slots:
	void init()
	{
		QVERIFY(m_dir.isValid());
		m_path = m_dir.filePath("options.txt");
		write(m_path, kOptions);
	}

	void setValueRewritesOnlyThatLine()
	{
		GameOptions options(m_path);
		QSignalSpy changed(&options, &QAbstractItemModel::dataChanged);
		QSignalSpy reset(&options, &QAbstractItemModel::modelReset);

		QVERIFY(options.setValue("renderDistance", "20"));
		QCOMPARE(read(m_path), QByteArray("version:3955\n"
										  "autoJump:false\n"
										  "renderDistance:20\n"
										  "fov:0.0\n"
										  "someModsOwnKey:[1,2]\n"
										  "key_key.jump:key.keyboard.space\n"));
		QCOMPARE(valueOf(options, "renderDistance"), QStringLiteral("20"));
		QCOMPARE(changed.count(), 1);
		QCOMPARE(changed.first().at(0).toModelIndex().row(), 1);
		QCOMPARE(reset.count(), 0);
	}

	void missingKeyIsAppended()
	{
		GameOptions options(m_path);
		QVERIFY(options.setValue("fullscreen", "true"));
		QVERIFY(read(m_path).endsWith("key_key.jump:key.keyboard.space\n"
									  "fullscreen:true\n"));
		QCOMPARE(valueOf(options, "fullscreen"), QStringLiteral("true"));
	}

	// The game may have rewritten the file since the page loaded it; a
	// change from the launcher must not undo that.
	void keepsWhatTheGameWroteSinceLoading()
	{
		GameOptions options(m_path);
		write(m_path, "version:3955\n"
					  "autoJump:true\n"
					  "renderDistance:12\n"
					  "fov:0.0\n"
					  "someModsOwnKey:[1,2]\n"
					  "key_key.jump:key.keyboard.space\n");
		QVERIFY(options.setValue("fov", "0.5"));
		const QByteArray now = read(m_path);
		QVERIFY(now.contains("autoJump:true\n"));
		QVERIFY(now.contains("fov:0.5\n"));
		QCOMPARE(valueOf(options, "autoJump"), QStringLiteral("true"));
	}

	void differentKeysOnDiskResetTheModel()
	{
		GameOptions options(m_path);
		write(m_path, "version:3955\nlang:en_us\n");
		QSignalSpy reset(&options, &QAbstractItemModel::modelReset);
		QVERIFY(options.setValue("lang", "tr_tr"));
		QCOMPARE(reset.count(), 1);
		QCOMPARE(options.rowCount(), 1);
		QCOMPARE(valueOf(options, "lang"), QStringLiteral("tr_tr"));
	}

	void refusesWhatCannotBeOneLine()
	{
		GameOptions options(m_path);
		QVERIFY(!options.setValue("", "1"));
		QVERIFY(!options.setValue("a:b", "1"));
		QVERIFY(!options.setValue("version", "1"));
		QVERIFY(!options.setValue("fov", "1\nversion:1"));
		QVERIFY(!options.setValue("fov\r", "1"));
		QCOMPARE(read(m_path), kOptions);
	}

	void refusesWithoutAnOptionsFile()
	{
		const QString missing = m_dir.filePath("never-played/options.txt");
		GameOptions options(missing);
		QVERIFY(!options.isLoaded());
		QVERIFY(!options.setValue("renderDistance", "8"));
		QVERIFY(!QFile::exists(missing));
	}

  private:
	QTemporaryDir m_dir;
	QString m_path;
};

QTEST_GUILESS_MAIN(GameOptionsTest)

#include "GameOptions_test.moc"
