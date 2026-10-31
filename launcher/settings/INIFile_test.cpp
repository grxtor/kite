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

#include <QTest>
#include "TestUtil.h"

#include "settings/INIFile.h"

class IniFileTest : public QObject
{
	Q_OBJECT
  private slots:
	void initTestCase() {}
	void cleanupTestCase() {}

	void test_Escape_data()
	{
		QTest::addColumn<QString>("through");

		QTest::newRow("unix path") << "/abc/def/ghi/jkl";
		QTest::newRow("windows path")
			<< "C:\\Program files\\terrible\\name\\of something\\";
		QTest::newRow("Plain text") << "Lorem ipsum dolor sit amet.";
		QTest::newRow("Escape sequences")
			<< "Lorem\n\t\n\\n\\tAAZ\nipsum dolor\n\nsit amet.";
		QTest::newRow("Escape sequences 2") << "\"\n\n\"";
		QTest::newRow("Hashtags") << "some data#something";
	}
	void test_Escape()
	{
		QFETCH(QString, through);

		QString there = INIFile::escape(through);
		QString back = INIFile::unescape(there);

		QCOMPARE(back, through);
	}

	void test_SaveLoad()
	{
		QString a = "a";
		QString b = "a\nb\t\n\\\\\\C:\\Program files\\terrible\\name\\of "
					"something\\#thisIsNotAComment";
		QString filename = "test_SaveLoad.ini";

		INIFile f;
		f.set("a", a);
		f.set("b", b);
		f.saveFile(filename);

		INIFile f2;
		f2.loadFile(filename);
		QCOMPARE(a, f2.get("a", "NOT SET").toString());
		QCOMPARE(b, f2.get("b", "NOT SET").toString());
	}
};

QTEST_GUILESS_MAIN(IniFileTest)

#include "INIFile_test.moc"
