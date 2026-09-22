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

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDebug>
#include "Logging.h"

#include "AuthFlow.h"
#include "katabasis/Globals.h"

AuthFlow::AuthFlow(AccountData* data, QObject* parent)
	: AccountTask(data, parent)
{
}

void AuthFlow::succeed()
{
	m_data->validity_ = Katabasis::Validity::Certain;
	changeState(AccountTaskState::STATE_SUCCEEDED,
				tr("Finished all authentication steps"));
}

void AuthFlow::executeTask()
{
	if (m_currentStep) {
		return;
	}
	changeState(AccountTaskState::STATE_WORKING, tr("Initializing"));
	nextStep();
}

void AuthFlow::nextStep()
{
	if (m_steps.size() == 0) {
		// we got to the end without an incident... assume this is all.
		m_currentStep.reset();
		succeed();
		return;
	}
	m_currentStep = m_steps.front();
	qCDebug(minecraftauthLog) << "AuthFlow:" << m_currentStep->describe();
	m_steps.pop_front();
	connect(m_currentStep.get(), &AuthStep::finished, this,
			&AuthFlow::stepFinished);
	connect(m_currentStep.get(), &AuthStep::authorizeWithBrowser, this,
			&AuthFlow::authorizeWithBrowser);

	m_currentStep->perform();
}

QString AuthFlow::getStateMessage() const
{
	switch (m_taskState) {
		case AccountTaskState::STATE_WORKING: {
			if (m_currentStep) {
				return m_currentStep->describe();
			} else {
				return tr("Working...");
			}
		}
		default: {
			return AccountTask::getStateMessage();
		}
	}
}

void AuthFlow::stepFinished(AccountTaskState resultingState, QString message)
{
	if (changeState(resultingState, message)) {
		nextStep();
	}
}
