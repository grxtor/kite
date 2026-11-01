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

#include <QObject>
#include <QString>
#include <QVariantList>

#include <memory>

#include "minecraft/auth/MinecraftAccount.h"

class SkinLibrary;

/*
 * One account's skin library for the QML skin editor: the PNGs in the skins
 * folder (SkinsDir) plus adding skins from a file, a link or a player's name.
 * Wearing one goes through AccountsController::changeSkin(). A thin layer over
 * SkinLibrary (minecraft/skins), which watches the folder, so anything dropped
 * into it shows up here too.
 */
class SkinsController : public QObject
{
	Q_OBJECT

	/* One map per skin, in folder order: name, path, url (file url of the PNG),
	 * textureUrl (image://accountface/skinfile/... of the normalised 64x64
	 * texture, for 3D), thumbnailUrl (flat front-and-back sprite), bodyUrl (front
	 * view), slim, worn (what the account wears now). */
	Q_PROPERTY(QVariantList skins READ skins NOTIFY skinsChanged)
	Q_PROPERTY(QString directory READ directory CONSTANT)
	/* The game's nine default skins, apart from the user's own: per character
	 * name, slim, and variants [classic, slim], each a map like a skin above plus
	 * ready (false, with empty urls, until downloaded). AccountsController::skins()
	 * starts the downloads each time the editor opens. */
	Q_PROPERTY(QVariantList defaultSkins READ defaultSkins NOTIFY
				   defaultSkinsChanged)
	Q_PROPERTY(bool defaultSkinsLoading READ defaultSkinsLoading NOTIFY
				   defaultSkinsChanged)

  public:
	/* @p directory is the skins folder, created if missing. @p account decides
	 * which skin counts as worn; may be null. */
	SkinsController(MinecraftAccountPtr account, const QString& directory,
					QObject* parent = nullptr);
	~SkinsController() override;

	QVariantList skins() const;
	QString directory() const;
	QVariantList defaultSkins() const;
	bool defaultSkinsLoading() const;

	/* Copies a PNG (path or file url) into the library. Empty string, or why not. */
	Q_INVOKABLE QString importFile(const QString& fileUrlOrPath);
	/* Downloads a skin from an http(s) link. TaskWatcher (parented here), or null
	 * when @p link is not a link; the task fails when what arrives is not a skin. */
	Q_INVOKABLE QObject* importFromUrl(const QString& link);
	/* Fetches the skin another player wears, by name. TaskWatcher, or null for an empty name. */
	Q_INVOKABLE QObject* importFromUsername(const QString& username);
	/* Renames skin @p name (its file). False when @p newName is empty or taken. */
	Q_INVOKABLE bool rename(const QString& name, const QString& newName);
	/* Deletes skin @p name, to the trash where there is one. Empty string, or why
	 * not; the worn skin cannot be deleted. */
	Q_INVOKABLE QString remove(const QString& name);
	Q_INVOKABLE void setSlim(const QString& name, bool slim);
	Q_INVOKABLE void openFolder();
	/* Records that skin @p name is now worn, once changeSkin() has uploaded it; the
	 * library recognises it by texture url (SkinLibrary::indexOfAccountSkin()). */
	Q_INVOKABLE void markWorn(const QString& name);

  signals:
	void skinsChanged();
	void defaultSkinsChanged();

  private:
	bool isWorn(const QString& url) const;

	MinecraftAccountPtr m_account;
	std::unique_ptr<SkinLibrary> m_library;
};
