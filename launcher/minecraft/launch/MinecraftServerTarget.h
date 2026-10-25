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

#include <memory>

#include <QString>

struct MinecraftServerTarget {
	QString address;
	quint16 port = 25565;

	QString world;

	/// The address as the game's --quickPlayMultiplayer "host:port" takes it:
	/// an IPv6 address has colons of its own, so it goes in brackets.
	QString quickPlayHost() const;

	static MinecraftServerTarget parse(const QString& fullAddress,
									   bool useWorld);
};

typedef std::shared_ptr<MinecraftServerTarget> MinecraftServerTargetPtr;
