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

#include "launch/LaunchProgressTracker.h"
#include "tasks/Task.h"

namespace
{

/* Stands in for a real LaunchTask/launch step: nothing to execute, and
 * Task::emitSucceeded()/emitFailed() forwarded to public wrappers so the
 * test can drive the same lifecycle a real launch would, without needing an
 * instance, launch steps, or a real launch. */
class FakeTask : public Task
{
	Q_OBJECT
  public:
	using Task::Task;

	void succeed()
	{
		emitSucceeded();
	}
	void fail(const QString& reason)
	{
		emitFailed(reason);
	}

  protected:
	void executeTask() override {}
};

} // namespace

class LaunchProgressTrackerTest : public QObject
{
	Q_OBJECT

  private slots:
	void test_idle_byDefault()
	{
		LaunchProgressTracker tracker;
		QCOMPARE(tracker.status(), QString());
		QCOMPARE(tracker.progress(), -1.0);
	}

	void test_watch_relaysStatusAndDeterminateProgress()
	{
		LaunchProgressTracker tracker;
		tracker.setMinIntervalMs(0);
		FakeTask task;
		task.start();
		tracker.watch(&task);

		QSignalSpy changedSpy(&tracker, &LaunchProgressTracker::changed);

		task.setStatus("Downloading assets...");
		QVERIFY(!changedSpy.isEmpty());
		QCOMPARE(tracker.status(), QString("Downloading assets..."));
		// No progress reported yet -- still indeterminate.
		QCOMPARE(tracker.progress(), -1.0);

		task.setProgress(45, 100);
		QCOMPARE(tracker.progress(), 0.45);
	}

	void test_progress_isIndeterminate_whenTotalIsNotPositive()
	{
		LaunchProgressTracker tracker;
		tracker.setMinIntervalMs(0);
		FakeTask task;
		task.start();
		tracker.watch(&task);

		task.setProgress(10, 0);
		QCOMPARE(tracker.progress(), -1.0);
	}

	void test_taskFinishing_goesBackToIdle()
	{
		LaunchProgressTracker tracker;
		tracker.setMinIntervalMs(0);
		FakeTask task;
		task.start();
		tracker.watch(&task);

		task.setStatus("Downloading assets...");
		task.setProgress(50, 100);
		QVERIFY(!tracker.status().isEmpty());

		QSignalSpy changedSpy(&tracker, &LaunchProgressTracker::changed);
		task.succeed();

		QVERIFY(!changedSpy.isEmpty());
		QCOMPARE(tracker.status(), QString());
		QCOMPARE(tracker.progress(), -1.0);
	}

	void test_taskFailing_alsoGoesBackToIdle()
	{
		LaunchProgressTracker tracker;
		tracker.setMinIntervalMs(0);
		FakeTask task;
		task.start();
		tracker.watch(&task);
		task.setStatus("Downloading assets...");

		task.fail("network error");

		QCOMPARE(tracker.status(), QString());
		QCOMPARE(tracker.progress(), -1.0);
	}

	void test_watch_switchesToTheNewTask_ignoringTheOldOne()
	{
		LaunchProgressTracker tracker;
		tracker.setMinIntervalMs(0);
		FakeTask first;
		first.start();
		tracker.watch(&first);
		first.setStatus("First step");

		FakeTask second;
		second.start();
		tracker.watch(&second);

		// The old task no longer drives the tracker.
		first.setStatus("Should be ignored");
		QCOMPARE(tracker.status(), QString("First step"));

		second.setStatus("Second step");
		QCOMPARE(tracker.status(), QString("Second step"));
	}

	void test_watchedTaskDestroyed_goesBackToIdle()
	{
		LaunchProgressTracker tracker;
		tracker.setMinIntervalMs(0);
		auto* task = new FakeTask();
		task->start();
		tracker.watch(task);
		task->setStatus("Downloading assets...");

		delete task;

		QCOMPARE(tracker.status(), QString());
		QCOMPARE(tracker.progress(), -1.0);
	}

	void test_clear_resetsToIdle_andAnnouncesItRightAway()
	{
		LaunchProgressTracker tracker;
		tracker.setMinIntervalMs(1000); // long enough it can't fire on its own
		FakeTask task;
		task.start();
		tracker.watch(&task);
		task.setStatus("Downloading assets...");

		QSignalSpy changedSpy(&tracker, &LaunchProgressTracker::changed);
		tracker.clear();

		// Not coalesced, unlike an ordinary status/progress update.
		QCOMPARE(changedSpy.count(), 1);
		QCOMPARE(tracker.status(), QString());
		QCOMPARE(tracker.progress(), -1.0);
	}

	/// The whole point of throttling: a burst of updates inside one
	/// interval must not each produce their own changed() emission, but
	/// the latest value is still visible immediately, and the coalesced
	/// update is not simply dropped.
	void test_rapidUpdates_areCoalesced()
	{
		LaunchProgressTracker tracker;
		tracker.setMinIntervalMs(200);
		FakeTask task;
		task.start();
		tracker.watch(&task);

		QSignalSpy changedSpy(&tracker, &LaunchProgressTracker::changed);
		for (int i = 1; i <= 20; ++i) {
			task.setProgress(i, 20);
		}

		// The first update in a quiet tracker is never throttled; the
		// other 19 in the same burst should not each add their own.
		QCOMPARE(changedSpy.count(), 1);
		// ...but the latest value is visible right away regardless.
		QCOMPARE(tracker.progress(), 1.0);

		// And the coalesced update is not lost - it shows up once the
		// interval elapses.
		QVERIFY(changedSpy.wait(2000));
	}
};

QTEST_GUILESS_MAIN(LaunchProgressTrackerTest)

#include "LaunchProgressTracker_test.moc"
