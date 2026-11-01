/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include "SkinEntry.h"

#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QVarLengthArray>

// Every coordinate is transcribed from the skin format as data, so it can be
// checked line by line.

namespace
{
	// Forces a rectangle fully opaque, in place. Includes the last row and
	// column: QRect::right()/bottom() are inclusive.
	void fillAlpha(QImage& image, const QRect& region, int alpha)
	{
		const QRect clipped =
			region.intersected(QRect(0, 0, image.width(), image.height()));
		for (int y = clipped.top(); y <= clipped.bottom(); ++y) {
			QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
			for (int x = clipped.left(); x <= clipped.right(); ++x) {
				const QRgb pixel = line[x];
				line[x] =
					qRgba(qRed(pixel), qGreen(pixel), qBlue(pixel), alpha);
			}
		}
	}

	// The rectangles of a 64x64 skin that belong to the base layer.
	const QRect kBaseLayerRegions[] = {
		QRect(0, 0, 32, 16),   /* head */
		QRect(0, 16, 64, 16),  /* torso, right arm, right leg */
		QRect(16, 48, 32, 16), /* left arm, left leg */
	};

	// The quadrant a legacy 64x32 skin leaves unused; it becomes the second
	// layer once widened.
	const QRect kLegacyUnusedQuadrant(32, 0, 32, 32);

	// Left-limb copies for widening a legacy 64x32 skin, which only has the
	// right arm and leg. Each row mirrors the (x, y, width, height) rectangle
	// and draws it at (x + offsetX, y + offsetY).
	struct LegacyLimbCopy {
		int x;
		int y;
		int offsetX;
		int offsetY;
		int width;
		int height;
	};

	const LegacyLimbCopy kLegacyLimbCopies[] = {
		/* right leg -> left leg */
		{4, 16, 16, 32, 4, 4},
		{8, 16, 16, 32, 4, 4},
		{0, 20, 24, 32, 4, 12},
		{4, 20, 16, 32, 4, 12},
		{8, 20, 8, 32, 4, 12},
		{12, 20, 16, 32, 4, 12},
		/* right arm -> left arm */
		{44, 16, -8, 32, 4, 4},
		{48, 16, -8, 32, 4, 4},
		{40, 20, 0, 32, 4, 12},
		{44, 20, -8, 32, 4, 12},
		{48, 20, -16, 32, 4, 12},
		{52, 20, -8, 32, 4, 12},
	};

	void makeBaseLayerOpaque(QImage& skin)
	{
		for (const QRect& region : kBaseLayerRegions) {
			fillAlpha(skin, region, 255);
		}
	}

	// A legacy skin's unused quadrant may hold opaque garbage that would render
	// as a shell once widened. As the game does, keep it only if some pixel in
	// it is transparent (an authored second layer); otherwise clear it.
	void clearUnusedLegacyOverlay(QImage& skin)
	{
		for (int y = kLegacyUnusedQuadrant.top();
			 y <= kLegacyUnusedQuadrant.bottom(); ++y) {
			const QRgb* line = reinterpret_cast<const QRgb*>(skin.scanLine(y));
			for (int x = kLegacyUnusedQuadrant.left();
				 x <= kLegacyUnusedQuadrant.right(); ++x) {
				if (qAlpha(line[x]) < 128) {
					// Authored second layer: leave it.
					return;
				}
			}
		}
		fillAlpha(skin, kLegacyUnusedQuadrant, 0);
	}

