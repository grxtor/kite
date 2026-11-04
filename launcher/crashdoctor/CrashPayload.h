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

#include <QJsonObject>
#include <QString>
#include <QVariantMap>

#include "CrashCollector.h"

/*
 * What a crash report sends to Kite's server (POST /v1/crash-reports,
 * server/friends README), built from one analysis: the versions, the mods'
 * ids and versions, the rules that matched and the scrubbed error lines with
 * their signature. Nothing names the player, the computer or the instance.
 * Pure, so the exact payload is a unit test.
 */
namespace CrashPayload
{
	inline constexpr int kFormatVersion = 1;
	inline constexpr int kMaxMods = 500;
	inline constexpr int kMaxRules = 20;

	/// Whether @p report has something to send: error lines, and no rule that
	/// knew the crash (fallbacks do not know it) unless its fix did not help.
	bool sendable(const CrashDoctor::Report& report);
	/// The JSON body for @p report; @p kiteVersion is BuildConfig's.
	QJsonObject build(const CrashDoctor::Report& report, const QString& kiteVersion);
	/// @p payload as the crash window shows it before sending: "versions"
	/// ("Kite 1.2.0 · macOS arm64 · Java 17"), "game" ("Minecraft 1.20.1 ·
	/// Forge 47.3.0"), "lines", "mods" ("create 0.5.1.j, …"), "modCount",
	/// "rules" and "culprit" ("" when none).
	QVariantMap readable(const QJsonObject& payload);
} // namespace CrashPayload
