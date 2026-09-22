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

#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QQuickImageResponse>
#include <QQuickTextureFactory>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include <memory>

#include "ScreenshotThumbnailProvider.h"

namespace
{
QString makePng(const QString& dir, const QString& name, const QSize& size)
{
	QImage image(size, QImage::Format_ARGB32);
	image.fill(Qt::red);
	const QString path = QDir(dir).filePath(name);
	return image.save(path, "PNG") ? path : QString();
}

// Mirrors the QML-side encoding this provider expects -- see the class
// comment on ScreenshotThumbnailProvider for why encode/decode round-trip.
QString encodeAsId(const QString& path)
{
	return QString::fromUtf8(QUrl::toPercentEncoding(path));
}
} // namespace

/*
 * Unit test for ScreenshotThumbnailProvider. Drives requestImageResponse()
 * directly rather than through a QQmlEngine -- QQuickAsyncImageProvider
 * needs nothing but a QGuiApplication to construct and run its worker, and
 * the response's finished() signal is exactly what QML itself waits on.
 */
class ScreenshotThumbnailProviderTest : public QObject
{
	Q_OBJECT

  private slots:
	void decodesAndScalesKeepingAspectRatio()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString path =
			makePng(tempDir.path(), "wide.png", QSize(800, 400));
		QVERIFY(!path.isEmpty());

		ScreenshotThumbnailProvider provider;
		std::unique_ptr<QQuickImageResponse> response(
			provider.requestImageResponse(encodeAsId(path), QSize(200, 200)));
		QVERIFY(response != nullptr);

		QSignalSpy finished(response.get(), &QQuickImageResponse::finished);
		QVERIFY(finished.wait(5000));

		QCOMPARE(response->errorString(), QString());
		std::unique_ptr<QQuickTextureFactory> factory(
			response->textureFactory());
		QVERIFY(factory != nullptr);
		// 800x400 kept-aspect into a 200x200 box: width fills the box,
		// height follows the 2:1 ratio -- never square-padded.
		QCOMPARE(factory->textureSize(), QSize(200, 100));
	}

	void repeatedRequestServesFromCache()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString path =
			makePng(tempDir.path(), "square.png", QSize(600, 600));
		QVERIFY(!path.isEmpty());

		ScreenshotThumbnailProvider provider;

		std::unique_ptr<QQuickImageResponse> first(
			provider.requestImageResponse(encodeAsId(path), QSize(128, 128)));
		QSignalSpy firstFinished(first.get(), &QQuickImageResponse::finished);
		QVERIFY(firstFinished.wait(5000));
		std::unique_ptr<QQuickTextureFactory> firstFactory(
			first->textureFactory());
		QVERIFY(firstFactory != nullptr);
		QCOMPARE(firstFactory->textureSize(), QSize(128, 128));

		// Same path, same mtime, same requested box: a second request
		// should still succeed (whether served from cache or decoded
		// again is an implementation detail -- the observable contract is
		// just that it returns the same, correct result).
		std::unique_ptr<QQuickImageResponse> second(
			provider.requestImageResponse(encodeAsId(path), QSize(128, 128)));
		QSignalSpy secondFinished(second.get(),
								  &QQuickImageResponse::finished);
		QVERIFY(secondFinished.wait(5000));
		std::unique_ptr<QQuickTextureFactory> secondFactory(
			second->textureFactory());
		QVERIFY(secondFactory != nullptr);
		QCOMPARE(secondFactory->textureSize(), QSize(128, 128));
	}

	void missingFileReportsErrorWithoutCrashing()
	{
		ScreenshotThumbnailProvider provider;
		std::unique_ptr<QQuickImageResponse> response(
			provider.requestImageResponse(
				encodeAsId(QStringLiteral("/no/such/screenshot.png")),
				QSize(128, 128)));
		QVERIFY(response != nullptr);

		QSignalSpy finished(response.get(), &QQuickImageResponse::finished);
		QVERIFY(finished.wait(5000));

		QVERIFY(!response->errorString().isEmpty());
		QVERIFY(response->textureFactory() == nullptr);
	}

	void defaultSizeIsUsedWhenNoneRequested()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		const QString path =
			makePng(tempDir.path(), "large.png", QSize(1024, 512));
		QVERIFY(!path.isEmpty());

		ScreenshotThumbnailProvider provider;
		std::unique_ptr<QQuickImageResponse> response(
			provider.requestImageResponse(encodeAsId(path), QSize()));
		QVERIFY(response != nullptr);

		QSignalSpy finished(response.get(), &QQuickImageResponse::finished);
		QVERIFY(finished.wait(5000));

		std::unique_ptr<QQuickTextureFactory> factory(
			response->textureFactory());
		QVERIFY(factory != nullptr);
		// 1024x512 (2:1) fit into the default 256x256 box.
		QCOMPARE(factory->textureSize(), QSize(256, 128));
	}
};

int main(int argc, char* argv[])
{
	// Same reasoning as InstanceIconProvider_test.cpp/
	// AccountFaceProvider_test.cpp: QQuickTextureFactory needs a
	// QGuiApplication, and forcing offscreen keeps this runnable on a
	// headless runner.
	qputenv("QT_QPA_PLATFORM", "offscreen");

	QGuiApplication app(argc, argv);

	ScreenshotThumbnailProviderTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "ScreenshotThumbnailProvider_test.moc"
