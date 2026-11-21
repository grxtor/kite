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

#pragma once
#include <QObject>
#include <BaseInstance.h>
#include <tools/BaseProfiler.h>

#include "launch/LaunchFailure.h"
#include "minecraft/launch/MinecraftServerTarget.h"
#include "minecraft/auth/MinecraftAccount.h"

class LaunchController : public Task
{
	Q_OBJECT
  public:
	void executeTask() override;

	LaunchController(QObject* parent = nullptr);
	virtual ~LaunchController() {};

	void setInstance(InstancePtr instance)
	{
		m_instance = instance;
	}

	InstancePtr instance()
	{
		return m_instance;
	}

	void setOnline(bool online)
	{
		m_online = online;
	}

	void setProfiler(BaseProfilerFactory* profiler)
	{
		m_profiler = profiler;
	}

	bool online() const
	{
		return m_online;
	}

	bool demoMode() const
	{
		return m_demoMode;
	}

	MinecraftServerTargetPtr serverToJoin() const
	{
		return m_serverToJoin;
	}

	MinecraftAccountPtr accountToUse() const
	{
		return m_accountToUse;
	}

	/// Launch the demo. The account still logs in, like a normal launch.
	void setDemoMode(bool demoMode)
	{
		m_demoMode = demoMode;
	}

	void setServerToJoin(MinecraftServerTargetPtr serverToJoin)
	{
		m_serverToJoin = std::move(serverToJoin);
	}

	void setAccountToUse(MinecraftAccountPtr accountToUse)
	{
		m_accountToUse = std::move(accountToUse);
	}

	QString id()
	{
		return m_instance->id();
	}

	bool abort() override;

	/// After failed(): what, if anything, is left to tell the player
	/// (Application::controllerFailed() shows it).
	LaunchFailure::Kind failureKind() const
	{
		return m_failureKind;
	}

  private:
	/// emitFailed() for a failure of @p kind; a bare emitFailed() counts as
	/// LaunchFailure::Kind::BeforeStart, so nothing fails without a word.
	void fail(LaunchFailure::Kind kind, const QString& reason);
	void login();
	void launchInstance();
	void decideAccount();

  private slots:
	void readyForLaunch();

	void onSucceeded();
	void onFailed(QString reason);
	void onProgressRequested();

  private:
	BaseProfilerFactory* m_profiler = nullptr;
	bool m_online = true;
	bool m_demoMode = false;
	LaunchFailure::Kind m_failureKind = LaunchFailure::Kind::BeforeStart;
	InstancePtr m_instance;
	MinecraftAccountPtr m_accountToUse = nullptr;
	AuthSessionPtr m_session;
	shared_qobject_ptr<LaunchTask> m_launcher;
	MinecraftServerTargetPtr m_serverToJoin;
};
