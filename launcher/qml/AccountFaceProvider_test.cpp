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

#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include "AccountFaceProvider.h"

namespace
{
// The layout AccountFaceProvider.cpp composites from, restated rather than
// included so the test checks the coordinates independently.
constexpr int kSkinExtent = 64;
constexpr int kFaceX = 8;
constexpr int kFaceY = 8;
constexpr int kHatX = 40;
constexpr int kHatY = 8;
constexpr int kFaceExtent = 8;

// A synthetic skin: the face region filled with faceColor and, if hatOpaque,
// the hat region with hatColor; otherwise the hat stays transparent.
QImage makeSkin(const QColor& faceColor, const QColor& hatColor,
			   bool hatOpaque)
{
	QImage skin(kSkinExtent, kSkinExtent, QImage::Format_ARGB32_Premultiplied);
	skin.fill(Qt::transparent);

	for (int y = 0; y < kFaceExtent; ++y) {
		for (int x = 0; x < kFaceExtent; ++x) {
			skin.setPixelColor(kFaceX + x, kFaceY + y, faceColor);
		}
	}

	if (hatOpaque) {
		for (int y = 0; y < kFaceExtent; ++y) {
			for (int x = 0; x < kFaceExtent; ++x) {
				skin.setPixelColor(kHatX + x, kHatY + y, hatColor);
			}
		}
	}

	return skin;
}

// The front-view body layout, restated like the constants above.
constexpr int kLimbHeight = 12;
constexpr int kTorsoWidth = 8;
constexpr int kLegWidth = 4;
constexpr int kClassicArmWidth = 4;
constexpr int kSlimArmWidth = 3;

// A synthetic skin with the head, torso, right arm and right leg each in a
// flat colour of their own, so a misplaced region is obvious. @p height is 64
// for the modern format (with a distinct left arm/leg) or 32 for the legacy
// one (mirrored by bodyFromSkin()).
QImage makeBodySkin(int height)
{
	QImage skin(kSkinExtent, height, QImage::Format_ARGB32_Premultiplied);
	skin.fill(Qt::transparent);

	auto fill = [&](int x, int y, int w, int h, const QColor& color) {
		for (int yy = 0; yy < h; ++yy) {
			for (int xx = 0; xx < w; ++xx) {
				skin.setPixelColor(x + xx, y + yy, color);
			}
		}
	};

	fill(kFaceX, kFaceY, kFaceExtent, kFaceExtent, Qt::red);      // head
	fill(20, 20, kTorsoWidth, kLimbHeight, Qt::blue);              // torso
	fill(44, 20, kClassicArmWidth, kLimbHeight, Qt::green);        // right arm
	fill(4, 20, kLegWidth, kLimbHeight, Qt::yellow);               // right leg

	if (height >= 64) {
		fill(36, 52, kClassicArmWidth, kLimbHeight, Qt::cyan);     // left arm
		fill(20, 52, kLegWidth, kLimbHeight, Qt::magenta);         // left leg
	}

	return skin;
}
} // namespace

/*
 * Unit test for the pure helpers faceFromSkin() and bodyFromSkin(), and for
 * skinTexture() with the skin library's file ids only. requestPixmap() needs a
 * real LauncherContext and account, which is neither cheap nor hermetic.
 */
class AccountFaceProviderTest : public QObject
{
	Q_OBJECT

  private slots:
	void faceIsEightByEight()
	{
		const QImage skin =
			makeSkin(Qt::red, Qt::blue, /*hatOpaque=*/false);

		const QImage face = AccountFaceProvider::faceFromSkin(skin);

		QVERIFY(!face.isNull());
		QCOMPARE(face.size(), QSize(kFaceExtent, kFaceExtent));
	}

	void transparentHatDoesNotCoverFace()
	{
		const QColor faceColor(255, 0, 0, 255);
		const QImage skin = makeSkin(faceColor, Qt::blue, /*hatOpaque=*/false);

		const QImage face = AccountFaceProvider::faceFromSkin(skin);

		for (int y = 0; y < kFaceExtent; ++y) {
			for (int x = 0; x < kFaceExtent; ++x) {
				QCOMPARE(face.pixelColor(x, y), faceColor);
			}
		}
	}

