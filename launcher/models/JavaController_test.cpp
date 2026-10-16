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
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "java/JavaArgs.h"
#include "models/JavaController.h"

namespace
{
	/* Makes <root>/<relative> as an (empty) file, with its folders. */
	void touch(const QString& root, const QString& relative)
	{
		const QString path = QDir(root).filePath(relative);
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write("x");
	}
} // namespace

class JavaControllerTest : public QObject
{
	Q_OBJECT

  private slots:
	void memoryFlagsAreSpotted_data();
	void memoryFlagsAreSpotted();
	void argsWarningIsPlainAndOnlyWhenNeeded();
	void permGenOnlyForLegacyJava();
	void vendorsListTheProviders();
	void describesAGoodCheck();
	void describesABadCheck();
	void testWithoutAPathAnswersWithAnError();
	void scansDownloadedRuntimes();
	void scanOfMissingFolderIsEmpty();
	void removesOnlyRuntimeFolders();
};

void JavaControllerTest::memoryFlagsAreSpotted_data()
{
	QTest::addColumn<QString>("args");
	QTest::addColumn<bool>("flagged");
	QTest::newRow("empty") << QString() << false;
	QTest::newRow("gc only") << "-XX:+UseG1GC" << false;
	QTest::newRow("xmx") << "-Xmx4G" << true;
	QTest::newRow("xms among others") << "-XX:+UseG1GC -Xms512m" << true;
	QTest::newRow("permsize") << "-XX:PermSize=128m" << true;
	QTest::newRow("initial heap") << "-XX:InitialHeapSize=1g" << true;
	QTest::newRow("typo'd max heap") << "-XX-MaxHeapSize=1g" << true;
	QTest::newRow("xss is fine") << "-Xss1M" << false;
}

void JavaControllerTest::memoryFlagsAreSpotted()
{
	QFETCH(QString, args);
	QFETCH(bool, flagged);
	QCOMPARE(JavaArgs::setsMemoryFlags(args), flagged);
	QCOMPARE(!JavaArgs::memoryFlagWarning(args).isEmpty(), flagged);
}

void JavaControllerTest::argsWarningIsPlainAndOnlyWhenNeeded()
{
	JavaController controller;
	QVERIFY(controller.argsWarning("-XX:+UseG1GC").isEmpty());
	const QString warning = controller.argsWarning("-Xmx2G");
	QVERIFY(!warning.isEmpty());
	QVERIFY(!warning.contains(QLatin1Char('\n')));
}

void JavaControllerTest::permGenOnlyForLegacyJava()
{
	JavaController controller;
	QVERIFY(controller.requiresPermGen("1.7.0_80"));
	QVERIFY(controller.requiresPermGen("1.6.0_45"));
	QVERIFY(!controller.requiresPermGen("1.8.0_292"));
	QVERIFY(!controller.requiresPermGen("17.0.2"));
	// Nothing known yet: do not show a row for it.
	QVERIFY(!controller.requiresPermGen(""));
	QVERIFY(!controller.requiresPermGen("not a version"));
}

void JavaControllerTest::vendorsListTheProviders()
{
	JavaController controller;
	const auto vendors = controller.vendors();
	QVERIFY(vendors.size() >= 2);
	const auto first = vendors.first().toMap();
	QVERIFY(!first.value("value").toString().isEmpty());
	QVERIFY(!first.value("label").toString().isEmpty());
}

void JavaControllerTest::describesAGoodCheck()
{
	JavaCheckResult result;
	result.path = "/opt/java/bin/java";
	result.validity = JavaCheckResult::Validity::Valid;
	result.javaVersion = "17.0.2";
	result.realPlatform = "aarch64";
	result.javaVendor = "Eclipse Adoptium";
	result.errorLog = "  WARNING: something\n";

	const auto map = JavaController::describeCheck(result);
	QCOMPARE(map.value("ok").toBool(), true);
	QCOMPARE(map.value("version").toString(), QString("17.0.2"));
	QCOMPARE(map.value("architecture").toString(), QString("aarch64"));
	QCOMPARE(map.value("vendor").toString(), QString("Eclipse Adoptium"));
	QCOMPARE(map.value("warnings").toString(), QString("WARNING: something"));
	QCOMPARE(map.value("requiresPermGen").toBool(), false);
}

