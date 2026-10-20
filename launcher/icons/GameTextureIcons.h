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

#include <QHash>
#include <QIcon>
#include <QImage>
#include <QString>
#include <QStringList>

/*
 * The real game look for the built-in instance icons ("grass", "diamond",
 * "creeper", ...), read at runtime from a Minecraft client jar the launcher
 * has already downloaded from Mojang onto the user's own disk.
 *
 * Kite never ships, commits or redistributes these textures: Mojang owns
 * them. They exist only on the user's machine -- inside the jar, and in a
 * small cache of the crops made from it -- the way map renderers read them.
 * Without a jar nothing here produces anything, and the built-in icon Kite
 * ships stays in place.
 *
 * Each key maps to one texture, or for a mob to the face of its entity
 * texture, overlay layers included (a player's hat, an enderman's eyes, a
 * chicken's beak). Nothing is drawn or shaded: every pixel comes from the
 * jar, and pixelIcon() only ever scales it with nearest-neighbour.
 */
namespace GameTextureIcons
{
	/// The built-in instance icon keys a game texture stands in for.
	QStringList keys();

	/*!
	 * The newest release client jar under @p librariesDir, laid out the way
	 * the launcher downloads it (com/mojang/minecraft/<version>/
	 * minecraft-<version>-client.jar), or empty when there is none.
	 * Snapshots and pre-releases are passed over. @p version, when given,
	 * receives the jar's version.
	 */
	QString findClientJar(const QString& librariesDir,
						  QString* version = nullptr);

	/*!
	 * Every key's image read from the jar at @p jarPath, at the texture's
	 * own resolution, padded to a square. A key whose textures this jar
	 * does not have is left out; an unreadable jar gives nothing at all.
	 */
	QHash<QString, QImage> readFromJar(const QString& jarPath);

	/*!
	 * readFromJar() for findClientJar(@p librariesDir), through a cache in
	 * @p cacheDir, one folder per game version, so each jar is opened once.
	 * Empty without a jar.
	 */
	QHash<QString, QImage> load(const QString& librariesDir,
								const QString& cacheDir);

	/// An icon of @p image at any size, scaled with nearest-neighbour so a
	/// 16-pixel texture stays pixel-crisp instead of being smoothed.
	QIcon pixelIcon(const QImage& image);
} // namespace GameTextureIcons
