/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

// A step that fails before the game runs, like a download with no network.
class FailingStep : public LaunchStep
{
	Q_OBJECT

  public:
	using LaunchStep::LaunchStep;

  protected:
	void executeTask() override
	{
		emitFailed(QStringLiteral("Couldn't download the game files."));
	}
};

// The game's process started (it has a pid) and then exited badly, the way
// GameProcessStep reports a crash.
class CrashingGameStep : public LaunchStep
{
	Q_OBJECT

  public:
	using LaunchStep::LaunchStep;

  protected:
	void executeTask() override
	{
		m_parent->setPid(4242);
		m_parent->setPid(-1);
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

// A step that ends well after writing @p report under the game root's
// crash-reports, the way Forge leaves its loading error screen.
class ReportingStep : public LaunchStep
{
	Q_OBJECT

  public:
	ReportingStep(LaunchTask* parent, QString gameRoot)
		: LaunchStep(parent), m_gameRoot(std::move(gameRoot))
	{
	}

  protected:
	void executeTask() override
	{
		QDir(m_gameRoot).mkpath(QStringLiteral("crash-reports"));
		QFile report(QDir(m_gameRoot).filePath(
			QStringLiteral("crash-reports/crash-2026-11-04_10.00.00-fml.txt")));
		if (report.open(QIODevice::WriteOnly)) {
			report.write("---- Minecraft Crash Report ----\n");
		}
		emitSucceeded();
	}

  private:
	QString m_gameRoot;
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
	void gameThatExitedBadlyIsACrash()
	{
		InstanceFixture fixture;
		auto task = LaunchTask::create(fixture.instance);
		task->appendStep(shared_qobject_ptr<LaunchStep>(new SilentStep(task.get())));
		task->appendStep(shared_qobject_ptr<LaunchStep>(new CrashingGameStep(task.get())));
		task->start();
		QTRY_VERIFY(task->isFinished());
		QVERIFY(!task->wasSuccessful());
		QVERIFY(task->gameStarted());
		QVERIFY(fixture.instance->hasCrashed());
	}

	// A launch that fails before the game's process starts (a download, Java)
	// is no crash: no "Crashed last time", no crash window.
	void failureBeforeTheGameStartsIsNoCrash()
	{
		InstanceFixture fixture;
		// The flag of an older crash goes too: this launch says nothing new
		// about the game.
		fixture.instance->setCrashed(true);
		auto task = LaunchTask::create(fixture.instance);
		task->appendStep(shared_qobject_ptr<LaunchStep>(new SilentStep(task.get())));
		task->appendStep(shared_qobject_ptr<LaunchStep>(new FailingStep(task.get())));
		QSignalSpy failedSpy(task.get(), &Task::failed);
		task->start();
		QTRY_VERIFY(task->isFinished());
		QVERIFY(!task->wasSuccessful());
		QVERIFY(!task->gameStarted());
		QVERIFY(!task->isAborting());
		QVERIFY(!fixture.instance->hasCrashed());
		// The failing step's own reason is what the player gets to read.
		QCOMPARE(failedSpy.size(), 1);
		QCOMPARE(failedSpy.first().first().toString(),
				 QStringLiteral("Couldn't download the game files."));
	}

	void gameStartedOnlyOnceThereIsAProcess()
	{
		InstanceFixture fixture;
		auto task = LaunchTask::create(fixture.instance);
		QVERIFY(!task->gameStarted());
		task->setPid(-1);
		QVERIFY(!task->gameStarted());
		task->setPid(4242);
		QVERIFY(task->gameStarted());
		// It stays said after the process is gone.
		task->setPid(-1);
		QVERIFY(task->gameStarted());
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

	// Cancel while the launch still prepares: no process yet, so nothing to
	// warn about (LaunchController::abort() asks only once gameStarted()).
	void cancelledBeforeTheGameStarts()
	{
		InstanceFixture fixture;
		auto task = LaunchTask::create(fixture.instance);
		task->appendStep(shared_qobject_ptr<LaunchStep>(new RunningStep(task.get())));
		task->start();
		QTRY_VERIFY(task->isRunning());
		QVERIFY(task->canAbort());
		QVERIFY(!task->gameStarted());
		QVERIFY(task->abort());
		QTRY_VERIFY(task->isFinished());
		QVERIFY(task->isAborting());
		QVERIFY(!fixture.instance->hasCrashed());
	}

	void crashReportFromThisRunIsACrash()
	{
		InstanceFixture fixture;
		auto task = LaunchTask::create(fixture.instance);
		task->appendStep(shared_qobject_ptr<LaunchStep>(
			new ReportingStep(task.get(), fixture.instance->gameRoot())));
		task->start();
		QTRY_VERIFY(task->isFinished());
		QVERIFY(task->wasSuccessful());
		QVERIFY(fixture.instance->hasCrashed());
	}

	void olderCrashReportIsNotThisRun()
	{
		InstanceFixture fixture;
		const QString reports =
			QDir(fixture.instance->gameRoot()).filePath(QStringLiteral("crash-reports"));
		QVERIFY(QDir().mkpath(reports));
		QFile old(QDir(reports).filePath(QStringLiteral("crash-2026-01-01_10.00.00-client.txt")));
		QVERIFY(old.open(QIODevice::WriteOnly));
		old.write("old");
		QVERIFY(old.flush());
		QVERIFY(old.setFileTime(QDateTime::currentDateTime().addDays(-1),
								QFileDevice::FileModificationTime));
		old.close();
		auto task = LaunchTask::create(fixture.instance);
		task->appendStep(shared_qobject_ptr<LaunchStep>(new SilentStep(task.get())));
		task->start();
		QTRY_VERIFY(task->isFinished());
		QVERIFY(task->wasSuccessful());
		QVERIFY(!fixture.instance->hasCrashed());
	}
};

QTEST_GUILESS_MAIN(LaunchTaskTest)

#include "LaunchTask_test.moc"
