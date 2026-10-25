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

#include "StartModBridge.h"

#include <launch/LaunchTask.h>

#include "core/LauncherContext.h"
#include "minecraft/MinecraftInstance.h"
#include "modbridge/ModBridge.h"

namespace
{
	/* The token is in no line the launcher writes; if it ever lands in one,
	 * this stands in for it. */
	const QString kCensored = QStringLiteral("<BRIDGE TOKEN>");
} // namespace

void StartModBridge::executeTask()
{
	m_instance = std::dynamic_pointer_cast<MinecraftInstance>(
		m_parent->instance());
	m_bridge = LAUNCHER ? LAUNCHER->modBridge() : nullptr;
	if (!m_instance || !m_bridge || !m_instance->kiteModApplies() ||
		m_instance->kiteModJar().isEmpty()) {
		emitSucceeded();
		return;
	}
	const auto grant = m_bridge->registerSession(m_instance.get());
	if (grant.valid()) {
		m_instance->setBridgeGrant(grant);
		m_parent->addCensorEntry(grant.token, kCensored);
	} else {
		emit logLine(tr("Kite mod: the in-game friends list is not available") +
						 QStringLiteral("\n"),
					 MessageLevel::Kite);
	}
	emitSucceeded();
}

void StartModBridge::finalize()
{
	if (m_bridge && m_instance) {
		m_bridge->unregisterSession(m_instance.get());
		m_instance->setBridgeGrant({});
	}
}
