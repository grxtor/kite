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

#include "SkinFace.h"

#include <QPainter>

namespace SkinFace
{
	namespace
	{
		// Standard Minecraft skin texture layout.
		constexpr int kFaceX = 8;
		constexpr int kFaceY = 8;
		constexpr int kHatX = 40;
		constexpr int kHatY = 8;
		constexpr int kFaceExtent = 8;
	} // namespace

	QImage fromSkin(const QImage& skin)
	{
		if (skin.isNull() || skin.width() < kHatX + kFaceExtent ||
			skin.height() < kFaceY + kFaceExtent) {
			return QImage();
		}

		QImage face(kFaceExtent, kFaceExtent,
					QImage::Format_ARGB32_Premultiplied);
		face.fill(Qt::transparent);

		QPainter painter(&face);
		painter.drawImage(
			0, 0, skin.copy(kFaceX, kFaceY, kFaceExtent, kFaceExtent));
		painter.drawImage(
			0, 0, skin.copy(kHatX, kHatY, kFaceExtent, kFaceExtent));
		painter.end();
		return face;
	}
} // namespace SkinFace
