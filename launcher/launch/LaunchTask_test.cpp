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
#include <QTemporaryDir>
#include <QTest>

#include "NullInstance.h"
#include "launch/LaunchStep.h"
#include "launch/LaunchTask.h"
#include "settings/INISettingsObject.h"

#include "TestSettings.h"

// What a launch says it is doing, step by step (LaunchTask::startStep()): the
// stage a step names, "Preparing to launch" for one that names none, and no
// progress left over from the previous step. The play bar shows it.
namespace
{
	struct InstanceFixture {
		QTemporaryDir globalDir;
		QTemporaryDir instDir;
		SettingsObjectPtr global = makeGlobalSettings(globalDir);
		InstancePtr instance = std::make_shared<NullInstance>(
			global,
			std::make_shared<INISettingsObject>(
				instDir.filePath("instance.cfg")),
			instDir.path());
	};
} // namespace

// A step that says nothing, like most of a real launch's.
class SilentStep : public LaunchStep
{
	Q_OBJECT
  public:
	using LaunchStep::LaunchStep;

  protected:
	void executeTask() override
	{
		emitSucceeded();
	}
};

// A step with a stage of its own, halfway along when it ends.
class StagedStep : public LaunchStep
{
	Q_OBJECT
  public:
	using LaunchStep::LaunchStep;

  protected:
	void executeTask() override
	{
		setStatus(QStringLiteral("Checking Java"));
		setProgress(1, 2);
		emitSucceeded();
	}
};

// A step that fails on its own, like a game that crashed.
class FailingStep : public LaunchStep
{
	Q_OBJECT

  public:
	using LaunchStep::LaunchStep;

  protected:
	void executeTask() override
	{
		emitFailed(QStringLiteral("Game crashed."));
	}
};

// A step that runs until it is stopped, like a running game; stopping it ends
// it failed, the way GameProcessStep does after a Kill.
class RunningStep : public LaunchStep
{
	Q_OBJECT

  public:
	using LaunchStep::LaunchStep;

	bool canAbort() const override
	{
		return true;
	}

  public slots:
	bool abort() override
	{
		QMetaObject::invokeMethod(
			this, [this] { emitFailed(QStringLiteral("The game was stopped.")); },
			Qt::QueuedConnection);
		return true;
	}

  protected:
	void executeTask() override {}
};

class LaunchTaskTest : public QObject
{
	Q_OBJECT

  private slots:
	void silentStepShowsPreparing()
	{
		InstanceFixture fixture;
		auto task = LaunchTask::create(fixture.instance);
		task->appendStep(shared_qobject_ptr<LaunchStep>(new SilentStep(task.get())));
		QSignalSpy statusSpy(task.get(), &Task::status);

		task->start();

		QTRY_VERIFY(task->isFinished());
		QVERIFY(!statusSpy.isEmpty());
		QCOMPARE(statusSpy.first().first().toString(),
				 QStringLiteral("Preparing to launch"));
	}

	void stepStageReplacesPreparing()
	{
		InstanceFixture fixture;
		auto task = LaunchTask::create(fixture.instance);
		task->appendStep(shared_qobject_ptr<LaunchStep>(new StagedStep(task.get())));
		QSignalSpy statusSpy(task.get(), &Task::status);

		task->start();

		QTRY_VERIFY(task->isFinished());
		QStringList statuses;
		for (const auto& args : statusSpy) {
			statuses << args.first().toString();
		}
		QCOMPARE(statuses, QStringList({QStringLiteral("Preparing to launch"),
										QStringLiteral("Checking Java")}));
	}

	// The next step starts with no progress, not the last step's half bar.
	void nextStepStartsWithoutProgress()
	{
		InstanceFixture fixture;
		auto task = LaunchTask::create(fixture.instance);
		task->appendStep(shared_qobject_ptr<LaunchStep>(new StagedStep(task.get())));
		task->appendStep(shared_qobject_ptr<LaunchStep>(new SilentStep(task.get())));
		QSignalSpy progressSpy(task.get(), &Task::progress);

		task->start();

		QTRY_VERIFY(task->isFinished());
		QVERIFY(progressSpy.size() >= 3);
		const QList<QVariant> last = progressSpy.last();
		QCOMPARE(last.at(0).toLongLong(), 0);
		QCOMPARE(last.at(1).toLongLong(), 0);
	}

	// The crash flag Home reads ("Crashed last time").
	void failedLaunchIsACrash()
	{
		InstanceFixture fixture;
		auto task = LaunchTask::create(fixture.instance);
		task->appendStep(shared_qobject_ptr<LaunchStep>(new FailingStep(task.get())));
		task->start();
		QTRY_VERIFY(task->isFinished());
		QVERIFY(!task->wasSuccessful());
		QVERIFY(fixture.instance->hasCrashed());
	}

	void stoppedGameIsNoCrash()
	{
		InstanceFixture fixture;
		auto task = LaunchTask::create(fixture.instance);
		task->appendStep(shared_qobject_ptr<LaunchStep>(new RunningStep(task.get())));
		task->start();
		QTRY_VERIFY(task->isRunning());
		QVERIFY(task->abort());
		QTRY_VERIFY(task->isFinished());
		QVERIFY(!task->wasSuccessful());
		QVERIFY(!fixture.instance->hasCrashed());
	}
};

QTEST_GUILESS_MAIN(LaunchTaskTest)

#include "LaunchTask_test.moc"
