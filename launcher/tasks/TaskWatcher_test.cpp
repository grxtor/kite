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

#include "tasks/TaskWatcher.h"

namespace
{
	/* A Task that does nothing on its own - executeTask() is a no-op -
	 * so the test can drive status/progress/success/failure by hand and
	 * check that TaskWatcher mirrors exactly what was driven. */
	class ScriptedTask : public Task
	{
		Q_OBJECT
	  public:
		using Task::Task;

		void driveStatus(const QString& status)
		{
			setStatus(status);
		}
		void driveProgress(qint64 current, qint64 total)
		{
			setProgress(current, total);
		}
		void driveSuccess()
		{
			emitSucceeded();
		}
		void driveFailure(const QString& reason)
		{
			emitFailed(reason);
		}

	  protected:
		void executeTask() override {}
	};
} // namespace

class TaskWatcherTest : public QObject
{
	Q_OBJECT

  private slots:

	void test_MirrorsStatusAndProgress()
	{
		auto* task = new ScriptedTask();
		TaskWatcher watcher{Task::Ptr(task)};

		QSignalSpy statusSpy(&watcher, &TaskWatcher::statusChanged);
		QSignalSpy progressSpy(&watcher, &TaskWatcher::progressChanged);

		QCOMPARE(watcher.isRunning(), false);
		task->start();
		QCOMPARE(watcher.isRunning(), true);

		task->driveStatus("Downloading pack.mrpack");
		QCOMPARE(watcher.status(), QString("Downloading pack.mrpack"));
		QCOMPARE(statusSpy.count(), 1);

		task->driveProgress(50, 100);
		QCOMPARE(watcher.progress(), 0.5);
		QCOMPARE(progressSpy.count(), 1);
	}

	void test_IndeterminateProgressIsMinusOne()
	{
		auto* task = new ScriptedTask();
		TaskWatcher watcher{Task::Ptr(task)};
		task->start();

		task->driveProgress(0, 0);
		QCOMPARE(watcher.progress(), -1.0);
	}

	void test_ProgressNotifyIsThrottled()
	{
		auto* task = new ScriptedTask();
		TaskWatcher watcher{Task::Ptr(task)};
		task->start();

		QSignalSpy progressSpy(&watcher, &TaskWatcher::progressChanged);

		/* The first update always goes straight through - there is
		 * nothing to coalesce with yet. */
		task->driveProgress(1, 100);
		QCOMPARE(progressSpy.count(), 1);

		/* A burst right behind it must not turn into a burst of NOTIFY
		 * firings; the throttle should coalesce them into (at most) one
		 * more. */
		for (int i = 2; i <= 20; ++i) {
			task->driveProgress(i, 100);
		}
		QVERIFY(progressSpy.count() <= 2);

		/* But the last value reported is never lost - it shows up once
		 * the throttle window has had time to flush. */
		QTRY_COMPARE_WITH_TIMEOUT(watcher.progress(), 0.20, 1000);
	}

	void test_Succeeds()
	{
		auto* task = new ScriptedTask();
		TaskWatcher watcher{Task::Ptr(task)};
		QSignalSpy finishedSpy(&watcher, &TaskWatcher::finished);

		task->start();
		task->driveProgress(3, 10);
		task->driveSuccess();

		QCOMPARE(watcher.isRunning(), false);
		QCOMPARE(watcher.succeeded(), true);
		QCOMPARE(watcher.failed(), false);
		QCOMPARE(watcher.progress(), 1.0);
		QCOMPARE(finishedSpy.count(), 1);
		QCOMPARE(finishedSpy.at(0).at(0).toBool(), true);
	}

	void test_Fails()
	{
		auto* task = new ScriptedTask();
		TaskWatcher watcher{Task::Ptr(task)};
		QSignalSpy finishedSpy(&watcher, &TaskWatcher::finished);

		task->start();
		task->driveFailure("network is on fire");

		QCOMPARE(watcher.isRunning(), false);
		QCOMPARE(watcher.succeeded(), false);
		QCOMPARE(watcher.failed(), true);
		QCOMPARE(watcher.error(), QString("network is on fire"));
		QCOMPARE(finishedSpy.count(), 1);
		QCOMPARE(finishedSpy.at(0).at(0).toBool(), false);
	}

	void test_TitleAndInstanceIdAreSettableByCreator()
	{
		auto* task = new ScriptedTask();
		TaskWatcher watcher{Task::Ptr(task)};

		QSignalSpy titleSpy(&watcher, &TaskWatcher::titleChanged);
		watcher.setTitle("Installing Vault Hunters");
		QCOMPARE(watcher.title(), QString("Installing Vault Hunters"));
		QCOMPARE(titleSpy.count(), 1);

		QCOMPARE(watcher.instanceId(), QString());
		QSignalSpy idSpy(&watcher, &TaskWatcher::instanceIdChanged);
		watcher.setInstanceId("abc123");
		QCOMPARE(watcher.instanceId(), QString("abc123"));
		QCOMPARE(idSpy.count(), 1);
	}
};

QTEST_GUILESS_MAIN(TaskWatcherTest)

#include "TaskWatcher_test.moc"