	QImage widenLegacyTexture(const QImage& legacy)
	{
		QImage widened(64, 64, legacy.format());
		widened.fill(Qt::transparent);
		{
			QPainter painter(&widened);
			painter.setCompositionMode(QPainter::CompositionMode_Source);
			painter.drawImage(0, 0, legacy);
		}

		// Copy everything out before painting: copy() on an image with a live
		// QPainter is unsafe.
		struct PendingCopy {
			QPoint target;
			QImage pixels;
		};
		QVarLengthArray<PendingCopy, 12> pending;

		for (const LegacyLimbCopy& copy : kLegacyLimbCopies) {
			const QImage face =
				widened.copy(copy.x, copy.y, copy.width, copy.height)
					.mirrored(true, false);
			pending.append(
				{QPoint(copy.x + copy.offsetX, copy.y + copy.offsetY), face});
		}

		{
			QPainter painter(&widened);
			painter.setCompositionMode(QPainter::CompositionMode_Source);
			for (const PendingCopy& copy : pending) {
				painter.drawImage(copy.target, copy.pixels);
			}
		}

		clearUnusedLegacyOverlay(widened);
		return widened;
	}

	// Brings a skin to 64x64 ARGB32 with an opaque base layer. Non-skins are
	// returned untouched so isUsable() sees their original size.
	QImage normaliseTexture(QImage skin)
	{
		const int width = skin.width();
		const int height = skin.height();
		if (width != 64 || (height != 32 && height != 64)) {
			return skin;
		}

		// The scanline code reads QRgb and QPainter cannot paint on paletted
		// images.
		if (skin.format() != QImage::Format_ARGB32) {
			skin = skin.convertToFormat(QImage::Format_ARGB32);
		}

		if (height == 32) {
			skin = widenLegacyTexture(skin);
		}
		makeBaseLayerOpaque(skin);
		return skin;
	}

	QImage loadNormalisedTexture(const QString& path)
	{
		return normaliseTexture(QImage(path));
	}

	// Flat 36x36 sprite: front on the left half, back on the right, base layer
	// then second layer. The draw order matters (arms over torso), so it is a
	// sequence of draws.
	QImage renderThumbnail(const QImage& texture, bool slim)
	{
		QImage sprite(36, 36, QImage::Format_ARGB32);
		sprite.fill(Qt::transparent);
		if (texture.isNull()) {
			return sprite;
		}

		// A slim arm is 3 pixels wide and sits a pixel further in.
		const int armWidth = slim ? 3 : 4;
		const int armX = slim ? 1 : 0;

		QPainter paint(&sprite);

		/* front */
		/* head */
		paint.drawImage(4, 2, texture.copy(8, 8, 8, 8));
		paint.drawImage(4, 2, texture.copy(40, 8, 8, 8));
		/* torso */
		paint.drawImage(4, 10, texture.copy(20, 20, 8, 12));
		paint.drawImage(4, 10, texture.copy(20, 36, 8, 12));
		/* right leg */
		paint.drawImage(4, 22, texture.copy(4, 20, 4, 12));
		paint.drawImage(4, 22, texture.copy(4, 36, 4, 12));
		/* left leg */
		paint.drawImage(8, 22, texture.copy(20, 52, 4, 12));
		paint.drawImage(8, 22, texture.copy(4, 52, 4, 12));
		/* right arm */
		paint.drawImage(armX, 10, texture.copy(44, 20, armWidth, 12));
		paint.drawImage(armX, 10, texture.copy(44, 36, armWidth, 12));
		/* left arm */
		paint.drawImage(12, 10, texture.copy(36, 52, armWidth, 12));
		paint.drawImage(12, 10, texture.copy(52, 52, armWidth, 12));

		/* back */
		/* head */
		paint.drawImage(24, 2, texture.copy(24, 8, 8, 8));
		paint.drawImage(24, 2, texture.copy(56, 8, 8, 8));
		/* torso */
		paint.drawImage(24, 10, texture.copy(32, 20, 8, 12));
		paint.drawImage(24, 10, texture.copy(32, 36, 8, 12));
		/* right leg */
		paint.drawImage(24, 22, texture.copy(12, 20, 4, 12));
		paint.drawImage(24, 22, texture.copy(12, 36, 4, 12));
		/* left leg */
		paint.drawImage(28, 22, texture.copy(28, 52, 4, 12));
		paint.drawImage(28, 22, texture.copy(12, 52, 4, 12));
		/* right arm */
		paint.drawImage(armX + 20, 10,
						texture.copy(48 + armWidth, 20, armWidth, 12));
		paint.drawImage(armX + 20, 10,
						texture.copy(48 + armWidth, 36, armWidth, 12));
		/* left arm */
		paint.drawImage(32, 10, texture.copy(40 + armWidth, 52, armWidth, 12));
		paint.drawImage(32, 10, texture.copy(56 + armWidth, 52, armWidth, 12));

		return sprite;
	}
} // namespace

