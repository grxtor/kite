/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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
#include <QTemporaryDir>
#include <QTest>

#include "minecraft/PresetDefaults.h"

namespace
{
	constexpr quint64 kGiB = 1024;
}

class PresetDefaultsTest : public QObject
{
	Q_OBJECT

  private slots:
	void maxMemory_data()
	{
		QTest::addColumn<quint64>("ramMiB");
		QTest::addColumn<int>("expected");
		QTest::newRow("2 GiB is half") << 2 * kGiB << 1024;
		QTest::newRow("3 GiB rounds down") << 3 * kGiB << 1536;
		QTest::newRow("tiny machines get the floor") << 1 * kGiB << 1024;
		QTest::newRow("4 GiB") << 4 * kGiB << 2048;
		QTest::newRow("8 GiB") << 8 * kGiB << 3072;
		QTest::newRow("12 GiB") << 12 * kGiB << 3072;
		QTest::newRow("16 GiB") << 16 * kGiB << 4096;
		QTest::newRow("32 GiB") << 32 * kGiB << 6144;
		QTest::newRow("64 GiB") << 64 * kGiB << 6144;
	}
	void maxMemory()
	{
		QFETCH(quint64, ramMiB);
		QFETCH(int, expected);
		QCOMPARE(PresetDefaults::maxMemoryMiB(ramMiB), expected);
		QVERIFY(quint64(PresetDefaults::maxMemoryMiB(ramMiB)) <=
				qMax(ramMiB / 2, quint64(1024)));
	}

	void legacyOptions_data()
	{
		QTest::addColumn<QString>("version");
		QTest::addColumn<bool>("legacy");
		QTest::newRow("1.8.9") << "1.8.9" << true;
		QTest::newRow("1.12.2") << "1.12.2" << true;
		QTest::newRow("1.7") << "1.7" << true;
		QTest::newRow("beta") << "b1.7.3" << true;
		QTest::newRow("alpha") << "a1.2.6" << true;
		QTest::newRow("empty") << "" << true;
		QTest::newRow("1.13") << "1.13" << false;
		QTest::newRow("1.16.5") << "1.16.5" << false;
		QTest::newRow("1.21.11") << "1.21.11" << false;
		QTest::newRow("26.3") << "26.3" << false;
		QTest::newRow("old snapshot") << "17w31a" << true;
		QTest::newRow("first 1.13 snapshot") << "17w43a" << false;
		QTest::newRow("new snapshot") << "24w14a" << false;
	}
	void legacyOptions()
	{
		QFETCH(QString, version);
		QFETCH(bool, legacy);
		QCOMPARE(PresetDefaults::usesLegacyOptions(version), legacy);
	}

	void writesKeyValueLines()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		/* The game folder does not exist yet. */
		const QString root = dir.filePath("minecraft");
		QVERIFY(PresetDefaults::writeOptions(
			root, {{"renderDistance", "8"}, {"maxFps", "260"}}));

		QFile file(root + "/options.txt");
		QVERIFY(file.open(QIODevice::ReadOnly));
		QCOMPARE(file.readAll(), QByteArray("maxFps:260\nrenderDistance:8\n"));
	}

	void neverReplacesOptions()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QFile existing(dir.filePath("options.txt"));
		QVERIFY(existing.open(QIODevice::WriteOnly));
		existing.write("mine:1\n");
		existing.close();

		QVERIFY(!PresetDefaults::writeOptions(dir.path(), {{"maxFps", "260"}}));
		QVERIFY(existing.open(QIODevice::ReadOnly));
		QCOMPARE(existing.readAll(), QByteArray("mine:1\n"));
	}

	void writesNothingWithoutOptions()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QVERIFY(!PresetDefaults::writeOptions(dir.path(), {}));
		QVERIFY(!QFile::exists(dir.filePath("options.txt")));
	}
};

QTEST_GUILESS_MAIN(PresetDefaultsTest)

#include "PresetDefaults_test.moc"
