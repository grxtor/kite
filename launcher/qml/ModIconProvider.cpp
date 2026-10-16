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

#include "ModIconProvider.h"

#include <QMutexLocker>

#include <algorithm>
#include <utility>

#include "minecraft/mod/ModIconCache.h"

namespace
{
	/* Fits @p image into @p box. Shrinking is smooth. Growing goes
	 * nearest-neighbour by the largest whole factor that fits, so pixel-art
	 * logos keep square pixels, and only the last fraction is smoothed. */
	QImage fitted(const QImage& image, const QSize& box)
	{
		if (image.width() > box.width() || image.height() > box.height()) {
			return image.scaled(box, Qt::KeepAspectRatio,
								Qt::SmoothTransformation);
		}
		const int factor = std::min(box.width() / image.width(),
									box.height() / image.height());
		QImage grown = factor > 1 ? image.scaled(image.size() * factor,
												 Qt::KeepAspectRatio,
												 Qt::FastTransformation)
								  : image;
		if (grown.size() == grown.size().scaled(box, Qt::KeepAspectRatio)) {
			return grown;
		}
		return grown.scaled(box, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
} // namespace

ModIconProvider::ModIconProvider(QString cacheDir)
	: QQuickImageProvider(QQuickImageProvider::Image,
						  QQmlImageProviderBase::ForceAsynchronousImageLoading),
	  m_cacheDir(std::move(cacheDir))
{
}

QImage ModIconProvider::load(const QString& key)
{
	QMutexLocker lock(&m_mutex);
	if (const QImage* hit = m_cache.object(key)) {
		return *hit;
	}
	lock.unlock();

	QImage image(ModIconCache::pathFor(m_cacheDir, key));
	if (image.isNull()) {
		return image;
	}

	lock.relock();
	m_cache.insert(key, new QImage(image));
	return image;
}

QImage ModIconProvider::requestImage(const QString& id, QSize* size,
									 const QSize& requestedSize)
{
	QImage image;
	if (ModIconCache::isValidKey(id)) {
		image = load(id);
	}

	// Only one of sourceSize's dimensions set means a square box.
	QSize box = requestedSize;
	if (box.width() <= 0) {
		box.setWidth(box.height());
	}
	if (box.height() <= 0) {
		box.setHeight(box.width());
	}
	if (!image.isNull() && !box.isEmpty()) {
		image = fitted(image, box);
	}

	if (size) {
		*size = image.size();
	}
	return image;
}
