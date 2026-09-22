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

#include <QQuickAsyncImageProvider>
#include <QSize>
#include <QString>
#include <QThreadPool>

#include <memory>

class ScreenshotThumbnailCache;

/*
 * Decodes and scales screenshot thumbnails for QML, off the GUI thread. A
 * delegate writes
 *
 *     Image { source: "image://screenshot/" + encodeURIComponent(path) }
 *
 * where `path` is the absolute file path -- e.g. the `path` role of
 * ScreenshotListModel (see screenshots/ScreenshotListModel.h). encodeURIComponent
 * is plain QML/JS, available in any binding or delegate; it turns whatever
 * the OS allows in a path (spaces, '#', non-ASCII, ...) into a string that
 * survives being parsed as a URL. requestImageResponse() below reverses
 * exactly that with QUrl::fromPercentEncoding(), which is a no-op on a
 * string that has nothing left to decode -- safe either way, whether or not
 * Qt Quick's own URL handling has already undone some of the encoding by
 * the time `id` reaches this provider.
 *
 * THREADING. Unlike InstanceIconProvider/AccountFaceProvider (Pixmap-type,
 * GUI-thread-only -- see InstanceIconProvider.h for why), this is an async
 * provider: requestImageResponse() only creates and returns a
 * QQuickImageResponse, whose finished() signal QML waits on; the actual
 * QImage decode + scale runs on this provider's own QThreadPool (separate
 * from the global pool, so screenshot thumbnails never queue behind
 * unrelated work), on a worker QRunnable that hands its result back across
 * threads with a Qt::QueuedConnection. QImage (unlike QPixmap) is safe to
 * build off the GUI thread, which is what makes this split possible at all.
 *
 * SIZE. `requestedSize` is the box QML wants the thumbnail to fit inside,
 * already in device pixels. An invalid/empty one (QML did not set
 * sourceSize) falls back to a 256x256 box. Either way the source image is
 * scaled with Qt::KeepAspectRatio, so the longer side lands on the box's
 * matching dimension and the image is never stretched or cropped -- unlike
 * ScreenshotsPage.cpp's ThumbnailRunnable, which pads to a centered 256x256
 * square. Callers that want a uniform grid cell handle that in the QML
 * delegate (Image.fillMode: PreserveAspectFit/Crop), not here.
 *
 * CACHE. A small in-memory LRU, keyed by path + the file's mtime + the
 * requested box size, so re-showing the same thumbnail at the same size
 * (scrolling back up a grid) is instant and never re-decodes the PNG/JPEG.
 * The mtime in the key means a screenshot overwritten with new content
 * (same path) is simply a cache miss, not stale data -- no invalidation
 * logic needed. Shared (via std::shared_ptr) between the provider and every
 * in-flight worker rather than owned solely by the provider, so a response
 * that outlives the provider (e.g. during shutdown) never touches a freed
 * cache.
 *
 * MISSING/UNREADABLE FILES. Handled without throwing or crashing: a
 * response for a path that does not exist, is not a file, or fails to
 * decode as an image comes back with a null QQuickTextureFactory and a
 * non-empty errorString() -- QML sees Image.status == Image.Error, not a
 * broken provider.
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
