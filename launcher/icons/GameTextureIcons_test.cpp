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

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "FileSystem.h"
#include "icons/GameTextureFixture.h"
#include "icons/GameTextureIcons.h"

using GameTextureFixture::filled;

namespace
{
/* Every pixel a colour of its own, so a crop can be checked against the
 * exact place it was taken from. */
QImage coordinates(int width, int height)
{
	QImage image(width, height, QImage::Format_ARGB32);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			image.setPixel(x, y, qRgba(x * 3, y * 3, 200, 255));
		}
	}
	return image;
}
} // namespace

/*
 * Covers GameTextureIcons and the recipes behind it against client jars
 * this test builds itself out of generated PNGs at the paths the game
 * uses (GameTextureFixture). How IconList shows the result is covered in
 * IconList_test.cpp.
 */
class GameTextureIconsTest : public QObject
{
	Q_OBJECT

  private slots:
	void init()
	{
		m_dir = std::make_unique<QTemporaryDir>();
		QVERIFY(m_dir->isValid());
	}

	void cleanup()
	{
		m_dir.reset();
	}

	void findsNewestReleaseJar()
	{
		for (const QString& version :
			 {QStringLiteral("1.12.2"), QStringLiteral("1.21.4"),
			  QStringLiteral("26.3"), QStringLiteral("26.4-snapshot-4"),
			  QStringLiteral("24w14a"), QStringLiteral("1.21-pre1")}) {
			writeJar(version, {});
		}
		// A version folder whose jar never arrived is not a candidate.
		QVERIFY(QDir().mkpath(libraries() +
							  QStringLiteral("/com/mojang/minecraft/27.1")));

		QString version;
		const QString jar =
			GameTextureIcons::findClientJar(libraries(), &version);
		QCOMPARE(version, QStringLiteral("26.3"));
		QCOMPARE(jar, jarPath(QStringLiteral("26.3")));
	}

	void modernLayout()
	{
		const QImage grass = filled(16, 16, qRgba(40, 160, 40, 255));
		const QImage star = filled(16, 16, qRgba(250, 250, 200, 255));
		writeJar(QStringLiteral("1.21.4"),
				 {{QStringLiteral("block/grass_block_side.png"), grass},
				  {QStringLiteral("item/nether_star.png"), star}});

		const auto images =
			GameTextureIcons::readFromJar(jarPath(QStringLiteral("1.21.4")));
		QCOMPARE(images.value(QStringLiteral("grass")), grass);
		QCOMPARE(images.value(QStringLiteral("netherstar")), star);
		QVERIFY(!images.contains(QStringLiteral("stone")));
	}

	void legacyLayout()
	{
		const QImage grass = filled(16, 16, qRgba(60, 140, 60, 255));
		const QImage beef = filled(16, 16, qRgba(120, 60, 30, 255));
		writeJar(QStringLiteral("1.12.2"),
				 {{QStringLiteral("blocks/grass_side.png"), grass},
				  {QStringLiteral("items/beef_cooked.png"), beef}});

		const auto images =
			GameTextureIcons::readFromJar(jarPath(QStringLiteral("1.12.2")));
		QCOMPARE(images.value(QStringLiteral("grass")), grass);
		QCOMPARE(images.value(QStringLiteral("meat")), beef);
	}

	void animatedTextureGivesItsFirstFrame()
	{
		QImage fire = filled(16, 64, qRgba(200, 0, 0, 255));
		const QImage firstFrame = filled(16, 16, qRgba(255, 160, 0, 255));
		for (int y = 0; y < 16; ++y) {
			for (int x = 0; x < 16; ++x) {
				fire.setPixel(x, y, firstFrame.pixel(x, y));
			}
		}
		writeJar(QStringLiteral("1.21.4"),
				 {{QStringLiteral("block/fire_0.png"), fire}});

		const auto images =
			GameTextureIcons::readFromJar(jarPath(QStringLiteral("1.21.4")));
		QCOMPARE(images.value(QStringLiteral("flame")), firstFrame);
	}

	void faceIsCroppedWithItsHat()
	{
		QImage skin = coordinates(64, 64);
		// The hat layer: see-through except for one pixel.
		for (int y = 8; y < 16; ++y) {
			for (int x = 40; x < 48; ++x) {
				skin.setPixel(x, y, qRgba(0, 0, 0, 0));
			}
		}
		const QRgb hat = qRgba(255, 0, 255, 255);
		skin.setPixel(41, 9, hat);
		writeJar(QStringLiteral("1.21.4"),
				 {{QStringLiteral("entity/player/wide/steve.png"), skin}});

		const QImage face =
			GameTextureIcons::readFromJar(jarPath(QStringLiteral("1.21.4")))
				.value(QStringLiteral("steve"));
		QCOMPARE(face.size(), QSize(8, 8));
		QCOMPARE(face.pixel(0, 0), skin.pixel(8, 8));
		QCOMPARE(face.pixel(7, 7), skin.pixel(15, 15));
		QCOMPARE(face.pixel(1, 1), hat);
	}

