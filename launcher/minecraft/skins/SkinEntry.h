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

#include <QDir>
#include <QImage>
#include <QJsonObject>
#include <QString>

/* One skin in the local skin library.
 *
 * A skin is a PNG file on disk plus three pieces of metadata that the file
 * itself cannot carry: which arm width the player model should use, which
 * cape was worn with it, and the profile texture URL it was last uploaded
 * as. The URL is what lets the library recognise "this saved skin is the
 * one the account is currently wearing" after a restart, since the account
 * only ever tells us a URL.
 *
 * The file name (without extension) is the identity of the entry: it is the
 * display name, the key the library and the dialog pass around, and what
 * renaming changes. Nothing else is unique -- two files can easily hold the
 * same pixels.
 *
 * Deliberately a plain value type, not a QObject: the library keeps these in
 * a QList and hands out raw pointers into it, and copies are made freely
 * while rebuilding that list.
 */
class SkinEntry
{
  public:
	/* Arm width of the player model this texture was drawn for.
	 *
	 * Named after what it actually controls rather than after Steve and
	 * Alex, because the mapping between the two is a UI label, not a fact
	 * about the texture. The wire format spells these "CLASSIC" and "SLIM";
	 * see armsToken(). */
	enum class Arms { Classic, Slim };

	SkinEntry() = default;

	/* Adopt a PNG that is already sitting in the library directory. */
	explicit SkinEntry(const QString& pngPath);

	/* Restore an entry from the library index. `record` supplies the
	 * metadata, `libraryDir` is where the PNG named in it lives. */
	SkinEntry(const QDir& libraryDir, const QJsonObject& record);

	/* Display name and library key: the file name minus ".png". */
	QString name() const;

	QString path() const
	{
		return m_path;
	}

	/* The normalised 64x64 texture. Empty if the file could not be read or
	 * is not a skin; see isUsable(). */
	QImage texture() const
	{
		return m_texture;
	}

	/* Flat 36x36 front-and-back sprite, for the list view and for the
	 * fallback preview when there is no OpenGL. */
	QImage thumbnail() const
	{
		return m_thumbnail;
	}

	QString capeId() const
	{
		return m_capeId;
	}

	Arms arms() const
	{
		return m_arms;
	}

	/* The spelling the profile API uses: "CLASSIC" or "SLIM". */
	QString armsToken() const;

	QString textureUrl() const
	{
		return m_textureUrl;
	}

	/* The arm width the account was last seen wearing this skin with, for
	 * the entry whose textureUrl() the account wears. A rescan compares the
	 * profile against it, not against arms(), so it can tell a change made
	 * elsewhere (in game, on another machine) from a width picked here and
	 * not worn yet. */
	Arms wornArms() const
	{
		return m_wornArms;
	}

	/* @p image in the one shape every texture here has: 64x64, ARGB32,
	 * base layer opaque, a legacy 64x32 skin widened (its left limbs
	 * mirrored from the right). Anything that is not a skin comes back
	 * untouched. For skins that are not files, such as an account's. */
	static QImage normalisedTexture(const QImage& image);

	/* Whether this is a skin at all: 64 pixels wide and either 32 (legacy)
	 * or 64 pixels tall. Everything else -- HD skins, arbitrary images, a
	 * PNG that failed to decode -- is rejected. */
	bool isUsable() const;

	/* Rename the file on disk, and with it this entry's identity.
	 *
	 * Fails without touching anything if the target name is already taken,
	 * so a rename can never silently swallow another skin. */
	bool renameTo(const QString& newName);

	void setCapeId(const QString& capeId)
	{
		m_capeId = capeId;
	}

	/* Also regenerates the thumbnail: arm width changes which columns of
	 * the texture the arms are drawn from. */
	void setArms(Arms arms);

	void setTextureUrl(const QString& url)
	{
		m_textureUrl = url;
	}

	void setWornArms(Arms arms)
	{
		m_wornArms = arms;
	}

	/* Re-read the PNG from disk. Used when the directory watcher reports
	 * that the file changed underneath us. */
	void reload();

	QJsonObject toRecord() const;

  private:
	QString m_path;
	QImage m_texture;
	QImage m_thumbnail;
	QString m_capeId;
	Arms m_arms = Arms::Classic;
	QString m_textureUrl;
	Arms m_wornArms = Arms::Classic;
};
