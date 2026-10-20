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
#include <QImage>
#include <QSet>
#include <QString>
#include <QStringList>

/*
 * Which game texture each built-in instance icon key is made from, and how:
 * a whole block or item texture, or for a mob the face of its entity
 * texture with its overlay layers over it. Paths are relative to a client
 * jar's texture root (assets/minecraft/textures/); reading the jar is
 * GameTextureIcons' job, this only crops and stacks what it was given.
 */
namespace GameTextureRecipes
{
	/// The built-in instance icon keys a game texture stands in for.
	QStringList keys();

	/// Every texture any key may be made from, in every layout listed.
	QSet<QString> texturePaths();

	/*!
	 * Each key's image made from @p textures (texture path to its image),
	 * at the texture's own resolution and padded to a square. A key whose
	 * main texture is missing, or too small for its crop, is left out.
	 */
	QHash<QString, QImage> compose(const QHash<QString, QImage>& textures);
} // namespace GameTextureRecipes
