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

#include <QObject>
#include <QList>
#include <QUrl>
#include <memory>

#include "tasks/Task.h"

#include "QObjectPtr.h"

class MinecraftVersion;
class MinecraftInstance;
class LaunchProfile;

class MinecraftLoadAndCheck : public Task
{
	Q_OBJECT
  public:
	explicit MinecraftLoadAndCheck(MinecraftInstance* inst,
								   QObject* parent = 0);
	virtual ~MinecraftLoadAndCheck() {};
	void executeTask() override;

	/// Whether @p profile (PackProfile::getProfile(), which is null when the
	/// patches could not be applied) is complete enough to launch from: it
	/// exists and none of its components is missing or broken. A launch that
	/// carries on without this starts java with no libraries and no game,
	/// and only says "Game crashed".
	static bool profileUsable(const std::shared_ptr<LaunchProfile>& profile);
	/// What the user is told when the game's data is not there yet: that it
	/// still has to be downloaded, and how. @p detail (the technical reason,
	/// may be empty) follows on its own line.
	static QString notDownloadedMessage(const QString& detail = QString());

  private slots:
	void subtaskSucceeded();
	void subtaskFailed(QString error);

  private:
	MinecraftInstance* m_inst = nullptr;
	Task::Ptr m_task;
	QString m_preFailure;
	QString m_fail_reason;
};
