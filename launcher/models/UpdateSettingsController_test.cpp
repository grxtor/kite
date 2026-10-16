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

#include "models/UpdateSettingsController.h"

namespace
{
	// A stand-in updater that remembers what it was told.
	struct FakeUpdater {
		bool autoCheck = true;
		double interval = 24 * 3600.0;
		int checks = 0;

		UpdateSettingsController::Backend backend()
		{
			return {[this] { return autoCheck; },
					[this](bool v) { autoCheck = v; },
					[this] { return interval; },
					[this](double v) { interval = v; },
					[this] { ++checks; }};
		}
	};
} // namespace

class UpdateSettingsControllerTest : public QObject
{
	Q_OBJECT

  private slots:
	void isUnavailableWithoutAnUpdater();
	void readsTheUpdatersSettings();
	void writesHoursAsSeconds();
	void zeroHoursMeansOnlyAtStartup();
	void checkNowReachesTheUpdater();
	void anIncompleteBackendStaysUnavailable();
};

void UpdateSettingsControllerTest::isUnavailableWithoutAnUpdater()
{
	UpdateSettingsController controller;
	QVERIFY(!controller.available());
	QVERIFY(!controller.autoCheck());
	QCOMPARE(controller.intervalHours(), 0);
	// Nothing to talk to: all of these must be harmless.
	controller.setAutoCheck(true);
	controller.setIntervalHours(6);
	controller.checkNow();
	QVERIFY(!controller.autoCheck());
}

void UpdateSettingsControllerTest::readsTheUpdatersSettings()
{
	FakeUpdater updater;
	UpdateSettingsController controller;
	QSignalSpy changed(&controller, &UpdateSettingsController::changed);
	controller.setBackend(updater.backend());
	QVERIFY(controller.available());
	QVERIFY(controller.autoCheck());
	QCOMPARE(controller.intervalHours(), 24);
	QCOMPARE(changed.count(), 1);

	controller.setAutoCheck(false);
	QVERIFY(!updater.autoCheck);
	QCOMPARE(changed.count(), 2);
	// Setting what is already set says nothing.
	controller.setAutoCheck(false);
	QCOMPARE(changed.count(), 2);
}

void UpdateSettingsControllerTest::writesHoursAsSeconds()
{
	FakeUpdater updater;
	UpdateSettingsController controller;
	controller.setBackend(updater.backend());
	controller.setIntervalHours(6);
	QCOMPARE(updater.interval, 6 * 3600.0);
	QCOMPARE(controller.intervalHours(), 6);
	// An interval that is not a whole number of hours is rounded.
	updater.interval = 5400;
	QCOMPARE(controller.intervalHours(), 2);
}

void UpdateSettingsControllerTest::zeroHoursMeansOnlyAtStartup()
{
	FakeUpdater updater;
	UpdateSettingsController controller;
	controller.setBackend(updater.backend());
	controller.setIntervalHours(0);
	QCOMPARE(updater.interval, 0.0);
	// Never negative.
	controller.setIntervalHours(-4);
	QCOMPARE(updater.interval, 0.0);
}

void UpdateSettingsControllerTest::checkNowReachesTheUpdater()
{
	FakeUpdater updater;
	UpdateSettingsController controller;
	controller.setBackend(updater.backend());
	controller.checkNow();
	controller.checkNow();
	QCOMPARE(updater.checks, 2);
}

void UpdateSettingsControllerTest::anIncompleteBackendStaysUnavailable()
{
	UpdateSettingsController controller;
	UpdateSettingsController::Backend half;
	half.checkNow = [] {};
	controller.setBackend(half);
	QVERIFY(!controller.available());
	controller.checkNow();
}

QTEST_GUILESS_MAIN(UpdateSettingsControllerTest)
#include "UpdateSettingsController_test.moc"
