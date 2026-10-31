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

#include "ScreenshotThumbnailProvider.h"

#include <QCache>
#include <QFileInfo>
#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QQuickTextureFactory>
#include <QRunnable>
#include <QUrl>

#include <utility>

namespace
{
// Fallback for an invalid or empty requestedSize.
constexpr int kDefaultThumbnailExtent = 256;

QSize normalizedBox(const QSize& requestedSize)
{
	return requestedSize.isValid() && !requestedSize.isEmpty()
			   ? requestedSize
			   : QSize(kDefaultThumbnailExtent, kDefaultThumbnailExtent);
}

QString cacheKeyFor(const QString& path, qint64 mtimeMs, const QSize& box)
{
	return QStringLiteral("%1@%2@%3x%4")
		.arg(path)
		.arg(mtimeMs)
		.arg(box.width())
		.arg(box.height());
}
} // namespace

/* QCache keeps no locking of its own, and the pool's workers look up and
 * insert concurrently. Capacity is an entry count, not a byte budget. */
class ScreenshotThumbnailCache
{
  public:
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
	static constexpr int kMaxEntries = 200;
	QMutex m_mutex;
	QCache<QString, QImage> m_cache{kMaxEntries};
};

namespace
{
/* Runs on a QThreadPool worker: stats the file, checks the cache, decodes
 * and scales on a miss. Touches only QImage, never QPixmap/QIcon. */
class ScreenshotThumbnailRunnable : public QObject, public QRunnable
{
	Q_OBJECT
  public:
	ScreenshotThumbnailRunnable(
		QString path, QSize requestedSize,
		std::shared_ptr<ScreenshotThumbnailCache> cache)
		: m_path(std::move(path)), m_requestedSize(requestedSize),
		  m_cache(std::move(cache))
	{
	}

	void run() override
	{
		const QSize box = normalizedBox(m_requestedSize);

		const QFileInfo info(m_path);
		if (!info.exists() || !info.isFile()) {
			emit done(QImage(),
					  QStringLiteral("screenshot not found: %1").arg(m_path));
			return;
		}

		const qint64 mtimeMs = info.lastModified().toMSecsSinceEpoch();
		const QString key = cacheKeyFor(m_path, mtimeMs, box);

		QImage cached;
		if (m_cache->get(key, cached)) {
			emit done(cached, QString());
			return;
		}

		QImage image(m_path);
		if (image.isNull()) {
			emit done(QImage(), QStringLiteral("could not decode image: %1")
									.arg(m_path));
			return;
		}

		const QImage scaled = image.scaled(box, Qt::KeepAspectRatio,
											Qt::SmoothTransformation);
		m_cache->insert(key, scaled);
		emit done(scaled, QString());
	}

  signals:
	// Queued across to the response's thread.
	void done(QImage image, QString error);

  private:
	QString m_path;
	QSize m_requestedSize;
	std::shared_ptr<ScreenshotThumbnailCache> m_cache;
};

class ScreenshotImageResponse : public QQuickImageResponse
{
	Q_OBJECT
  public:
	ScreenshotImageResponse(const QString& path, const QSize& requestedSize,
							std::shared_ptr<ScreenshotThumbnailCache> cache,
							QThreadPool* pool)
	{
		auto* runnable = new ScreenshotThumbnailRunnable(
			path, requestedSize, std::move(cache));
		// autoDelete is fine: the queued connection copies its arguments
		// before run() returns.
		connect(runnable, &ScreenshotThumbnailRunnable::done, this,
				&ScreenshotImageResponse::handleDone, Qt::QueuedConnection);
		pool->start(runnable);
	}

	QQuickTextureFactory* textureFactory() const override
	{
		return m_image.isNull()
				   ? nullptr
				   : QQuickTextureFactory::textureFactoryForImage(m_image);
	}

	QString errorString() const override
	{
		return m_error;
	}

  private slots:
	void handleDone(QImage image, QString error)
	{
		m_image = std::move(image);
		m_error = std::move(error);
		emit finished();
	}

  private:
	QImage m_image;
	QString m_error;
};
} // namespace

ScreenshotThumbnailProvider::ScreenshotThumbnailProvider()
	: m_cache(std::make_shared<ScreenshotThumbnailCache>())
{
	// A private pool avoids contending with the global one other subsystems
	// share.
	m_pool.setMaxThreadCount(4);
}

ScreenshotThumbnailProvider::~ScreenshotThumbnailProvider()
{
	// Let in-flight work finish rather than destroying the pool under running
	// QRunnables.
	m_pool.waitForDone(500);
}

QQuickImageResponse* ScreenshotThumbnailProvider::requestImageResponse(
	const QString& id, const QSize& requestedSize)
{
	const QString path = QUrl::fromPercentEncoding(id.toUtf8());
	return new ScreenshotImageResponse(path, requestedSize, m_cache, &m_pool);
}

#include "ScreenshotThumbnailProvider.moc"
