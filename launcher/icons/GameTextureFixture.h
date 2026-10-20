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

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QList>
#include <QPair>
#include <QString>

#include "FileSystem.h"
#include "MMCZip.h"

/*
 * For tests only: client jars built at runtime out of generated PNGs, laid
 * out where the launcher keeps a downloaded one. No Mojang texture is
 * involved anywhere -- only the paths are the game's.
 */
namespace GameTextureFixture
{
	inline QImage filled(int width, int height, QRgb colour)
	{
		QImage image(width, height, QImage::Format_ARGB32);
		image.fill(colour);
		return image;
	}

	inline QString jarPath(const QString& librariesDir, const QString& version)
	{
		return librariesDir + QStringLiteral("/com/mojang/minecraft/%1/"
											 "minecraft-%1-client.jar")
								  .arg(version);
	}

	/* A client jar for @p version under @p librariesDir holding @p textures,
	 * each under the game's texture root at the path it is paired with.
	 * False when it could not be written. */
	inline bool writeJar(const QString& librariesDir, const QString& version,
						 const QList<QPair<QString, QImage>>& textures)
	{
		const QString path = jarPath(librariesDir, version);
		if (!FS::ensureFilePathExists(path)) {
			return false;
		}
		QFile::remove(path);
		MMCZip::ZipWriter jar(path);
		// Something besides textures, as a real jar has.
		bool written =
			jar.open() &&
			jar.addFile(QStringLiteral("net/minecraft/client/Main.class"),
						QByteArray("\xca\xfe\xba\xbe"));
		for (const auto& texture : textures) {
			QByteArray png;
			QBuffer buffer(&png);
			buffer.open(QIODevice::WriteOnly);
			texture.second.save(&buffer, "PNG");
			written = written &&
					  jar.addFile(QStringLiteral("assets/minecraft/textures/") +
									  texture.first,
								  png);
		}
		return jar.close() && written;
	}
} // namespace GameTextureFixture
