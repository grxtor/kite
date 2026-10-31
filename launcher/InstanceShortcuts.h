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

#pragma once

#include <QString>
#include <QStringList>

#include "BaseInstance.h"

/* Shortcuts that start one instance without the launcher window: the platform
 * half shared by both user interfaces. FS::createShortcut() writes the file;
 * this renders the icon first (the launcher knows it only as a theme key;
 * shells want .icns, .png or .ico). Calls return a Result whose `error` is
 * already worded; the caller decides how to show it. */
namespace InstanceShortcuts
{
	struct Request {
		BaseInstance* instance = nullptr;
		QString name;
		/// What the shortcut leads to, already translated, for the "Could not
		/// create a shortcut to this %1" message: an instance, a world, or a
		/// server.
		QString targetString;
		QStringList extraArgs;
		QString iconKey;
		/// Where it goes; also remembered on the instance once the shortcut is
		/// written.
		ShortcutTarget target = ShortcutTarget::Desktop;
	};

	struct Result {
		bool ok = false;
		QString path;
		QString error;
	};

	/** The folder @p target writes into, or empty when this system has none.
	 * Not created here. Other has no folder of its own: the caller asks the
	 * user. */
	QString folderFor(ShortcutTarget target);

	/** The suffix FS::createShortcut() gives the file on this platform, dot
	 * included, or empty where a user would not type it (the `.app` of a macOS
	 * bundle). For a save dialog's filter and default suffix. */
	QString fileSuffix();

	/** @p path as create() wants it: without the suffix FS::createShortcut()
	 * adds itself, in case the user typed it into a save dialog. */
	QString withoutSuffix(QString path);

	/** Write @p request to @p filePath, which carries no suffix;
	 * FS::createShortcut() appends whatever this platform uses. */
	Result create(const Request& request, const QString& filePath);

	/** Write @p request into folderFor(request.target), named after
	 * request.name, creating the folder when it is ours to create. For Desktop
	 * and Applications. */
	Result createInFolder(const Request& request);
} // namespace InstanceShortcuts
