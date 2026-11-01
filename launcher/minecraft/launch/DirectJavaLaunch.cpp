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

#include "DirectJavaLaunch.h"
#include <launch/LaunchTask.h>
#include <minecraft/MinecraftInstance.h>
#include <FileSystem.h>

DirectJavaLaunch::DirectJavaLaunch(LaunchTask* parent)
	: GameProcessStep(parent)
{
	connect(&m_process, &LoggedProcess::stateChanged, this,
			&DirectJavaLaunch::on_state);
}

void DirectJavaLaunch::executeTask()
{
	setStatus(tr("Starting Minecraft"));
	auto instance = m_parent->instance();
	std::shared_ptr<MinecraftInstance> minecraftInstance =
		std::dynamic_pointer_cast<MinecraftInstance>(instance);
	QStringList args = minecraftInstance->javaArguments();
	args.append(minecraftInstance->kiteModArguments());

	args.append("-Djava.library.path=" + minecraftInstance->getNativePath());

	auto classPathEntries = minecraftInstance->getClassPath();
	args.append("-cp");
	QString classpath;
#ifdef Q_OS_WIN32
	classpath = classPathEntries.join(';');
#else
	classpath = classPathEntries.join(':');
#endif
	args.append(classpath);
	args.append(minecraftInstance->getMainClass());

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

	auto mcArgs =
		minecraftInstance->processMinecraftArgs(m_session, m_serverToJoin);
	args.append(mcArgs);

	const char* wrapperMissing =
		QT_TR_NOOP("The wrapper command \"%1\" couldn't be found.");
	startProcess(javaPath, args, wrapperMissing, tr(wrapperMissing));
}

void DirectJavaLaunch::on_state(LoggedProcess::State state)
{
	//: Error message displayed if instance can't start
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
			// The game is up: nothing left to report, so no stage either.
			setStatus(QString());
			break;
		default:
			break;
	}
}

void DirectJavaLaunch::proceed()
{
	// nil
}

bool DirectJavaLaunch::abort()
{
	auto state = m_process.state();
	if (state == LoggedProcess::Running || state == LoggedProcess::Starting) {
		m_process.kill();
	}
	return true;
}
