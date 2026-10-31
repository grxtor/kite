/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

// One skin in the local skin library: a PNG plus the metadata the file cannot
// carry (arm width, cape, last uploaded texture URL). The file name without
// extension is the entry's identity. A plain value type: the library copies
// entries freely and hands out pointers into its list.
class SkinEntry
{
  public:
	// Arm width of the player model; the profile API spells these "CLASSIC" and
	// "SLIM" (see armsToken()).
	enum class Arms { Classic, Slim };

	SkinEntry() = default;

	explicit SkinEntry(const QString& pngPath);

	// Restores an entry from the library index; `libraryDir` holds the PNG.
	SkinEntry(const QDir& libraryDir, const QJsonObject& record);

	// Display name and library key: the file name minus ".png".
	QString name() const;

	QString path() const
	{
		return m_path;
	}

	// The normalised 64x64 texture; empty if the file is not a skin.
	QImage texture() const
	{
		return m_texture;
	}

	// Flat 36x36 front-and-back sprite.
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

	// "CLASSIC" or "SLIM", as the profile API spells it.
	QString armsToken() const;

	QString textureUrl() const
	{
		return m_textureUrl;
	}

	// The arm width the account was last seen wearing this skin with. A rescan
	// compares the profile against it, not arms(), to tell a change made
	// elsewhere from a width picked here and not worn yet.
	Arms wornArms() const
	{
		return m_wornArms;
	}

	// Brings @p image to 64x64 ARGB32 with an opaque base layer, widening a
	// legacy 64x32 skin. Non-skins come back untouched.
	static QImage normalisedTexture(const QImage& image);

	// Whether this is a skin: 64 pixels wide and 32 (legacy) or 64 tall.
	bool isUsable() const;

	// Renames the file on disk, and with it the entry's identity. Fails without
	// changes if the target name is taken.
	bool renameTo(const QString& newName);

	void setCapeId(const QString& capeId)
	{
		m_capeId = capeId;
	}

	// Also regenerates the thumbnail.
	void setArms(Arms arms);

	void setTextureUrl(const QString& url)
	{
		m_textureUrl = url;
	}

	void setWornArms(Arms arms)
	{
		m_wornArms = arms;
	}

	// Re-reads the PNG from disk.
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
