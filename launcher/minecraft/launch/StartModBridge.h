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

#pragma once

#include <launch/LaunchStep.h>

#include <memory>

class MinecraftInstance;
class ModBridge;

/*
 * The launch step, right before the game starts, that gives a game with the
 * Kite mod its way to the launcher (modbridge/ModBridge.h): a session on the
 * bridge, whose port the game gets as a JVM property and whose token it gets
 * in its environment (MinecraftInstance::kiteModArguments() and
 * createEnvironment() add them while the instance holds the grant).
 *
 * It sits after PrepareKiteMod, so whether the jar is there is known; without
 * the mod, or without a bridge (a tool, a test), it does nothing. It never
 * fails the launch. The session is dropped when the launch ends however it
 * ends (finalize()); the bridge also drops it when the instance stops or is
 * destroyed.
 */
class StartModBridge : public LaunchStep
{
	Q_OBJECT
  public:
	explicit StartModBridge(LaunchTask* parent) : LaunchStep(parent) {}
	~StartModBridge() override = default;

	void executeTask() override;
	bool canAbort() const override
	{
		return false;
	}
	void finalize() override;

  private:
	std::shared_ptr<MinecraftInstance> m_instance;
	ModBridge* m_bridge = nullptr;
};
