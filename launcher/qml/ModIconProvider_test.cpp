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

#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include "minecraft/mod/ModIconCache.h"
#include "qml/ModIconProvider.h"

namespace
{
	QByteArray encoded(const QImage& image)
	{
		QByteArray bytes;
		QBuffer buffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		image.save(&buffer, "PNG");
		return bytes;
	}

	// 2x2, one colour per pixel, so scaling mode is visible pixel by pixel.
	QImage quad()
	{
		QImage image(2, 2, QImage::Format_ARGB32);
		image.setPixelColor(0, 0, Qt::red);
		image.setPixelColor(1, 0, Qt::green);
		image.setPixelColor(0, 1, Qt::blue);
		image.setPixelColor(1, 1, Qt::white);
		return image;
	}

	const QString kKey = QStringLiteral("0123456789abcdef0123456789abcdef01234567");
	const QString kBigKey =
		QStringLiteral("fedcba9876543210fedcba9876543210fedcba98");
} // namespace

class ModIconProviderTest : public QObject
{
	Q_OBJECT

  private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
		QVERIFY(ModIconCache::store(m_dir.path(), kKey, encoded(quad())));
		QImage big(128, 128, QImage::Format_ARGB32);
		big.fill(Qt::red);
		QVERIFY(ModIconCache::store(m_dir.path(), kBigKey, encoded(big)));
	}

	void loadsOnTheImageLoaderThread()
	{
		ModIconProvider provider(m_dir.path());
		QCOMPARE(provider.imageType(), QQuickImageProvider::Image);
		QVERIFY(provider.flags() &
				QQmlImageProviderBase::ForceAsynchronousImageLoading);
	}

	void cachedSizeWithoutRequestedSize()
	{
		ModIconProvider provider(m_dir.path());
		QSize size;
		const QImage image = provider.requestImage(kKey, &size, QSize());
		QCOMPARE(image.size(), QSize(2, 2));
		QCOMPARE(size, QSize(2, 2));
	}

	void smallLogosGrowWithoutBlurring()
	{
		ModIconProvider provider(m_dir.path());
		QSize size;
		const QImage image = provider.requestImage(kKey, &size, QSize(8, 8));
		QCOMPARE(size, QSize(8, 8));
		// Nearest-neighbour by a whole factor: each source pixel is a
		// clean 4x4 block, edges included.
		QCOMPARE(image.pixelColor(0, 0), QColor(Qt::red));
		QCOMPARE(image.pixelColor(3, 3), QColor(Qt::red));
		QCOMPARE(image.pixelColor(4, 0), QColor(Qt::green));
		QCOMPARE(image.pixelColor(3, 4), QColor(Qt::blue));
		QCOMPARE(image.pixelColor(4, 4), QColor(Qt::white));
	}

	void largeLogosShrinkToTheBox()
	{
		ModIconProvider provider(m_dir.path());
		QSize size;
		provider.requestImage(kBigKey, &size, QSize(36, 36));
		QCOMPARE(size, QSize(36, 36));
	}

	void oneDimensionMeansASquareBox()
	{
		ModIconProvider provider(m_dir.path());
		QSize size;
		provider.requestImage(kBigKey, &size, QSize(40, 0));
		QCOMPARE(size, QSize(40, 40));
	}

	void rejectsAnythingButAKey()
	{
		ModIconProvider provider(m_dir.path());
		const QString outside = m_dir.filePath("outside.png");
		QVERIFY(QFile::copy(ModIconCache::pathFor(m_dir.path(), kKey), outside));

		for (const QString& id :
			 {QString(), QStringLiteral("../outside"), outside,
			  QStringLiteral("outside"), kKey + QStringLiteral("?rev=1")}) {
			QSize size(1, 1);
			const QImage image = provider.requestImage(id, &size, QSize(8, 8));
			QVERIFY2(image.isNull(), qPrintable(id));
			QCOMPARE(size, QSize(0, 0));
		}
	}

	void missingCacheFileIsNull()
	{
		ModIconProvider provider(m_dir.path());
		QSize size;
		const QImage image = provider.requestImage(
			QString(40, QLatin1Char('a')), &size, QSize(8, 8));
		QVERIFY(image.isNull());
	}

	void servesFromMemoryOnceLoaded()
	{
		QTemporaryDir dir;
		QVERIFY(ModIconCache::store(dir.path(), kKey, encoded(quad())));
		ModIconProvider provider(dir.path());
		QSize size;
		QVERIFY(!provider.requestImage(kKey, &size, QSize()).isNull());
		QVERIFY(QFile::remove(ModIconCache::pathFor(dir.path(), kKey)));
		QVERIFY(!provider.requestImage(kKey, &size, QSize()).isNull());
	}

  private:
	QTemporaryDir m_dir;
};

QTEST_GUILESS_MAIN(ModIconProviderTest)

#include "ModIconProvider_test.moc"
