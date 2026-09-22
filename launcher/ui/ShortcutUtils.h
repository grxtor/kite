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

class QWidget;

/* Shortcuts that start one instance without going through the launcher
 * window first.
 *
 * FS::createShortcut() writes the file; everything here is the step before
 * that, which is entirely about the icon. The launcher knows an instance's
 * icon only as a theme key, and every platform's shell wants a different
 * image format on disk (.icns, .png, .ico), so the icon has to be rendered
 * into the instance folder before a shortcut can point at it.
 *
 * All of these report their own failures with a message box, since the
 * only caller is a dialog the user is looking at. */
namespace ShortcutUtils
{
	/** Everything one shortcut needs. Grouped because the three
	 *  placement variants below all forward the same set on. */
	struct Shortcut {
		/// Instance to launch. Nothing is written if this is null.
		BaseInstance* instance = nullptr;
		/// File name and window title of the shortcut.
		QString name;
		/// What the shortcut leads to, already translated, for the
		/// "Created a shortcut to this %1" messages: an instance, a
		/// world, or a server.
		QString targetString;
		/// Parent for the message boxes and the file dialog.
		QWidget* parent = nullptr;
		/// Appended after --launch, e.g. --server or --profile.
		QStringList extraArgs;
		/// Icon to render, or empty to use the instance's own.
		QString iconKey;
		/// Which of the three variants below the caller means. Also
		/// remembered on the instance once the shortcut is written.
		ShortcutTarget target = ShortcutTarget::Desktop;
	};

	/** Write @p shortcut to @p filePath, which carries no suffix --
	 *  FS::createShortcut() appends whatever this platform uses. */
	bool createInstanceShortcut(const Shortcut& shortcut,
								const QString& filePath);

	/** Write @p shortcut to the user's desktop. */
	bool createInstanceShortcutOnDesktop(const Shortcut& shortcut);

	/** Write @p shortcut where the platform lists installed
	 *  applications. */
	bool createInstanceShortcutInApplications(const Shortcut& shortcut);

	/** Ask the user where @p shortcut should go, then write it. Returns
	 *  false if the user cancelled, same as for a real failure. */
	bool createInstanceShortcutInOther(const Shortcut& shortcut);
} // namespace ShortcutUtils
