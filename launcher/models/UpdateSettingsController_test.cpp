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

#include <QSignalSpy>
#include <QTest>

#include "models/UpdateSettingsController.h"

namespace
{
	// A stand-in updater that remembers what it was told.
	struct FakeUpdater {
		bool autoCheck = true;
		QDateTime last;
		int checks = 0;
		QString pending;
		bool gamesRunning = false;
		int installs = 0;

		UpdateSettingsController::Backend backend()
		{
			UpdateSettingsController::Backend b;
			b.automaticallyChecks = [this] { return autoCheck; };
			b.setAutomaticallyChecks = [this](bool v) { autoCheck = v; };
			b.lastCheck = [this] { return last; };
			b.checkNow = [this] { ++checks; };
			b.pendingVersion = [this] { return pending; };
			b.canInstallNow = [this] { return !gamesRunning; };
			b.installPendingNow = [this] { ++installs; };
			return b;
		}
	};
} // namespace

class UpdateSettingsControllerTest : public QObject
{
	Q_OBJECT

  private slots:
	void isUnavailableWithoutAnUpdater();
	void readsTheUpdatersSettings();
	void lastCheckIsMillisecondsOrZero();
	void channelAndManager();
	void checkNowReachesTheUpdater();
	void installsAPendingUpdateOnlyWhenNoGameRuns();
	void sparkleNeedsNoPendingFunctions();
	void refreshTellsQml();
	void anIncompleteBackendStaysUnavailable();
};

void UpdateSettingsControllerTest::isUnavailableWithoutAnUpdater()
{
	UpdateSettingsController controller;
	QVERIFY(!controller.available());
	QVERIFY(!controller.autoCheck());
	QCOMPARE(controller.lastCheckedMs(), qint64(0));
	QVERIFY(controller.channel().isEmpty());
	QVERIFY(controller.pendingVersion().isEmpty());
	QVERIFY(!controller.canInstallNow());
	// Nothing to talk to: all of these must be harmless.
	controller.setAutoCheck(true);
	controller.checkNow();
	controller.installPendingNow();
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
	QCOMPARE(changed.count(), 1);

	controller.setAutoCheck(false);
	QVERIFY(!updater.autoCheck);
	QCOMPARE(changed.count(), 2);
	// Setting what is already set says nothing.
	controller.setAutoCheck(false);
	QCOMPARE(changed.count(), 2);
}

void UpdateSettingsControllerTest::lastCheckIsMillisecondsOrZero()
{
	FakeUpdater updater;
	UpdateSettingsController controller;
	controller.setBackend(updater.backend());
	// Never checked: 0, which the page shows as "Never".
	QCOMPARE(controller.lastCheckedMs(), qint64(0));
	updater.last = QDateTime::fromMSecsSinceEpoch(1'793'000'000'000LL);
	QCOMPARE(controller.lastCheckedMs(), 1'793'000'000'000LL);
}

void UpdateSettingsControllerTest::channelAndManager()
{
	FakeUpdater updater;
	UpdateSettingsController controller;
	auto backend = updater.backend();
	controller.setBackend(backend);
	QCOMPARE(controller.channel(), QStringLiteral("stable"));
	QVERIFY(controller.managedBy().isEmpty());

	backend.managedBy = QStringLiteral("Scoop");
	controller.setBackend(backend);
	QCOMPARE(controller.managedBy(), QStringLiteral("Scoop"));
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

void UpdateSettingsControllerTest::installsAPendingUpdateOnlyWhenNoGameRuns()
{
	FakeUpdater updater;
	UpdateSettingsController controller;
	controller.setBackend(updater.backend());

	// Nothing waiting: nothing to install.
	controller.installPendingNow();
	QCOMPARE(updater.installs, 0);

	updater.pending = QStringLiteral("1.3.0");
	updater.gamesRunning = true;
	QCOMPARE(controller.pendingVersion(), QStringLiteral("1.3.0"));
	QVERIFY(!controller.canInstallNow());
	controller.installPendingNow();
	QCOMPARE(updater.installs, 0);

	updater.gamesRunning = false;
	QVERIFY(controller.canInstallNow());
	controller.installPendingNow();
	QCOMPARE(updater.installs, 1);
}

void UpdateSettingsControllerTest::sparkleNeedsNoPendingFunctions()
{
	FakeUpdater updater;
	UpdateSettingsController::Backend backend = updater.backend();
	backend.pendingVersion = nullptr;
	backend.canInstallNow = nullptr;
	backend.installPendingNow = nullptr;
	UpdateSettingsController controller;
	controller.setBackend(backend);
	QVERIFY(controller.available());
	QVERIFY(controller.pendingVersion().isEmpty());
	QVERIFY(controller.canInstallNow());
	controller.installPendingNow();
}

void UpdateSettingsControllerTest::refreshTellsQml()
{
	FakeUpdater updater;
	UpdateSettingsController controller;
	controller.setBackend(updater.backend());
	QSignalSpy changed(&controller, &UpdateSettingsController::changed);
	updater.last = QDateTime::currentDateTime();
	controller.refresh();
	QCOMPARE(changed.count(), 1);
	QVERIFY(controller.lastCheckedMs() > 0);
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