SkinEntry::SkinEntry(const QString& pngPath)
	: m_path(pngPath), m_texture(loadNormalisedTexture(pngPath)),
	  m_arms(Arms::Classic)
{
	// Nothing on disk says which arm width a bare PNG uses: start Classic.
	m_thumbnail = renderThumbnail(m_texture, false);
}

SkinEntry::SkinEntry(const QDir& libraryDir, const QJsonObject& record)
	: m_capeId(record.value(QStringLiteral("capeId")).toString()),
	  m_arms(Arms::Classic),
	  m_textureUrl(record.value(QStringLiteral("url")).toString())
{
	if (record.value(QStringLiteral("model")).toString() ==
		QLatin1String("SLIM")) {
		m_arms = Arms::Slim;
	}
	// Indexes from before "wornModel" kept the worn skin's width in step with
	// the profile.
	const QString wornModel =
		record.value(QStringLiteral("wornModel")).toString();
	m_wornArms = wornModel.isEmpty()
					 ? m_arms
					 : (wornModel == QLatin1String("SLIM") ? Arms::Slim
														   : Arms::Classic);

	// The index stores the display name, not a path: the library dir is
	// relocatable.
	const QString name = record.value(QStringLiteral("name")).toString();
	m_path = libraryDir.absoluteFilePath(name + QStringLiteral(".png"));

	m_texture = loadNormalisedTexture(m_path);
	m_thumbnail = renderThumbnail(m_texture, m_arms == Arms::Slim);
}

QString SkinEntry::name() const
{
	return QFileInfo(m_path).completeBaseName();
}

QString SkinEntry::armsToken() const
{
	switch (m_arms) {
		case Arms::Classic:
			return QStringLiteral("CLASSIC");
		case Arms::Slim:
			return QStringLiteral("SLIM");
	}
	return QString();
}

bool SkinEntry::isUsable() const
{
	if (m_texture.isNull()) {
		return false;
	}
	if (m_texture.width() != 64) {
		return false;
	}
	// 64 after normalising; 32 is accepted so a legacy file that skipped
	// widening still counts as a skin.
	const int height = m_texture.height();
	return height == 64 || height == 32;
}

bool SkinEntry::renameTo(const QString& newName)
{
	const QFileInfo current(m_path);
	const QString target =
		QDir(current.absolutePath())
			.absoluteFilePath(newName + QStringLiteral(".png"));

	if (QFileInfo::exists(target)) {
		// Never overwrite: the other file is somebody's skin.
		return false;
	}
	if (!QFile::rename(current.absoluteFilePath(), target)) {
		return false;
	}
	m_path = target;
	return true;
}

void SkinEntry::setArms(Arms arms)
{
	m_arms = arms;
	m_thumbnail = renderThumbnail(m_texture, m_arms == Arms::Slim);
}

void SkinEntry::reload()
{
	m_texture = loadNormalisedTexture(m_path);
	m_thumbnail = renderThumbnail(m_texture, m_arms == Arms::Slim);
}

QJsonObject SkinEntry::toRecord() const
{
	QJsonObject record;
	record[QStringLiteral("name")] = name();
	record[QStringLiteral("capeId")] = m_capeId;
	record[QStringLiteral("url")] = m_textureUrl;
	record[QStringLiteral("model")] = armsToken();
	record[QStringLiteral("wornModel")] = m_wornArms == Arms::Slim
											  ? QStringLiteral("SLIM")
											  : QStringLiteral("CLASSIC");
	return record;
}

QImage SkinEntry::normalisedTexture(const QImage& image)
{
	return normaliseTexture(image);
}
