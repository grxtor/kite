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

#include <QString>
#include <QStringList>

// For ShortcutTarget, which the instance stores alongside the path so
// that it can clean its own shortcuts up.
#include "BaseInstance.h"

/* Shortcuts that start one instance without going through the launcher
 * window first: the platform half, shared by both user interfaces.
 *
 * FS::createShortcut() writes the file; everything here is the step before
 * that, which is mostly about the icon. The launcher knows an instance's
 * icon only as a theme key, and every platform's shell wants a different
 * image format on disk (.icns, .png, .ico), so the icon has to be rendered
 * into the instance folder before a shortcut can point at it.
 *
 * Nothing here talks to the user. Every call hands back a Result whose
 * `error` is already worded for them, and the caller decides how to show
 * it: ui/ShortcutUtils in a message box, InstanceShortcutController as a
 * line in the QML dialog. This used to be ui/ShortcutUtils alone, where
 * the message boxes kept it out of the QML shell's reach. */
namespace InstanceShortcuts
{
	/** Everything one shortcut needs. */
	struct Request {
		/// Instance to launch. Nothing is written if this is null.
		BaseInstance* instance = nullptr;
		/// File name and window title of the shortcut.
		QString name;
		/// What the shortcut leads to, already translated, for the
		/// "Could not create a shortcut to this %1" message: an instance,
		/// a world, or a server.
		QString targetString;
		/// Appended after --launch, e.g. --server or --profile.
		QStringList extraArgs;
		/// Icon to render, or empty to use the instance's own.
		QString iconKey;
		/// Where it goes. Also remembered on the instance once the
		/// shortcut is written.
		ShortcutTarget target = ShortcutTarget::Desktop;
	};

	/** How it went. */
	struct Result {
		bool ok = false;
		/// The file (or, on macOS, the .app bundle) that was written.
		QString path;
		/// Empty on success, a sentence for the user otherwise.
		QString error;
	};

	/** The folder @p target writes into, or empty when this system has
	 *  none. Not created here. Other has no folder of its own: the caller
	 *  asks the user. */
	QString folderFor(ShortcutTarget target);

	/** The suffix FS::createShortcut() gives the file on this platform,
	 *  dot included, or empty where it is not one a user would type (the
	 *  `.app` of a macOS bundle). For a save dialog's filter and default
	 *  suffix. */
	QString fileSuffix();

	/** @p path as create() wants it: without the suffix FS::createShortcut()
	 *  adds itself (fileSuffix(), or a macOS bundle's `.app`), in case the
	 *  user typed it into a save dialog. */
	QString withoutSuffix(QString path);

	/** Write @p request to @p filePath, which carries no suffix --
	 *  FS::createShortcut() appends whatever this platform uses. */
	Result create(const Request& request, const QString& filePath);

	/** Write @p request into folderFor(request.target), named after
	 *  request.name, creating the folder when it is ours to create. For
	 *  Desktop and Applications. */
	Result createInFolder(const Request& request);
} // namespace InstanceShortcuts