	void opaqueHatCoversFace()
	{
		const QColor faceColor(255, 0, 0, 255);
		const QColor hatColor(0, 0, 255, 255);
		const QImage skin = makeSkin(faceColor, hatColor, /*hatOpaque=*/true);

		const QImage face = AccountFaceProvider::faceFromSkin(skin);

		for (int y = 0; y < kFaceExtent; ++y) {
			for (int x = 0; x < kFaceExtent; ++x) {
				QCOMPARE(face.pixelColor(x, y), hatColor);
			}
		}
	}

	// Per-pixel alpha compositing: only the one opaque hat pixel shows through.
	void partiallyTransparentHatBlendsPerPixel()
	{
		const QColor faceColor(255, 0, 0, 255);
		const QColor hatPixelColor(0, 255, 0, 255);
		QImage skin = makeSkin(faceColor, Qt::blue, /*hatOpaque=*/false);
		skin.setPixelColor(kHatX + 3, kHatY + 3, hatPixelColor);

		const QImage face = AccountFaceProvider::faceFromSkin(skin);

		QCOMPARE(face.pixelColor(3, 3), hatPixelColor);
		QCOMPARE(face.pixelColor(0, 0), faceColor);
		QCOMPARE(face.pixelColor(7, 7), faceColor);
	}

	void tooSmallSkinReturnsNullImage()
	{
		QImage tiny(16, 16, QImage::Format_ARGB32_Premultiplied);
		tiny.fill(Qt::transparent);

		QVERIFY(AccountFaceProvider::faceFromSkin(tiny).isNull());
	}

	void nullSkinReturnsNullImage()
	{
		QVERIFY(AccountFaceProvider::faceFromSkin(QImage()).isNull());
	}

	void bodyIsClassicAspectByDefault()
	{
		const QImage skin = makeBodySkin(64);

		const QImage body = AccountFaceProvider::bodyFromSkin(skin, /*slim=*/false);

		QVERIFY(!body.isNull());
		QCOMPARE(body.size(),
				 QSize(kClassicArmWidth * 2 + kTorsoWidth,
					   kFaceExtent + kLimbHeight * 2));
	}

	void slimBodyIsNarrower()
	{
		const QImage skin = makeBodySkin(64);

		const QImage body = AccountFaceProvider::bodyFromSkin(skin, /*slim=*/true);

		QVERIFY(!body.isNull());
		QCOMPARE(body.width(), kSlimArmWidth * 2 + kTorsoWidth);
		QCOMPARE(body.height(), kFaceExtent + kLimbHeight * 2);
	}

	// The character faces the viewer, so its right arm/leg (the ones every
	// skin carries) render on the left, with the torso right after them.
	void rightArmAndLegLandLeftOfTorso()
	{
		const QImage skin = makeBodySkin(64);

		const QImage body = AccountFaceProvider::bodyFromSkin(skin, /*slim=*/false);

		QCOMPARE(body.pixelColor(0, kFaceExtent), QColor(Qt::green)); // arm
		QCOMPARE(body.pixelColor(kClassicArmWidth, kFaceExtent),
				 QColor(Qt::blue)); // torso starts right after it
		QCOMPARE(body.pixelColor(kClassicArmWidth, kFaceExtent + kLimbHeight),
				 QColor(Qt::yellow)); // leg, under the torso's left half
	}

	void modernSkinUsesItsOwnLeftArmAndLeg()
	{
		const QImage skin = makeBodySkin(64);

		const QImage body = AccountFaceProvider::bodyFromSkin(skin, /*slim=*/false);

		const int leftArmX = kClassicArmWidth + kTorsoWidth;
		const int leftLegX = kClassicArmWidth + kLegWidth;
		QCOMPARE(body.pixelColor(leftArmX, kFaceExtent), QColor(Qt::cyan));
		QCOMPARE(body.pixelColor(leftLegX, kFaceExtent + kLimbHeight),
				 QColor(Qt::magenta));
	}

