/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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

#pragma once

#include <QProcess>
#include "MessageLevel.h"

/* A basic process with line-based logging; hides some of QProcess's nastier
 * bits. */
class LoggedProcess : public QProcess
{
	Q_OBJECT
  public:
	enum State {
		NotRunning,
		Starting,
		FailedToStart,
		Running,
		Finished,
		Crashed,
		Aborted
	};

  public:
	explicit LoggedProcess(QObject* parent = 0);
	virtual ~LoggedProcess();

	State state() const;
	int exitCode() const;
	qint64 processId() const;

	void setDetachable(bool detachable);

  signals:
	void log(QStringList lines, MessageLevel::Enum level);
	void stateChanged(LoggedProcess::State state);

  public slots:
	void kill();

  private slots:
	void on_stdErr();
	void on_stdOut();
	void on_exit(int exit_code, QProcess::ExitStatus status);
	void on_error(QProcess::ProcessError error);
	void on_stateChange(QProcess::ProcessState);

  private:
	void changeState(LoggedProcess::State state);

#ifdef Q_OS_WIN
	/** Puts the child into a fresh job object once it exists; otherwise kill()
	 * reaches only the direct child (the wrapper command when one is
	 * configured), leaving java alive. */
	void assignToJobObject();
#endif

  private:
	QString m_err_leftover;
	QString m_out_leftover;
	bool m_killed = false;
	State m_state = NotRunning;
	int m_exit_code = 0;
	bool m_is_aborting = false;
	bool m_is_detachable = false;
#ifdef Q_OS_WIN
	// The job object the child was assigned to, or null if none was obtained
	// (kill() then reaches only the direct child). void* because it is a
	// Windows HANDLE, keeping windows.h out of every translation unit that
	// launches a process.
	void* m_job = nullptr;
#endif
};
