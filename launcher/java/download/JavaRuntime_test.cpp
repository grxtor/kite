/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include "java/download/JavaRuntime.h"

using namespace JavaDownload;

class JavaRuntimeTest : public QObject
{
	Q_OBJECT

  private slots:
	void segmentsMustBeOneName_data();
	void segmentsMustBeOneName();
	void relativePathsMustStayClean_data();
	void relativePathsMustStayClean();
	void insideMeansStrictlyBelow();
	void joinedPathsStayInTheRoot();
	void linksMayNotLeaveTheRoot_data();
	void linksMayNotLeaveTheRoot();
};

void JavaRuntimeTest::segmentsMustBeOneName_data()
{
	QTest::addColumn<QString>("segment");
	QTest::addColumn<bool>("safe");
	QTest::newRow("name") << "jdk-21.0.1+12" << true;
	QTest::newRow("vendor") << "adoptium" << true;
	QTest::newRow("empty") << "" << false;
	QTest::newRow("dot") << "." << false;
	QTest::newRow("dotdot") << ".." << false;
	QTest::newRow("slash") << "a/b" << false;
	QTest::newRow("up and out") << "../.." << false;
	QTest::newRow("backslash") << "a\\b" << false;
	QTest::newRow("drive") << "C:" << false;
	QTest::newRow("absolute") << "/etc" << false;
}

void JavaRuntimeTest::segmentsMustBeOneName()
{
	QFETCH(QString, segment);
	QFETCH(bool, safe);
	QCOMPARE(isSafeSegment(segment), safe);
}

void JavaRuntimeTest::relativePathsMustStayClean_data()
{
	QTest::addColumn<QString>("path");
	QTest::addColumn<bool>("safe");
	QTest::newRow("file") << "bin/java" << true;
	QTest::newRow("deep") << "lib/security/default.policy" << true;
	QTest::newRow("spaces") << "legal/java base/LICENSE" << true;
	QTest::newRow("empty") << "" << false;
	QTest::newRow("absolute") << "/etc/passwd" << false;
	QTest::newRow("trailing slash") << "bin/" << false;
	QTest::newRow("doubled slash") << "bin//java" << false;
	QTest::newRow("dot") << "./bin/java" << false;
	QTest::newRow("escape") << "../../x" << false;
	QTest::newRow("escape inside") << "bin/../../x" << false;
	QTest::newRow("windows escape") << "bin\\..\\x" << false;
}

void JavaRuntimeTest::relativePathsMustStayClean()
{
	QFETCH(QString, path);
	QFETCH(bool, safe);
	QCOMPARE(isSafeRelativePath(path), safe);
}

void JavaRuntimeTest::insideMeansStrictlyBelow()
{
	QVERIFY(isInside("/data/java", "/data/java/azul/zulu-8"));
	QVERIFY(isInside("/data/java/", "/data/java/azul"));
	QVERIFY(!isInside("/data/java", "/data/java"));
	QVERIFY(!isInside("/data/java", "/data/java-other/x"));
	QVERIFY(!isInside("/data/java", "/data/java/../x"));
	QVERIFY(!isInside("/data/java", "/etc"));
}

void JavaRuntimeTest::joinedPathsStayInTheRoot()
{
	QCOMPARE(joinInside("/data/java/azul/zulu-8", "bin/java"),
			 QString("/data/java/azul/zulu-8/bin/java"));
	QVERIFY(joinInside("/data/java/azul/zulu-8", "../../x").isEmpty());
	QVERIFY(joinInside("/data/java/azul/zulu-8", "/etc/passwd").isEmpty());
	QVERIFY(joinInside("/data/java/azul/zulu-8", "").isEmpty());
	QVERIFY(joinInside("/data/java/azul/zulu-8", "a/../../b").isEmpty());
}

void JavaRuntimeTest::linksMayNotLeaveTheRoot_data()
{
	QTest::addColumn<QString>("linkPath");
	QTest::addColumn<QString>("target");
	QTest::addColumn<bool>("safe");
	QTest::newRow("sibling") << "/data/java/azul/zulu-8/lib/a" << "b" << true;
	QTest::newRow("up one") << "/data/java/azul/zulu-8/lib/a"
							<< "../Home/lib/libjli.dylib" << true;
	QTest::newRow("up to root file") << "/data/java/azul/zulu-8/lib/a"
									 << "../bin/java" << true;
	QTest::newRow("empty") << "/data/java/azul/zulu-8/lib/a" << "" << false;
	QTest::newRow("absolute") << "/data/java/azul/zulu-8/lib/a"
							  << "/etc/passwd" << false;
	QTest::newRow("too far up") << "/data/java/azul/zulu-8/lib/a"
								<< "../../../x" << false;
	QTest::newRow("the root itself") << "/data/java/azul/zulu-8/lib/a"
									 << ".." << false;
	QTest::newRow("up after down") << "/data/java/azul/zulu-8/a"
								   << "lib/link/../../.." << false;
	QTest::newRow("up after down, inside")
		<< "/data/java/azul/zulu-8/a" << "lib/../bin/java" << false;
	QTest::newRow("drive") << "/data/java/azul/zulu-8/a" << "C:/x" << false;
}

void JavaRuntimeTest::linksMayNotLeaveTheRoot()
{
	QFETCH(QString, linkPath);
	QFETCH(QString, target);
	QFETCH(bool, safe);
	QCOMPARE(isSafeLinkTarget("/data/java/azul/zulu-8", linkPath, target),
			 safe);
}

QTEST_GUILESS_MAIN(JavaRuntimeTest)
#include "JavaRuntime_test.moc"
