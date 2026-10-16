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

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QSize>
#include <QString>

/*
 * Serves the logos of installed mods and packs to QML. A delegate writes
 *
 *     Image { source: model.iconUrl }
 *
 * where iconUrl is ModFolderModel's role of that name:
 * "image://modicon/<key>", or "" for a file with no logo. The key is a
 * ModIconCache key (see minecraft/mod/ModIconCache.h) -- by the time a row
 * has one, LocalModParseTask has already pulled the logo out of the jar on
 * its worker thread and written it to the on-disk cache, so all this
 * provider ever does is read a small PNG back. Because the key changes
 * whenever the file does, a replaced jar is a new URL and QML re-fetches it
 * on its own; there is nothing to cache-bust.
 *
 * Only keys are accepted, never paths: anything that is not a well-formed
 * key (ModIconCache::isValidKey()) comes back as a null image, so a URL can
 * not be made to read a file outside the cache directory. A key whose file
 * is gone (cache cleared) is a null image as well -- QML sees
 * Image.status == Image.Error and the delegate keeps its fallback glyph.
 *
 * THREADING. An Image-type provider with ForceAsynchronousImageLoading, so
 * requestImage() always runs on Qt Quick's image loader thread rather than
 * blocking the GUI thread on disk reads; it only touches QImage, which is
 * safe there. The in-memory cache (decoded logos at their cached size) is
 * behind a mutex because several loader requests can be in flight at once.
 *
 * SIZE. requestedSize is in device pixels. Logos larger than it are scaled
 * down smoothly; smaller ones -- most mod logos are 16-64 px pixel art --
 * are scaled up nearest-neighbour by the largest whole factor that fits,
 * so they stay crisp, and only the remaining fraction is smoothed. An
 * empty requestedSize returns the logo at its cached size.
 */
class ModIconProvider : public QQuickImageProvider
{
  public:
	explicit ModIconProvider(QString cacheDir);

	QImage requestImage(const QString& id, QSize* size,
						const QSize& requestedSize) override;

  private:
	QImage load(const QString& key);

	static constexpr int kMaxCachedIcons = 256;

	QString m_cacheDir;
	QMutex m_mutex;
	QCache<QString, QImage> m_cache{kMaxCachedIcons};
};
