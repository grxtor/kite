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

#include <QSignalSpy>
#include <QTest>

#include "java/JavaUtils.h"
#include "models/JavaDownloadController.h"

namespace
{
	// Fixtures are written with single quotes so they stay readable as C++.
	QByteArray json(const char* text)
	{
		return QByteArray(text).replace('\'', '"');
	}

	// Trimmed copies of what the meta server serves.
	const QByteArray kVersionIndex = json(
		"{"
		"'name': 'Eclipse Temurin',"
		"'uid': 'net.adoptium.java',"
		"'versions': ["
		"{'version': 'java25', 'sha256': 'aa', 'releaseTime': '2025-09-16T00:00:00+00:00', 'recommended': false},"
		"{'version': 'java21', 'sha256': 'bb', 'releaseTime': '2023-09-19T00:00:00+00:00', 'recommended': true},"
		"{'version': 'java8', 'sha256': 'cc', 'releaseTime': '2014-03-18T00:00:00+00:00'}"
		"]"
		"}");

	const QByteArray kRuntimes = json(
		"{"
		"'runtimes': ["
		"{'name': 'jdk', 'url': 'https://x/a.tar.gz', 'vendor': 'adoptium',"
		"'runtimeOS': 'mac-os-arm64', 'packageType': 'jdk',"
		"'checksum': {'type': 'sha256', 'hash': '1234'},"
		"'version': {'major': 21, 'minor': 0, 'security': 1, 'build': 12}},"
		"{'name': 'jdk', 'url': 'https://x/b.zip', 'vendor': 'adoptium',"
		"'runtimeOS': 'windows-x64', 'packageType': 'jdk',"
		"'version': {'major': 21, 'minor': 0, 'security': 1, 'build': 12}},"
		"{'name': 'jdk', 'url': 'https://x/c.tar.gz', 'vendor': 'adoptium',"
		"'runtimeOS': 'mac-os-arm64', 'packageType': 'jdk',"
		"'version': {'major': 21, 'minor': 0, 'security': 2}}"
		"]"
		"}");
} // namespace

class JavaDownloadControllerTest : public QObject
{
	Q_OBJECT

  private slots:
	void versionLabelsAreReadable();
	void parsesTheVersionIndex();
	void rejectsABrokenVersionIndex();
	void preselectsTheRecommendedVersion();
	void keepsOnlyBuildsForThisComputer();
	void rejectsBrokenRuntimeDetails();
	void installsUnderTheVendorFolder();
	void startsIdle();
	void downloadWithoutAPickDoesNothing();
};

void JavaDownloadControllerTest::versionLabelsAreReadable()
{
	QCOMPARE(JavaDownloadController::versionLabel("java21"), QString("Java 21"));
	QCOMPARE(JavaDownloadController::versionLabel("java8"), QString("Java 8"));
	QCOMPARE(JavaDownloadController::versionLabel("17.0.1"), QString("17.0.1"));
}

void JavaDownloadControllerTest::parsesTheVersionIndex()
{
	QString error;
	const auto rows = JavaDownloadController::versionsFromJson(
		kVersionIndex, "net.adoptium.java", &error);
	QVERIFY(error.isEmpty());
	QCOMPARE(rows.size(), 3);
	QCOMPARE(rows.at(1).toMap().value("value").toString(), QString("java21"));
	QCOMPARE(rows.at(1).toMap().value("label").toString(), QString("Java 21"));
	QCOMPARE(rows.at(1).toMap().value("recommended").toBool(), true);
	QCOMPARE(rows.at(2).toMap().value("recommended").toBool(), false);
}

void JavaDownloadControllerTest::rejectsABrokenVersionIndex()
{
	QString error;
	QVERIFY(JavaDownloadController::versionsFromJson("<html>502</html>", "x",
													 &error)
				.isEmpty());
	QVERIFY(!error.isEmpty());

	error.clear();
	QVERIFY(JavaDownloadController::versionsFromJson("[]", "x", &error)
				.isEmpty());
	QVERIFY(!error.isEmpty());
}

void JavaDownloadControllerTest::preselectsTheRecommendedVersion()
{
	QString error;
	const auto rows = JavaDownloadController::versionsFromJson(
		kVersionIndex, "net.adoptium.java", &error);
	QCOMPARE(JavaDownloadController::defaultVersion(rows), QString("java21"));
	// Nothing recommended: the first one. Nothing at all: nothing.
	QVariantList plain = {rows.at(0), rows.at(2)};
	QCOMPARE(JavaDownloadController::defaultVersion(plain), QString("java25"));
	QVERIFY(JavaDownloadController::defaultVersion({}).isEmpty());
}

void JavaDownloadControllerTest::keepsOnlyBuildsForThisComputer()
{
	QString error;
	const auto macBuilds = JavaDownloadController::runtimesFromJson(
		kRuntimes, "mac-os-arm64", &error);
	QVERIFY(error.isEmpty());
	QCOMPARE(macBuilds.size(), 2);
	QCOMPARE(macBuilds.at(0).version.toString(), QString("21.0.1+12"));
	QCOMPARE(macBuilds.at(0).checksumHash, QString("1234"));
	QCOMPARE(macBuilds.at(1).version.toString(), QString("21.0.2"));

	// Nothing for a platform the vendor does not build for: empty, no error.
	QVERIFY(JavaDownloadController::runtimesFromJson(kRuntimes, "linux-riscv64",
													 &error)
				.isEmpty());
	QVERIFY(error.isEmpty());
}

void JavaDownloadControllerTest::rejectsBrokenRuntimeDetails()
{
	QString error;
	QVERIFY(JavaDownloadController::runtimesFromJson("nope", "mac-os-arm64",
													 &error)
				.isEmpty());
	QVERIFY(!error.isEmpty());
}

void JavaDownloadControllerTest::installsUnderTheVendorFolder()
{
	QString error;
	const auto builds = JavaDownloadController::runtimesFromJson(
		kRuntimes, "mac-os-arm64", &error);
	QVERIFY(!builds.isEmpty());
	QCOMPARE(JavaDownloadController::targetDirFor(builds.first()),
			 JavaUtils::managedJavaRoot() + "/adoptium/jdk-21.0.1+12");
}

void JavaDownloadControllerTest::startsIdle()
{
	JavaDownloadController controller;
	QVERIFY(!controller.vendors().isEmpty());
	QVERIFY(!controller.loading());
	QVERIFY(!controller.downloading());
	QCOMPARE(controller.build(), -1);
	QVERIFY(!controller.alreadyInstalled());
	QVERIFY(controller.error().isEmpty());
}

void JavaDownloadControllerTest::downloadWithoutAPickDoesNothing()
{
	JavaDownloadController controller;
	QSignalSpy changed(&controller, &JavaDownloadController::downloadingChanged);
	controller.download();
	controller.selectBuild(3);
	QCOMPARE(changed.count(), 0);
	QCOMPARE(controller.build(), -1);
}

QTEST_GUILESS_MAIN(JavaDownloadControllerTest)
#include "JavaDownloadController_test.moc"
