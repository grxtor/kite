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

#include <QCache>
#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>
#include <QSize>
#include <QString>

/*
 * Serves the logos of installed mods and packs to QML:
 *
 *     Image { source: model.iconUrl }    // "image://modicon/<key>", or ""
 *
 * The key is a ModIconCache key; LocalModParseTask has already written the
 * logo to the on-disk cache, so this only reads a small PNG back. The key
 * changes whenever the file does, so there is nothing to cache-bust.
 *
 * Only keys are accepted, never paths: anything that is not a valid key comes
 * back as a null image, so a URL cannot read outside the cache directory. A
 * key whose file is gone is a null image too, and QML keeps its fallback glyph.
 *
 * Image provider with ForceAsynchronousImageLoading: requestImage() runs on
 * Qt Quick's image loader thread and touches only QImage. The in-memory cache
 * is behind a mutex because several requests can be in flight.
 *
 * requestedSize is in device pixels. Larger logos are scaled down smoothly;
 * smaller ones (mostly 16-64 px pixel art) are scaled up nearest-neighbour by
 * the largest whole factor that fits, and only the remainder is smoothed. An
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
