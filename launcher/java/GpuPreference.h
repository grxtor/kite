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

#include <QString>
#include <QStringList>

#include <optional>

class SettingsObject;

/*
 * Windows' per-app graphics card choice (Settings > System > Display >
 * Graphics) for the Java that runs the game: one value per executable under
 * HKCU\Software\Microsoft\DirectX\UserGpuPreferences. Kite only adds a value
 * where there is none and only removes the ones it added, so a choice the
 * player made there stays. The decisions are plain functions; the registry
 * part exists on Windows only.
 */
namespace GpuPreference
{
	/// The launcher setting that turns it on, and the one listing the values
	/// Kite wrote.
	extern const QString kSetting;
	extern const QString kWrittenSetting;

	/// The data Windows writes for "High performance".
	QString highPerformance();

	/// The value name Windows uses for @p javaPath: the path with
	/// backslashes.
	QString valueName(const QString& javaPath);

	/// Whether Kite writes its value over @p current (nullopt: no value).
	/// Only where there is none: any value there is the player's choice.
	bool shouldWrite(const std::optional<QString>& current);

	/// Whether Kite removes a value it wrote that now reads @p current: only
	/// while it is still Kite's.
	bool shouldRemove(const std::optional<QString>& current);

	/// The written value names as kWrittenSetting stores them, and back.
	QString encodeWritten(const QStringList& names);
	QStringList decodeWritten(const QString& stored);

#ifdef Q_OS_WIN
	/// At launch: with kSetting on, gives @p javaPath the high-performance
	/// card unless it already has a value.
	void applyForLaunch(SettingsObject& settings, const QString& javaPath);

	/// kSetting turned off: removes the values Kite wrote.
	void removeWritten(SettingsObject& settings);
#endif
} // namespace GpuPreference
