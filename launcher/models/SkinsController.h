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

#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>

#include "minecraft/auth/MinecraftAccount.h"

class SkinLibrary;

/*
 * One account's skin library for the QML skin editor: the PNGs in the
 * launcher's skins folder (SkinsDir), what the widget SkinManageDialog
 * kept -- list, rename, delete, arm width, open the folder -- plus adding
 * skins from a file, a link or another player's name. Wearing one goes
 * through AccountsController::changeSkin() with the entry's file and arms.
 *
 * A thin layer over SkinLibrary (minecraft/skins), which watches the
 * folder: anything dropped into it shows up here too, and imports only
 * have to put a file in place.
 */
class SkinsController : public QObject
{
	Q_OBJECT

	/* One map per skin, in the folder's order: name, path, url (a file
	 * url of the PNG), textureUrl (an image://accountface/skinfile/... url
	 * of its normalised 64x64 texture, for a 3D preview), thumbnailUrl
	 * (its flat front-and-back sprite, with its arm width), bodyUrl (its
	 * front view, for a
	 * flat preview), slim, worn (whether it is what the account wears
	 * now). */
	Q_PROPERTY(QVariantList skins READ skins NOTIFY skinsChanged)
	Q_PROPERTY(QString directory READ directory CONSTANT)
	/* The game's nine default skins (DefaultSkins' gallery), apart from
	 * the user's own: one map per character with name, slim (the arm width
	 * it is drawn with by default) and variants -- [classic, slim], each a
	 * map like a skin above (path, textureUrl, thumbnailUrl, bodyUrl, slim,
	 * worn) plus ready, false until it has been downloaded (the urls are
	 * empty until then). AccountsController::skins() starts the downloads
	 * each time the editor opens (DefaultSkins::fetchGallery()). */
	Q_PROPERTY(QVariantList defaultSkins READ defaultSkins NOTIFY
				   defaultSkinsChanged)
	/// Whether any default skin is still downloading.
	Q_PROPERTY(bool defaultSkinsLoading READ defaultSkinsLoading NOTIFY
				   defaultSkinsChanged)

  public:
	/* @p directory is the skins folder; it is created if missing.
	 * @p account decides which skin counts as worn; it may be null. */
	SkinsController(MinecraftAccountPtr account, const QString& directory,
					QObject* parent = nullptr);
	~SkinsController() override;

	QVariantList skins() const;
	QString directory() const;
	QVariantList defaultSkins() const;
	bool defaultSkinsLoading() const;

	/* Copies a PNG (a path or a file url) into the library. Returns an
	 * empty string, or why not, in words. */
	Q_INVOKABLE QString importFile(const QString& fileUrlOrPath);
	/* Downloads a skin from a link (http:// or https://, written out)
	 * into the library.
	 * Returns a TaskWatcher (parented here), or null when @p link is not
	 * a link at all. The task fails, with a reason, when what arrives is
	 * not a skin. */
	Q_INVOKABLE QObject* importFromUrl(const QString& link);
	/* Fetches the skin another player wears, by name, the way the widget
	 * dialog's "Import from user" did. Returns a TaskWatcher, or null for
	 * an empty name. */
	Q_INVOKABLE QObject* importFromUsername(const QString& username);
	/* Renames skin @p name (its file) to @p newName. False when the new
	 * name is empty or already taken. */
	Q_INVOKABLE bool rename(const QString& name, const QString& newName);
	/* Deletes skin @p name, to the trash where there is one. Returns an
	 * empty string, or why not -- the skin the account wears now cannot be
	 * deleted, as in the widget dialog. */
	Q_INVOKABLE QString remove(const QString& name);
	/// The arm width skin @p name was drawn for, remembered in the library.
	Q_INVOKABLE void setSlim(const QString& name, bool slim);
	Q_INVOKABLE void openFolder();
	/* Records that skin @p name is now what the account wears, once
	 * AccountsController::changeSkin() has uploaded it and refreshed the
	 * account -- the skin's texture url is how the library recognises it
	 * (see SkinLibrary::indexOfAccountSkin()). */
	Q_INVOKABLE void markWorn(const QString& name);

	/// Turns a path or file url into a local path; the input otherwise.
	static QString localPath(const QString& fileUrlOrPath);

  signals:
	void skinsChanged();
	void defaultSkinsChanged();

  private:
	/// Whether the account wears the texture at @p url.
	bool isWorn(const QString& url) const;

	MinecraftAccountPtr m_account;
	std::unique_ptr<SkinLibrary> m_library;
};