void JavaControllerTest::describesABadCheck()
{
	JavaCheckResult result;
	result.path = "/nope/java";
	result.errorLog = "boom";
	const auto map = JavaController::describeCheck(result);
	QCOMPARE(map.value("ok").toBool(), false);
	// A failed run reports through "error", never as a warning.
	QVERIFY(map.value("warnings").toString().isEmpty());
	QVERIFY(!map.value("requiresPermGen").toBool());
}

void JavaControllerTest::testWithoutAPathAnswersWithAnError()
{
	JavaController controller;
	QSignalSpy finished(&controller, &JavaController::testFinished);
	controller.test("global", "   ", "", 512, 2048, 64);
	QCOMPARE(finished.count(), 1);
	QCOMPARE(finished.at(0).at(0).toString(), QString("global"));
	const auto result = finished.at(0).at(1).toMap();
	QCOMPARE(result.value("ok").toBool(), false);
	QVERIFY(!result.value("error").toString().isEmpty());
	// Idle again, so the next test can start.
	QVERIFY(controller.testingToken().isEmpty());
}

void JavaControllerTest::scansDownloadedRuntimes()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	touch(dir.path(), "adoptium/jdk-21.0.1/Contents/Home/bin/java");
	touch(dir.path(), "adoptium/jdk-17.0.9/bin/java");
	touch(dir.path(), "azul/zulu-8/bin/java");
	// Unfinished download: a folder without a binary is not a runtime.
	QDir().mkpath(dir.filePath("azul/zulu-broken/lib"));
	// A "java" that is not under bin/ does not count either.
	touch(dir.path(), "azul/zulu-odd/java");

	const auto rows = JavaController::scanManaged(dir.path());
	QCOMPARE(rows.size(), 3);
	// Vendors in order, newest name first inside one vendor.
	QCOMPARE(rows.at(0).toMap().value("vendor").toString(),
			 QString("adoptium"));
	QCOMPARE(rows.at(0).toMap().value("name").toString(),
			 QString("jdk-21.0.1"));
	QCOMPARE(rows.at(1).toMap().value("name").toString(),
			 QString("jdk-17.0.9"));
	QCOMPARE(rows.at(2).toMap().value("vendor").toString(), QString("azul"));
	QVERIFY(rows.at(0).toMap().value("javaPath").toString().endsWith(
		"Contents/Home/bin/java"));
	QCOMPARE(rows.at(1).toMap().value("root").toString(),
			 dir.filePath("adoptium/jdk-17.0.9"));
}

void JavaControllerTest::scanOfMissingFolderIsEmpty()
{
	QTemporaryDir dir;
	QVERIFY(JavaController::scanManaged(dir.filePath("nothing")).isEmpty());
}

void JavaControllerTest::removesOnlyRuntimeFolders()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	touch(dir.path(), "azul/zulu-8/bin/java");
	touch(dir.path(), "azul/zulu-11/bin/java");

	// Not a runtime folder: the vendor folder, the root, outside, nothing.
	QVERIFY(!JavaController::removeManagedUnder(dir.path(),
												dir.filePath("azul"))
				 .isEmpty());
	QVERIFY(!JavaController::removeManagedUnder(dir.path(), dir.path())
				 .isEmpty());
	QVERIFY(!JavaController::removeManagedUnder(dir.path(), "/tmp").isEmpty());
	QVERIFY(!JavaController::removeManagedUnder(dir.path(), "").isEmpty());
	QVERIFY(!JavaController::removeManagedUnder(
				 dir.path(), dir.filePath("azul/zulu-8/bin"))
				 .isEmpty());
	QVERIFY(QFileInfo::exists(dir.filePath("azul/zulu-8/bin/java")));
	QVERIFY(QFileInfo::exists(dir.filePath("azul/zulu-11/bin/java")));

	// A real one goes, its sibling stays.
	QVERIFY(JavaController::removeManagedUnder(dir.path(),
											   dir.filePath("azul/zulu-8"))
				.isEmpty());
	QVERIFY(!QFileInfo::exists(dir.filePath("azul/zulu-8")));
	QVERIFY(QFileInfo::exists(dir.filePath("azul/zulu-11/bin/java")));
}

QTEST_GUILESS_MAIN(JavaControllerTest)
#include "JavaController_test.moc"
