/* SPDX-FileCopyrightText: 2026 Project Tick
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

#include "LoggedProcess.h"
#include "MessageLevel.h"
#include <QDebug>

#ifdef Q_OS_UNIX
#include <signal.h>
#include <unistd.h>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#endif

LoggedProcess::LoggedProcess(QObject* parent) : QProcess(parent)
{
	connect(this, &QProcess::readyReadStandardOutput, this,
			&LoggedProcess::on_stdOut);
	connect(this, &QProcess::readyReadStandardError, this,
			&LoggedProcess::on_stdErr);
	connect(this, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &LoggedProcess::on_exit);
	connect(this, &QProcess::errorOccurred, this, &LoggedProcess::on_error);
	connect(this, &QProcess::stateChanged, this,
			&LoggedProcess::on_stateChange);

#if defined(Q_OS_UNIX)
	// A new process group, so kill() can take down the entire tree. Runs in
	// the forked child before exec(): only async-signal-safe calls belong here.
	setChildProcessModifier([]() { setsid(); });
#endif
}

LoggedProcess::~LoggedProcess()
{
	if (m_is_detachable) {
		setProcessState(QProcess::NotRunning);
	}
#ifdef Q_OS_WIN
	// Closing the job handle kills nothing: the job has no
	// JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE, deliberately (see
	// assignToJobObject()).
	if (m_job) {
		CloseHandle(static_cast<HANDLE>(m_job));
		m_job = nullptr;
	}
#endif
}

void LoggedProcess::on_stdErr()
{
	emit log(m_err.feed(readAllStandardError()), MessageLevel::StdErr);
}

void LoggedProcess::on_stdOut()
{
	emit log(m_out.feed(readAllStandardOutput()), MessageLevel::StdOut);
}

void LoggedProcess::on_exit(int exit_code, QProcess::ExitStatus status)
{
	m_exit_code = exit_code;

	if (const QString last = m_err.finish(); !last.isEmpty()) {
		emit log({last}, MessageLevel::StdErr);
	}
	if (const QString last = m_out.finish(); !last.isEmpty()) {
		emit log({last}, MessageLevel::StdOut);
	}

	if (!m_is_aborting) {
		if (status == QProcess::NormalExit) {
			emit log({tr("Process exited with code %1.").arg(exit_code)},
					 MessageLevel::Kite);
			changeState(LoggedProcess::Finished);
		} else {
			if (exit_code == -1)
				emit log({tr("Process crashed.")}, MessageLevel::Kite);
			else
				emit log(
					{tr("Process crashed with exitcode %1.").arg(exit_code)},
					MessageLevel::Kite);
			changeState(LoggedProcess::Crashed);
		}
	} else {
		emit log({tr("Process was killed by user.")}, MessageLevel::Error);
		changeState(LoggedProcess::Aborted);
	}
}

void LoggedProcess::on_error(QProcess::ProcessError error)
{
	switch (error) {
		case QProcess::FailedToStart: {
			emit log({tr("The process failed to start.")}, MessageLevel::Fatal);
			changeState(LoggedProcess::FailedToStart);
			break;
		}
		case QProcess::Crashed:
		case QProcess::ReadError:
		case QProcess::Timedout:
		case QProcess::UnknownError:
		case QProcess::WriteError:
			break;
	}
}

void LoggedProcess::kill()
{
	m_is_aborting = true;
#ifdef Q_OS_UNIX
	// Kill the whole process group so child processes (e.g. Java behind a
	// wrapper) die too.
	auto pid = processId();
	if (pid > 0) {
		::kill(-pid, SIGKILL);
	}
#elif defined(Q_OS_WIN)
	// Terminating the job takes down every process in it (wrapper and java);
	// QProcess::kill() is only the fallback and reaches the direct child
	// alone.
	if (m_job) {
		if (TerminateJobObject(static_cast<HANDLE>(m_job), 1)) {
			return;
		}
		qWarning() << "TerminateJobObject failed with error"
				   << GetLastError()
				   << "- killing only the direct child instead";
	}
	QProcess::kill();
#else
	QProcess::kill();
#endif
}

#ifdef Q_OS_WIN
void LoggedProcess::assignToJobObject()
{
	if (m_job) {
		return;
	}
	auto pid = processId();
	if (pid <= 0) {
		// No pid before the process is running, hence called from the Running
		// state change.
		return;
	}

	HANDLE job = CreateJobObjectW(nullptr, nullptr);
	if (!job) {
		qWarning() << "Could not create a job object for process" << pid
				   << "- error" << GetLastError()
				   << "; killing it will not take down its children";
		return;
	}

	// The job deliberately has no limits: JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
	// would kill the game when the handle closes, but setDetachable(true) lets
	// it outlive the launcher. A limitless job is enough for
	// TerminateJobObject().

	// Reopening by pid is safe: QProcess holds a handle, so Windows does not
	// recycle the pid.
	HANDLE process = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE,
								 static_cast<DWORD>(pid));
	if (!process) {
		qWarning() << "Could not open process" << pid << "- error"
				   << GetLastError();
		CloseHandle(job);
		return;
	}
	if (!AssignProcessToJobObject(job, process)) {
		qWarning() << "Could not assign process" << pid
				   << "to a job object - error" << GetLastError();
		CloseHandle(process);
		CloseHandle(job);
		return;
	}
	CloseHandle(process);
	m_job = job;

	// Known race: a grandchild spawned between CreateProcess and the
	// assignment is not in the job. Closing it would need a suspended start,
	// and QProcess does not expose the PROCESS_INFORMATION to resume the main
	// thread.
}
#endif

int LoggedProcess::exitCode() const
{
	return m_exit_code;
}

void LoggedProcess::changeState(LoggedProcess::State state)
{
	if (state == m_state)
		return;
	m_state = state;
	emit stateChanged(m_state);
}

LoggedProcess::State LoggedProcess::state() const
{
	return m_state;
}

void LoggedProcess::on_stateChange(QProcess::ProcessState state)
{
	switch (state) {
		case QProcess::NotRunning:
			break;
		case QProcess::Starting: {
			if (m_state != LoggedProcess::NotRunning) {
				qWarning() << "Wrong state change for process from state"
						   << m_state << "to" << (int)LoggedProcess::Starting;
			}
			changeState(LoggedProcess::Starting);
			return;
		}
		case QProcess::Running: {
			if (m_state != LoggedProcess::Starting) {
				qWarning() << "Wrong state change for process from state"
						   << m_state << "to" << (int)LoggedProcess::Running;
			}
#ifdef Q_OS_WIN
			// Before changeState(), so anything reacting to Running, including
			// an immediate kill(), sees the job.
			assignToJobObject();
#endif
			changeState(LoggedProcess::Running);
			return;
		}
	}
}

qint64 LoggedProcess::processId() const
{
	return QProcess::processId();
}

void LoggedProcess::setDetachable(bool detachable)
{
	m_is_detachable = detachable;
}
