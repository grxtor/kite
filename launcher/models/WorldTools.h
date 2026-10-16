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

/*
 * Installing a world from a .zip (or folder) and opening one in an external
 * editor: the widget-free halves of WorldListPage (drop / "Add" and
 * on_actionMCEdit_triggered()), which also ask the safety question and show
 * errors in message boxes - the QML Worlds tab does both itself.
 *
 * The MCEdit lookup mirrors MCEditTool::getProgramPath() for a given
 * MCEditPath; it is kept separate because MCEditTool registers the
 * MCEditPath setting on construction and the application already owns the
 * one instance of that.
 */
namespace WorldTools
{
	/// Whether @p source - a .zip or a folder - holds a Minecraft world (a
	/// level.dat). Reads only the archive's table of contents and level.dat.
	bool isWorld(const QString& source);

	/// Installs the world at @p source into @p worldsDir under a folder
	/// named after the world, the way WorldList::installWorld() does, but
	/// reporting the result. False if @p source is not a world or could not
	/// be unpacked. Blocking: run it off the GUI thread for a big world.
	bool installWorld(const QString& source, const QString& worldsDir);

	/// The executable to start for an MCEdit installed at @p mceditPath (the
	/// MCEditPath setting), or an empty string if there is none there.
	QString mceditProgram(const QString& mceditPath);

	/// Starts MCEdit on the world at @p worldFolder, detached. Returns an
	/// empty string on success, otherwise a sentence fit to show the user.
	QString openInMCEdit(const QString& mceditPath, const QString& worldFolder);
} // namespace WorldTools
