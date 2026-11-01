/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include "GameProcessStep.h"

#include <QStandardPaths>

#include "Commandline.h"
#include "launch/LaunchTask.h"

GameProcessStep::GameProcessStep(LaunchTask* parent) : LaunchStep(parent)
{
	connect(&m_process, &LoggedProcess::log, this, &GameProcessStep::logLines);
}

void GameProcessStep::setWorkingDirectory(const QString& wd)
{
	m_process.setWorkingDirectory(wd);
}

bool GameProcessStep::handleProcessEnd(LoggedProcess::State state,
									   const char* couldNotLaunchLog,
									   const QString& couldNotLaunch,
									   const QString& crashed)
{
	switch (state) {
		case LoggedProcess::FailedToStart: {
			emit logLine(couldNotLaunchLog, MessageLevel::Fatal);
			emitFailed(couldNotLaunch);
			return true;
		}
		case LoggedProcess::Aborted:
		case LoggedProcess::Crashed: {
			m_parent->setPid(-1);
			emitFailed(crashed);
			return true;
		}
		case LoggedProcess::Finished: {
			m_parent->setPid(-1);
			// if the exit code wasn't 0, report this as a crash
			auto exitCode = m_process.exitCode();
			if (exitCode != 0) {
				emitFailed(crashed);
				return true;
			}
			// FIXME: INST_EXITCODE is not passed to the post-exit command.
			emitSucceeded();
			return true;
		}
		default:
			return false;
	}
}

void GameProcessStep::startProcess(const QString& javaPath, QStringList args,
								   const char* wrapperMissingLog,
								   const QString& wrapperMissing)
{
	QString wrapperCommandStr = m_parent->wrapperCommand();
	if (!wrapperCommandStr.isEmpty()) {
		auto wrapperArgs = Commandline::splitArgs(wrapperCommandStr);
		auto wrapperCommand = wrapperArgs.takeFirst();
		auto realWrapperCommand =
			QStandardPaths::findExecutable(wrapperCommand);
		if (realWrapperCommand.isEmpty()) {
			emit logLine(QString(wrapperMissingLog).arg(wrapperCommand),
						 MessageLevel::Fatal);
			emitFailed(wrapperMissing.arg(wrapperCommand));
			return;
		}
		emit logLine("Wrapper command is:\n" + wrapperCommandStr + "\n\n",
					 MessageLevel::Kite);
		args.prepend(javaPath);
		m_process.start(wrapperCommand, wrapperArgs + args);
	} else {
		m_process.start(javaPath, args);
	}
}
