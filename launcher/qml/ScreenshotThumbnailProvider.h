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

#include <QQuickAsyncImageProvider>
#include <QSize>
#include <QString>
#include <QThreadPool>

#include <memory>

class ScreenshotThumbnailCache;

/*
 * Decodes and scales screenshot thumbnails for QML, off the GUI thread:
 *
 *     Image { source: "image://screenshot/" + encodeURIComponent(path) }
 *
 * `path` is the absolute file path. requestImageResponse() undoes the
 * encoding with QUrl::fromPercentEncoding(), a no-op on a string with nothing
 * left to decode, so it is safe whether or not Qt Quick already undid some.
 *
 * Async provider: the decode and scale run on the provider's own QThreadPool,
 * so thumbnails never queue behind unrelated work, and the result returns
 * across threads by a queued connection. QImage, unlike QPixmap, is safe to
 * build off the GUI thread.
 *
 * `requestedSize` is the box the thumbnail fits inside, in device pixels; an
 * empty one falls back to 256x256. The image keeps its aspect ratio and is
 * never stretched or cropped; a uniform grid cell is up to the QML delegate
 * (Image.fillMode).
 *
 * A small in-memory LRU is keyed by path + mtime + box size, so an overwritten
 * screenshot is a miss, not stale data. It is shared with in-flight workers
 * through a shared_ptr so a response outliving the provider never touches a
 * freed cache.
 *
 * A path that does not exist, is not a file or fails to decode yields a null
 * texture factory and a non-empty errorString(), so QML sees Image.Error.
 */
class ScreenshotThumbnailProvider : public QQuickAsyncImageProvider
{
  public:
	ScreenshotThumbnailProvider();
	~ScreenshotThumbnailProvider() override;

	QQuickImageResponse* requestImageResponse(
		const QString& id, const QSize& requestedSize) override;

  private:
	QThreadPool m_pool;
	std::shared_ptr<ScreenshotThumbnailCache> m_cache;
};
