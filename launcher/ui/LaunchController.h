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

#pragma once
#include <QObject>
#include <BaseInstance.h>
#include <tools/BaseProfiler.h>

#include "minecraft/launch/MinecraftServerTarget.h"
#include "minecraft/auth/MinecraftAccount.h"

class InstanceWindow;
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

	/**
	 * Launch the demo instead of logging in.
	 *
	 * An empty username means "decide later": decideAccount() falls back to
	 * the default account's profile name, or a generic one when there is no
	 * account at all.
	 */
	void setDemoMode(bool demoMode, const QString& username = QString())
	{
		m_demoMode = demoMode;
		m_demoUsername = username;
	}

	void setParentWidget(QWidget* widget)
	{
		m_parentWidget = widget;
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

  private:
	void login();
	void launchInstance();
	void decideAccount();

  private slots:
	void readyForLaunch();

	void onSucceeded();
	void onFailed(QString reason);
	void onProgressRequested(Task* task);

  private:
	BaseProfilerFactory* m_profiler = nullptr;
	bool m_online = true;
	bool m_demoMode = false;
	QString m_demoUsername;
	InstancePtr m_instance;
	QWidget* m_parentWidget = nullptr;
	InstanceWindow* m_console = nullptr;
	MinecraftAccountPtr m_accountToUse = nullptr;
	AuthSessionPtr m_session;
	shared_qobject_ptr<LaunchTask> m_launcher;
	MinecraftServerTargetPtr m_serverToJoin;
};
