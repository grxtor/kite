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

#include <tasks/Task.h>

#include <QString>
#include <QJsonObject>
#include <QTimer>
#include <qsslerror.h>

#include "MinecraftAccount.h"

class QNetworkReply;

/** The state of the current task; getStateMessage() derives the message. */
enum class AccountTaskState {
	STATE_CREATED,
	STATE_WORKING,
	STATE_SUCCEEDED,
	STATE_FAILED_SOFT, //!< soft failure. authentication went through partially
	STATE_FAILED_HARD, //!< hard failure. main tokens are invalid
	STATE_FAILED_GONE, //!< hard failure. main tokens are invalid, and the
					   //!< account no longer exists
	STATE_OFFLINE //!< soft failure. authentication failed in the first step in
				  //!< a 'soft' way
};

class AccountTask : public Task
{
	Q_OBJECT
  public:
	explicit AccountTask(AccountData* data, QObject* parent = 0);
	virtual ~AccountTask() {};

	AccountTaskState m_taskState = AccountTaskState::STATE_CREATED;

	AccountTaskState taskState()
	{
		return m_taskState;
	}

  signals:
	void authorizeWithBrowser(const QUrl& url);

  protected:
	/** The status message for the given state; override to change messages. */
	virtual QString getStateMessage() const;

  protected slots:
	// NOTE: true -> non-terminal state, false -> terminal state
	bool changeState(AccountTaskState newState, QString reason = QString());

  protected:
	AccountData* m_data = nullptr;
};
