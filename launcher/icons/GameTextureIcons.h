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

/* The game look for the built-in instance icons ("grass", "diamond",
 * "creeper", ...), read at runtime from a Minecraft client jar already
 * downloaded onto the user's disk. Kite never ships these textures (Mojang
 * owns them); without a jar nothing is produced and the built-in icon
 * stays.
 *
 * Each key maps to one texture, or for a mob to its entity texture's face
 * with overlay layers. Every pixel comes from the jar; pixelIcon() only
 * scales it. */
namespace GameTextureIcons
{
	/*! The newest release client jar under @p librariesDir
	 * (com/mojang/minecraft/<version>/minecraft-<version>-client.jar), or
	 * empty; snapshots and pre-releases are skipped. @p version, when given,
	 * receives the jar's version. */
	QString findClientJar(const QString& librariesDir,
						  QString* version = nullptr);

	/*! Every key's image read from the jar at @p jarPath (see
	 * GameTextureRecipes). A key whose textures the jar lacks is left out; an
	 * unreadable jar gives nothing. */
	QHash<QString, QImage> readFromJar(const QString& jarPath);

	/*! readFromJar() for the jar at @p jarPath of game @p version (both as
	 * findClientJar() gives them), through a per-version cache in @p cacheDir
	 * so each jar is opened once. Empty without a jar. */
	QHash<QString, QImage> load(const QString& jarPath, const QString& version,
								const QString& cacheDir);

	/// An icon of @p image at any size, scaled nearest-neighbour so a
	/// 16-pixel texture stays crisp.
	QIcon pixelIcon(const QImage& image);
} // namespace GameTextureIcons
