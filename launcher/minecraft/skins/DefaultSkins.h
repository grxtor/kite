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

#include <QImage>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include "minecraft/auth/MinecraftAccount.h"

class QNetworkReply;

// The game's default skins: Steve (classic arms) and Alex (slim) for a player
// with no skin of their own (offline accounts, or a Microsoft account before
// its profile texture arrives). Mojang's textures are not shipped: they are
// fetched from the textures.minecraft.net addresses the profile parser
// substitutes (Parsers.cpp) and cached in cache/skins/default-classic.png and
// default-slim.png, read first. Until one is here texture() is null and callers
// keep their own fallback; loaded() says when to look again.
//
// Also the gallery: the nine default skins since 1.19.3 (Steve, Alex, Ari, Efe,
// Kai, Makena, Noor, Sunny, Zuri), each for both arm widths, fetched on demand
// (fetchGallery()) into cache/skins/default/<texture hash>.png.
//
// Core: QtCore/QtGui/QtNetwork only, no QtWidgets, no ui/.
class DefaultSkins : public QObject
{
	Q_OBJECT

  public:
	/// The one instance, created on first use.
	static DefaultSkins* instance();

	/*!
	 * Whether the game gives @p uuid the slim (Alex) default: odd parity of the
	 * Java hashCode() of the UUID, as Parsers::parseMojangSessionProfile()
	 * applies. Dashes are optional; anything that is not a UUID gets the
	 * classic model.
	 */
	static bool isSlimFor(const QString& uuid);

	/*!
	 * Whether @p account is drawn with slim arms: what its profile says,
	 * otherwise the game's default for the UUID it launches with (isSlimFor()
	 * of MinecraftAccount::gameUuid()).
	 */
	static bool isSlim(const MinecraftAccountPtr& account);

	/// The 64x64 default texture with slim (Alex) or classic (Steve) arms; null
	/// until downloaded (see ensureLoaded()).
	QImage texture(bool slim) const;

	/// Reads the missing textures from the cache and fetches those it lacks. A
	/// no-op while a fetch runs, once both are here, or without a
	/// LauncherContext (tests).
	void ensureLoaded();

	/// For tests: puts @p image in place as if it had been downloaded.
	void setTexture(bool slim, const QImage& image);

	/// The textures.minecraft.net address of the Steve (classic) or Alex (slim)
	/// default (MHF_Steve/MHF_Alex).
	static QString fallbackUrl(bool slim);

	/// One of the gallery's nine characters.
	struct Character {
		QString name;
		/// The arm width the game draws it with by default.
		bool slim = false;
	};
	/// The nine, in the order the game lists them.
	static QList<Character> characters();
	/// The textures.minecraft.net address of @p name with slim or classic arms;
	/// empty for a name not in characters().
	static QString galleryUrl(const QString& name, bool slim);
	/// Where that texture is kept once downloaded, named by its hash; empty for
	/// an unknown name.
	static QString galleryPath(const QString& name, bool slim);
	/// Downloads the gallery textures the cache lacks; galleryChanged() as each
	/// lands or fails. A no-op for those already on their way, and without a
	/// LauncherContext (tests).
	void fetchGallery();
	/// Whether any gallery download is still running.
	bool galleryLoading() const;

  signals:
	/// A texture arrived; anything drawn without it should be redrawn.
	void loaded();
	/// A gallery download started, landed or failed.
	void galleryChanged();

  private:
	explicit DefaultSkins(QObject* parent = nullptr);
	void fetch(bool slim);
	void onFinished(QNetworkReply* reply, bool slim);
	void onGalleryFinished(QNetworkReply* reply, const QString& path);
	/// Where the downloaded texture is kept, under the data folder.
	static QString cachePath(bool slim);
	bool loadCached(bool slim);

	QImage m_classic;
	QImage m_slim;
	bool m_fetchingClassic = false;
	bool m_fetchingSlim = false;
	/// galleryPath()s being downloaded right now.
	QSet<QString> m_galleryFetching;
};
