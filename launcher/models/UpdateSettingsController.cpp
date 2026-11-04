/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include "models/UpdateSettingsController.h"

UpdateSettingsController::UpdateSettingsController(QObject* parent)
	: QObject(parent)
{
}

UpdateSettingsController::~UpdateSettingsController() = default;

void UpdateSettingsController::setBackend(Backend backend)
{
	m_backend = std::move(backend);
	m_available = m_backend.automaticallyChecks &&
				  m_backend.setAutomaticallyChecks && m_backend.lastCheck &&
				  m_backend.checkNow;
	emit changed();
}

bool UpdateSettingsController::available() const
{
	return m_available;
}

bool UpdateSettingsController::autoCheck() const
{
	return m_available && m_backend.automaticallyChecks();
}

void UpdateSettingsController::setAutoCheck(bool check)
{
	if (!m_available || check == autoCheck()) {
		return;
	}
	m_backend.setAutomaticallyChecks(check);
	emit changed();
}

QString UpdateSettingsController::channel() const
{
	return m_available ? m_backend.channel : QString();
}

qint64 UpdateSettingsController::lastCheckedMs() const
{
	if (!m_available) {
		return 0;
	}
	const QDateTime last = m_backend.lastCheck();
	return last.isValid() ? last.toMSecsSinceEpoch() : 0;
}

QString UpdateSettingsController::pendingVersion() const
{
	return m_available && m_backend.pendingVersion ? m_backend.pendingVersion()
												   : QString();
}

bool UpdateSettingsController::canInstallNow() const
{
	return m_available &&
		   (!m_backend.canInstallNow || m_backend.canInstallNow());
}

QString UpdateSettingsController::managedBy() const
{
	return m_available ? m_backend.managedBy : QString();
}

void UpdateSettingsController::checkNow()
{
	if (m_available) {
		m_backend.checkNow();
	}
}

void UpdateSettingsController::installPendingNow()
{
	if (m_available && m_backend.installPendingNow && canInstallNow() &&
		!pendingVersion().isEmpty()) {
		m_backend.installPendingNow();
	}
}

void UpdateSettingsController::refresh()
{
	emit changed();
}
