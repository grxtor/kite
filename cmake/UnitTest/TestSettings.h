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

#pragma once

#include <QDir>
#include <QString>
#include <QTemporaryDir>

#include <memory>

#include "settings/INISettingsObject.h"

/* The global settings BaseInstance's constructor registers overrides and
 * pass-throughs against; without them registration hands back a null Setting.
 * The returned object must outlive every instance built on it. */
inline SettingsObjectPtr makeGlobalSettings(const QString& dir)
{
	auto settings = std::make_shared<INISettingsObject>(
		QDir(dir).filePath("global.ini"));
	settings->registerSetting("PreLaunchCommand", "");
	settings->registerSetting("WrapperCommand", "");
	settings->registerSetting("PostExitCommand", "");
	settings->registerSetting("ShowConsole", true);
	settings->registerSetting("ShowConsoleOnError", true);
	settings->registerSetting("LogPrePostOutput", true);
	settings->registerSetting("ConsoleMaxLines", 100000);
	settings->registerSetting("ConsoleOverflowStop", true);
	return settings;
}

inline SettingsObjectPtr makeGlobalSettings(QTemporaryDir& dir)
{
	return makeGlobalSettings(dir.path());
}
