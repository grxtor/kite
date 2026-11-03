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

#include <QMap>
#include <QString>
#include <QtGlobal>

// What a recommended-instance preset sets besides its mods: the game's memory
// and the first options.txt. Plain functions, testable without an instance.
// The memory pair is also the launcher-wide default.
namespace PresetDefaults
{
	/// The most memory (MiB) the game gets on a machine with @p systemRamMiB:
	/// 3 GiB for 8 GB, 4 GiB for 16 GB, 6 GiB for 32 GB and more, never more
	/// than half the RAM (1 GiB at the least).
	int maxMemoryMiB(quint64 systemRamMiB);

	/// The memory (MiB) the game starts with on that machine; below
	/// maxMemoryMiB().
	int minMemoryMiB(quint64 systemRamMiB);

	/// Whether @p mcVersion reads the pre-1.13 options.txt keys: any 1.x
	/// below 1.13, a weekly snapshot before 17w43a, and anything else that
	/// is not a plain release number ("b1.7.3", "a1.2.6").
	bool usesLegacyOptions(const QString& mcVersion);

	/// Writes @p options as `key:value` lines to <gameRoot>/options.txt, only
	/// when that file does not exist yet. True when it was written.
	bool writeOptions(const QString& gameRoot,
					  const QMap<QString, QString>& options);
} // namespace PresetDefaults
