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

#include <QFileInfo>
#include <QStringList>

// Structural recognition for pack formats that sit next to mods but carry no
// mod metadata: for shader and data packs only the internal directory layout
// tells a usable file from a random zip. Cheap on purpose: it reads the
// archive's central directory (or a few QFileInfo stats for exploded folders)
// and extracts nothing, so it is safe on the GUI thread.
namespace PackLayout
{

	// True when `file` (a directory or zip) is an OptiFine/Iris shader pack: it
	// has a `shaders` directory at its root or exactly one level down (packs
	// from a Git forge are wrapped in one top-level folder).
	bool isShaderPack(const QFileInfo& file);

	// True when `file` is a resource pack: a `pack.mcmeta` beside an `assets`
	// directory, at the root or one level down.
	bool isResourcePack(const QFileInfo& file);

	// True when `file` is a vanilla data pack: a `pack.mcmeta` beside a `data`
	// directory, at the root or one level down.
	bool isDataPack(const QFileInfo& file);

	// The entries of the zip at `zipPath` with slashes forward and no leading
	// "./" or "/"; directories keep their trailing slash.
	QStringList archiveEntries(const QString& zipPath);

} // namespace PackLayout
