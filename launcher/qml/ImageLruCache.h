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
#include <QMutexLocker>
#include <QString>

/* QCache keeps no locking of its own, and image-loader workers look up and
 * insert concurrently. Capacity is an entry count, not a byte budget. */
class ImageLruCache
{
  public:
	explicit ImageLruCache(int maxEntries) : m_cache(maxEntries)
	{
	}

	bool get(const QString& key, QImage& out)
	{
		QMutexLocker lock(&m_mutex);
		if (const QImage* hit = m_cache.object(key)) {
			out = *hit;
			return true;
		}
		return false;
	}

	void insert(const QString& key, const QImage& image)
	{
		QMutexLocker lock(&m_mutex);
		m_cache.insert(key, new QImage(image));
	}

  private:
	QMutex m_mutex;
	QCache<QString, QImage> m_cache;
};
