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
};

QTEST_GUILESS_MAIN(AccountFaceProviderTest)

#include "AccountFaceProvider_test.moc"
