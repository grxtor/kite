/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include "PreparePerformance.h"

#include <launch/LaunchTask.h>

#include "minecraft/MinecraftInstance.h"
#include "minecraft/PerformanceSetupTask.h"

void PreparePerformance::executeTask()
{
	auto instance =
		std::dynamic_pointer_cast<MinecraftInstance>(m_parent->instance());
	if (!instance) {
		emitSucceeded();
		return;
	}

	m_setup.reset(new PerformanceSetupTask(instance));
	connect(m_setup.get(), &Task::finished, this,
			&PreparePerformance::setupFinished);
	connect(m_setup.get(), &Task::progress, this, &Task::setProgress);
	connect(m_setup.get(), &Task::status, this, &Task::setStatus);
	m_setup->start();
}

void PreparePerformance::setupFinished()
{
	if (m_setup->wasAborted()) {
		emitFailed(tr("Task aborted."));
		return;
	}
	if (!m_setup->summary().isEmpty()) {
		emit logLine(m_setup->summary() + QStringLiteral("\n"),
					 MessageLevel::Kite);
	}
	// Deliberately not resetting m_setup here: we are inside its own
	// finished() emission. It dies with this step.
	emitSucceeded();
}

bool PreparePerformance::abort()
{
	if (m_setup && m_setup->isRunning()) {
		m_setup->abort();
	}
	return true;
}
