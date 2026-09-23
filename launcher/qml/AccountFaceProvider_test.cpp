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

#include <QColor>
#include <QImage>
#include <QtTest>

#include "AccountFaceProvider.h"

namespace
{
// Same layout AccountFaceProvider.cpp composites from -- restated here
// rather than included, so the test proves the coordinates against a fresh
// reading of "what a real skin texture looks like", not against whatever the
// implementation happens to believe them to be.
constexpr int kSkinExtent = 64;
constexpr int kFaceX = 8;
constexpr int kFaceY = 8;
constexpr int kHatX = 40;
constexpr int kHatY = 8;
constexpr int kFaceExtent = 8;

// A synthetic skin texture: the face region filled with faceColor, and
// (optionally) the hat region filled with hatColor. When hatOpaque is
// false the hat region is left fully transparent, which is what
// QImage::fill(Qt::transparent) already puts there.
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

// Same front-view body layout AccountFaceProvider.cpp composites from,
// restated here for the same reason kSkinExtent etc. above are.
constexpr int kLimbHeight = 12;
constexpr int kTorsoWidth = 8;
constexpr int kLegWidth = 4;
constexpr int kClassicArmWidth = 4;
constexpr int kSlimArmWidth = 3;

// A synthetic full skin texture: head, torso, right arm and right leg each
// filled with their own flat colour, so a region landing in the wrong place
// in the composited body is obvious. @p height is 64 for the modern format
// (which also gets a distinctly-coloured left arm/leg) or 32 for the legacy
// one (which has neither, and relies on bodyFromSkin() mirroring the right
// side instead).
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
 * Unit test for AccountFaceProvider::faceFromSkin(), the pure
 * face-compositing helper. Deliberately does not exercise
 * AccountFaceProvider::requestPixmap() itself: that needs a real
 * LauncherContext/AccountList/MinecraftAccount (network-backed, or at least
 * a real Application), which is neither cheap nor hermetic here. The helper
 * is where the interesting logic (and the interesting bug -- a hat pixel
 * clobbering the face where it should be transparent) actually lives.
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

	// Proves per-pixel alpha compositing, not an all-or-nothing overlay
	// draw: only the one opaque hat pixel should show through, everything
	// else stays the face colour.
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

	// --- AccountFaceProvider::bodyFromSkin() ---

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

	// The character faces the viewer, so its right arm/leg -- the ones a
	// skin texture always carries, legacy or modern -- render on the left
	// of the composited image, and the torso sits right after them.
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

	// The legacy 64x32 format has no separate left arm/leg texture at all,
	// so bodyFromSkin() must synthesise the left side by mirroring the
	// right -- checked here with an asymmetric arm so a mirror and a plain
	// copy cannot be confused for each other.
	void legacySkinMirrorsRightArmAndLegForLeft()
	{
		QImage skin = makeBodySkin(32);
		skin.setPixelColor(44, 20, Qt::red);                    // leftmost column
		skin.setPixelColor(44 + kClassicArmWidth - 1, 20, Qt::blue); // rightmost

		const QImage body = AccountFaceProvider::bodyFromSkin(skin, /*slim=*/false);

		QVERIFY(!body.isNull());
		const int leftArmX = kClassicArmWidth + kTorsoWidth;
		// Mirrored: the arm's rightmost column becomes the leftmost here,
		// and vice versa.
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
};

QTEST_GUILESS_MAIN(AccountFaceProviderTest)

#include "AccountFaceProvider_test.moc"
