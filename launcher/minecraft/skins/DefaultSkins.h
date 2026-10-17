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

#include <QImage>
#include <QObject>
#include <QString>

#include "minecraft/auth/MinecraftAccount.h"

class QNetworkReply;

/*
 * The game's two default skins, Steve (classic arms) and Alex (slim arms),
 * for a player who has no skin of their own: offline accounts, and a
 * Microsoft account before its profile texture has been fetched. The game
 * shows such a player in one of these rather than as a blank figure, and so
 * does the launcher (AccountFaceProvider draws faces, bodies and the 3D
 * preview's texture from them).
 *
 * The textures are Mojang's and are not shipped with MeshMC: they are
 * fetched from the same textures.minecraft.net addresses the profile parser
 * already substitutes for a player without a skin (Parsers.cpp), and kept
 * in the data folder's cache (cache/skins/default-classic.png and
 * default-slim.png) so they are there offline from then on; the cache is
 * read first. Until they are here -- no cache yet and no network --
 * texture() is null and callers keep their own fallback; loaded() says
 * when to look again.
 *
 * Core: QtCore/QtGui/QtNetwork only, no QtWidgets, no ui/.
 */
class DefaultSkins : public QObject
{
	Q_OBJECT

  public:
	/// The one instance, created on first use.
	static DefaultSkins* instance();

	/*!
	 * Whether the game gives @p uuid the slim (Alex) default rather than the
	 * classic (Steve) one: the parity of the Java hashCode() of the UUID --
	 * odd is slim -- the same rule Parsers::parseMojangSessionProfile()
	 * applies. Dashes are optional; anything that is not a UUID gets the
	 * classic model, as it does in the game.
	 */
	static bool isSlimFor(const QString& uuid);

	/*!
	 * Whether @p account is drawn with slim arms: what its profile says
	 * when it says anything, otherwise the default the game gives the UUID
	 * it launches with (isSlimFor() of MinecraftAccount::gameUuid()) -- for
	 * an offline account the name-based one, so the preview shows what the
	 * game will, even when that puts an offline "Steve" in Alex's skin.
	 */
	static bool isSlim(const MinecraftAccountPtr& account);

	/// The 64x64 default texture with slim (Alex) or classic (Steve) arms;
	/// null until it has been downloaded (see ensureLoaded()).
	QImage texture(bool slim) const;

	/// Reads whichever textures are still missing from the cache, and starts
	/// fetching any the cache does not have; a no-op while a fetch is
	/// running, once both are here, or without a LauncherContext (tests).
	void ensureLoaded();

	/// For tests: puts @p image in place as if it had been downloaded.
	void setTexture(bool slim, const QImage& image);

  signals:
	/// A texture arrived; anything drawn without it should be redrawn.
	void loaded();

  private:
	explicit DefaultSkins(QObject* parent = nullptr);
	void fetch(bool slim);
	void onFinished(QNetworkReply* reply, bool slim);
	/// Where the downloaded texture is kept, under the data folder.
	static QString cachePath(bool slim);
	bool loadCached(bool slim);

	QImage m_classic;
	QImage m_slim;
	bool m_fetchingClassic = false;
	bool m_fetchingSlim = false;
};