	// The legacy 64x32 format has no left arm/leg texture, so the left side is
	// mirrored; an asymmetric arm tells a mirror from a copy.
	void legacySkinMirrorsRightArmAndLegForLeft()
	{
		QImage skin = makeBodySkin(32);
		skin.setPixelColor(44, 20, Qt::red);                    // leftmost column
		skin.setPixelColor(44 + kClassicArmWidth - 1, 20, Qt::blue); // rightmost

		const QImage body = AccountFaceProvider::bodyFromSkin(skin, /*slim=*/false);

		QVERIFY(!body.isNull());
		const int leftArmX = kClassicArmWidth + kTorsoWidth;
		// Mirrored: the arm's rightmost column becomes the leftmost.
		QCOMPARE(body.pixelColor(leftArmX, kFaceExtent), QColor(Qt::blue));
		QCOMPARE(body.pixelColor(leftArmX + kClassicArmWidth - 1, kFaceExtent),
				 QColor(Qt::red));
	}

	void bodyHeadIncludesHatOverlay()
	{
		QImage skin = makeBodySkin(64);
		skin.setPixelColor(kHatX, kHatY, QColor(0, 0, 0, 255));

		const QImage body = AccountFaceProvider::bodyFromSkin(skin, /*slim=*/false);

		QCOMPARE(body.pixelColor(kClassicArmWidth, 0), QColor(0, 0, 0, 255));
	}

	void tooSmallSkinReturnsNullBody()
	{
		QImage tiny(16, 16, QImage::Format_ARGB32_Premultiplied);
		tiny.fill(Qt::transparent);

		QVERIFY(AccountFaceProvider::bodyFromSkin(tiny, false).isNull());
	}

	void nullSkinReturnsNullBody()
	{
		QVERIFY(AccountFaceProvider::bodyFromSkin(QImage(), false).isNull());
	}

	// Ids carry the whole path percent-encoded, so spaces or a '#' must
	// survive; a legacy 64x32 skin comes out as 64x64.
	void skinFileTextureSurvivesItsUrl()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = dir.filePath(QStringLiteral("my skins #2/Old one.png"));
		QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
		QImage legacy(64, 32, QImage::Format_ARGB32);
		legacy.fill(QColor(30, 120, 60));
		QVERIFY(legacy.save(path, "PNG"));
		const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(path));

		const QImage texture =
			AccountFaceProvider::skinTexture(QStringLiteral("skinfile/") + encoded);
		QCOMPARE(texture.size(), QSize(64, 64));
		QVERIFY(!AccountFaceProvider::skinTexture(QStringLiteral("skinthumb/") + encoded).isNull());
	}

	// The sprite's back right arm starts a pixel further in with slim arms, so
	// its first column is empty.
	void slimThumbnailHasSlimArms()
	{
		QTemporaryDir dir;
		const QString path = dir.filePath(QStringLiteral("skin.png"));
		QImage skin(64, 64, QImage::Format_ARGB32);
		skin.fill(QColor(200, 150, 100));
		QVERIFY(skin.save(path, "PNG"));
		const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(path));
		const QPoint backRightArm(20, 10);

		const QImage classic =
			AccountFaceProvider::skinTexture(QStringLiteral("skinthumb/") + encoded);
		const QImage slim =
			AccountFaceProvider::skinTexture(QStringLiteral("skinthumbslim/") + encoded);
		QCOMPARE(classic.pixelColor(backRightArm).alpha(), 255);
		QCOMPARE(slim.pixelColor(backRightArm).alpha(), 0);
	}

	void notASkinFileHasNoTexture()
	{
		QTemporaryDir dir;
		const QString path = dir.filePath(QStringLiteral("big.png"));
		QImage big(128, 128, QImage::Format_ARGB32);
		big.fill(Qt::red);
		QVERIFY(big.save(path, "PNG"));
		const QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(path));

		QVERIFY(AccountFaceProvider::skinTexture(QStringLiteral("skinfile/") + encoded).isNull());
		QVERIFY(AccountFaceProvider::skinTexture(QStringLiteral("face/") + encoded).isNull());
	}
};

QTEST_GUILESS_MAIN(AccountFaceProviderTest)

#include "AccountFaceProvider_test.moc"