	void narrowFaceIsPaddedSquare()
	{
		const QImage chicken = coordinates(64, 32);
		writeJar(QStringLiteral("1.21.4"),
				 {{QStringLiteral("entity/chicken.png"), chicken}});

		const QImage face =
			GameTextureIcons::readFromJar(jarPath(QStringLiteral("1.21.4")))
				.value(QStringLiteral("chicken"));
		// 4x6 centred in 6x6: the head from (3,3), the beak's front from
		// (16,2) two rows down, the wattle's from (16,6) under it.
		QCOMPARE(face.size(), QSize(6, 6));
		QCOMPARE(qAlpha(face.pixel(0, 0)), 0);
		QCOMPARE(qAlpha(face.pixel(5, 5)), 0);
		QCOMPARE(face.pixel(1, 0), chicken.pixel(3, 3));
		QCOMPARE(face.pixel(1, 2), chicken.pixel(16, 2));
		QCOMPARE(face.pixel(2, 4), chicken.pixel(16, 6));
		QCOMPARE(face.pixel(1, 4), chicken.pixel(3, 7));
	}

	void noJarGivesNothing()
	{
		QVERIFY(GameTextureIcons::findClientJar(libraries()).isEmpty());
		QVERIFY(loadNewest().isEmpty());
		QVERIFY(!QFileInfo::exists(cache()));
	}

	void jarIsReadOnceThenCached()
	{
		const QImage first = filled(16, 16, qRgba(10, 20, 30, 255));
		writeJar(QStringLiteral("1.21.4"),
				 {{QStringLiteral("block/stone.png"), first}});
		QCOMPARE(loadNewest()
					 .value(QStringLiteral("stone")),
				 first);
		QVERIFY(QFileInfo::exists(cache() +
								  QStringLiteral("/1.21.4/stone.png")));

		// Same version, other pixels: what comes back is the cache's.
		writeJar(QStringLiteral("1.21.4"),
				 {{QStringLiteral("block/stone.png"),
				   filled(16, 16, qRgba(90, 90, 90, 255))}});
		QCOMPARE(loadNewest()
					 .value(QStringLiteral("stone")),
				 first);
	}

	void unreadableJarIsNotCached()
	{
		const QString jar = jarPath(QStringLiteral("1.21.4"));
		QVERIFY(FS::ensureFilePathExists(jar));
		QFile file(jar);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write("not a zip at all");
		file.close();

		QVERIFY(loadNewest().isEmpty());

		// Once the jar is whole, it is read after all.
		const QImage stone = filled(16, 16, qRgba(70, 70, 70, 255));
		writeJar(QStringLiteral("1.21.4"),
				 {{QStringLiteral("block/stone.png"), stone}});
		QCOMPARE(loadNewest()
					 .value(QStringLiteral("stone")),
				 stone);
	}

	void pixelIconScalesNearestNeighbour()
	{
		QImage image(2, 2, QImage::Format_ARGB32);
		image.setPixel(0, 0, qRgba(255, 0, 0, 255));
		image.setPixel(1, 0, qRgba(0, 255, 0, 255));
		image.setPixel(0, 1, qRgba(0, 0, 255, 255));
		image.setPixel(1, 1, qRgba(255, 255, 255, 255));
		const QIcon icon = GameTextureIcons::pixelIcon(image);

		const QImage scaled = icon.pixmap(QSize(64, 64), 1.0)
								  .toImage()
								  .convertToFormat(QImage::Format_ARGB32);
		QCOMPARE(scaled.size(), QSize(64, 64));
		// Hard edges: the last pixel of one block and the first of the
		// next are the two source colours, unblended.
		QCOMPARE(scaled.pixel(31, 0), image.pixel(0, 0));
		QCOMPARE(scaled.pixel(32, 0), image.pixel(1, 0));
		QCOMPARE(scaled.pixel(31, 63), image.pixel(0, 1));
		QCOMPARE(scaled.pixel(32, 63), image.pixel(1, 1));

		// At a device pixel ratio the pixmap is the device size.
		const QPixmap retina = icon.pixmap(QSize(16, 16), 2.0);
		QCOMPARE(retina.size(), QSize(32, 32));
		QCOMPARE(retina.devicePixelRatio(), 2.0);
	}

  private:
	QString libraries() const
	{
		return m_dir->filePath(QStringLiteral("libraries"));
	}

	QString cache() const
	{
		return m_dir->filePath(QStringLiteral("cache/gametextures"));
	}

	QString jarPath(const QString& version) const
	{
		return GameTextureFixture::jarPath(libraries(), version);
	}

	void writeJar(const QString& version,
				  const QList<QPair<QString, QImage>>& textures) const
	{
		QVERIFY(GameTextureFixture::writeJar(libraries(), version, textures));
	}

	/* What IconList does: the newest jar, through the cache. */
	QHash<QString, QImage> loadNewest() const
	{
		QString version;
		const QString jar =
			GameTextureIcons::findClientJar(libraries(), &version);
		return GameTextureIcons::load(jar, version, cache());
	}

	std::unique_ptr<QTemporaryDir> m_dir;
};

int main(int argc, char* argv[])
{
	// QIcon/QPixmap need a QGuiApplication; offscreen keeps this runnable on
	// a headless runner, as in IconList_test.cpp.
	qputenv("QT_QPA_PLATFORM", "offscreen");

	QGuiApplication app(argc, argv);

	GameTextureIconsTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "GameTextureIcons_test.moc"
