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

#pragma once

#include <launch/LaunchStep.h>
#include <LoggedProcess.h>
#include <minecraft/auth/AuthSession.h>

#include "MinecraftServerTarget.h"

/* The game process and the way it ends, as DirectJavaLaunch and
 * KitePartLaunch share them. The text arguments are the caller's tr() strings
 * so each step keeps its own translation context. */
class GameProcessStep : public LaunchStep
{
	Q_OBJECT
  public:
	virtual bool canAbort() const
	{
		return true;
	}
	void setWorkingDirectory(const QString& wd);
	void setAuthSession(AuthSessionPtr session)
	{
		m_session = session;
	}

	void setServerToJoin(MinecraftServerTargetPtr serverToJoin)
	{
		m_serverToJoin = std::move(serverToJoin);
	}

  protected:
	explicit GameProcessStep(LaunchTask* parent);

	/// Reports the states that end the process; false for any other state.
	bool handleProcessEnd(LoggedProcess::State state,
						  const char* couldNotLaunchLog,
						  const QString& couldNotLaunch,
						  const QString& crashed);
	/// Starts @p javaPath with @p args, behind the wrapper command if there is
	/// one. Fails the step when the wrapper cannot be found.
	void startProcess(const QString& javaPath, QStringList args,
					  const char* wrapperMissingLog,
					  const QString& wrapperMissing);

	LoggedProcess m_process;
	AuthSessionPtr m_session;
	MinecraftServerTargetPtr m_serverToJoin;
};
