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

#include "KitePartLaunch.h"

#include <QTimer>

#include "launch/LaunchTask.h"
#include "minecraft/MinecraftInstance.h"
#include "FileSystem.h"
#include "Logging.h"
#include "core/LauncherContext.h"

KitePartLaunch::KitePartLaunch(LaunchTask* parent) : GameProcessStep(parent)
{
	connect(&m_process, &LoggedProcess::stateChanged, this,
			&KitePartLaunch::on_state);
}

#ifdef Q_OS_WIN
// returns 8.3 file format from long path
#include <windows.h>
QString shortPathName(const QString& file)
{
	auto input = file.toStdWString();
	std::wstring output;
	long length = GetShortPathNameW(input.c_str(), NULL, 0);
	// NOTE: this resizing might seem weird...
	// when GetShortPathNameW fails, it returns length including null character
	// when it succeeds, it returns length excluding null character
	// See:
	// https://msdn.microsoft.com/en-us/library/windows/desktop/aa364989(v=vs.85).aspx
	output.resize(length);
	GetShortPathNameW(input.c_str(), (LPWSTR)output.c_str(), length);
	output.resize(length - 1);
	QString ret = QString::fromStdWString(output);
	return ret;
}
#endif

// if the string survives roundtrip through local 8bit encoding...
bool fitsInLocal8bit(const QString& string)
{
	return string == QString::fromLocal8Bit(string.toLocal8Bit());
}

void KitePartLaunch::executeTask()
{
	setStatus(tr("Starting Minecraft"));
	auto instance = m_parent->instance();
	std::shared_ptr<MinecraftInstance> minecraftInstance =
		std::dynamic_pointer_cast<MinecraftInstance>(instance);

	m_launchScript =
		minecraftInstance->createLaunchScript(m_session, m_serverToJoin);
	QStringList args = minecraftInstance->javaArguments();
	QString allArgs = args.join(", ");
	emit logLine("Java Arguments:\n[" + m_parent->censorPrivateInfo(allArgs) +
					 "]\n\n",
				 MessageLevel::Kite);

	auto javaPath =
		FS::ResolveExecutable(instance->settings()->get("JavaPath").toString());

	m_process.setProcessEnvironment(instance->createEnvironment());

	// make detachable - this will keep the process running even if the object
	// is destroyed
	m_process.setDetachable(true);

	auto classPath = minecraftInstance->getClassPath();
	classPath.prepend(
		FS::PathCombine(LAUNCHER->getJarsPath(), "NewLaunch.jar"));

	auto natPath = minecraftInstance->getNativePath();
#ifdef Q_OS_WIN
	if (!fitsInLocal8bit(natPath)) {
		args << "-Djava.library.path=" + shortPathName(natPath);
	} else {
		args << "-Djava.library.path=" + natPath;
	}
#else
	args << "-Djava.library.path=" + natPath;
#endif

	args << "-cp";
#ifdef Q_OS_WIN
	QStringList processed;
	for (auto& item : classPath) {
		if (!fitsInLocal8bit(item)) {
			processed << shortPathName(item);
		} else {
			processed << item;
		}
	}
	args << processed.join(';');
#else
	args << classPath.join(':');
#endif
	args << "io.github.grxtor.EntryPoint";

	qCDebug(minecraftlaunchLog) << args.join(' ');

	const char* wrapperMissing =
		QT_TR_NOOP("The wrapper command \"%1\" couldn't be found.");
	startProcess(javaPath, args, wrapperMissing, tr(wrapperMissing));
}

void KitePartLaunch::on_state(LoggedProcess::State state)
{
	//: Error message displayed if instace can't start
	const char* reason = QT_TR_NOOP("Could not launch minecraft!");
	if (handleProcessEnd(state, reason, tr(reason), tr("Game crashed."))) {
		return;
	}
	switch (state) {
		case LoggedProcess::Running:
			emit logLine(QString("Minecraft process ID: %1\n\n")
							 .arg(m_process.processId()),
						 MessageLevel::Kite);
			m_parent->setPid(m_process.processId());
			m_parent->instance()->setLastLaunch();
			// send the launch script to Kite part
			m_process.write(m_launchScript.toUtf8());

			mayProceed = true;
			emit readyForLaunch();
			break;
		default:
			break;
	}
}

void KitePartLaunch::proceed()
{
	if (mayProceed) {
		QString launchString("launch\n");
		m_process.write(launchString.toUtf8());
		mayProceed = false;
	}
}

// How long the helper gets to act on "abort" before we stop being polite: long
// enough for a healthy helper to shut the game down, short enough that Kill
// does not look broken.
static constexpr int ABORT_GRACE_MS = 3000;

bool KitePartLaunch::abort()
{
	if (mayProceed) {
		mayProceed = false;
		QString launchString("abort\n");
		m_process.write(launchString.toUtf8());
		// Writing "abort" only asks the helper to quit. A wedged helper never
		// reads it, and the task is marked aborted either way, which would
		// leave the game running with nothing to Kill. Escalate if it is still
		// there.
		QTimer::singleShot(ABORT_GRACE_MS, this, [this]() {
			auto state = m_process.state();
			if (state != LoggedProcess::Running &&
				state != LoggedProcess::Starting) {
				return;
			}
			emit logLine(tr("The game did not react to the abort request. "
							"Killing it."),
						 MessageLevel::Kite);
			m_process.kill();
		});
	} else {
		auto state = m_process.state();
		if (state == LoggedProcess::Running ||
			state == LoggedProcess::Starting) {
			m_process.kill();
		}
	}
	return true;
}
