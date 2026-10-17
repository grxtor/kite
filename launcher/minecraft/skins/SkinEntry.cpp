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

#include "SkinEntry.h"

#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QVarLengthArray>

/* ---------------------------------------------------------------------------
 * Every coordinate in this file is transcribed from the Minecraft skin
 * format, not computed from a rule about it.
 *
 * An earlier version of this file expressed the layout as a formula -- "a box
 * unwrap is a cross, so face N of a w*h*d box sits at offset X" -- and then
 * generated the legacy limb copies and the thumbnail source rectangles from
 * it. That is the wrong shape for this problem. The formula was an
 * interpretation of the format, so any mistake in it was invisible: there was
 * nothing to hold the output up against, and a single wrong term silently
 * moved every limb at once.
 *
 * A format is data. It is written out as data, so that it can be read against
 * the format and checked a line at a time.
 * ------------------------------------------------------------------------ */

namespace
{
	/* Force a rectangle fully opaque, in place.
	 *
	 * Skins are authored with the base layer opaque, but plenty of editors
	 * leave stray alpha there. Left alone it renders as a hole straight
	 * through the model, so the base layer is made opaque unconditionally.
	 *
	 * NOTE: the whole rectangle is covered, its last row and column
	 * included. Qt's QRect::right()/bottom() are inclusive, so the obvious
	 * `x < rect.right()` loop quietly skips those. */
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

	/* The three rectangles of a 64x64 skin that belong to the base layer.
	 * Everything else is either second layer -- which is *supposed* to have
	 * transparency -- or unused padding. */
	const QRect kBaseLayerRegions[] = {
		QRect(0, 0, 32, 16),   /* head */
		QRect(0, 16, 64, 16),  /* torso, right arm, right leg */
		QRect(16, 48, 32, 16), /* left arm, left leg */
	};

	/* The quadrant that a legacy 64x32 skin does not use, and that becomes
	 * the head and arm second layer once the texture is widened. */
	const QRect kLegacyUnusedQuadrant(32, 0, 32, 32);

	/* Legacy 64x32 -> 64x64 limb copies.
	 *
	 * Legacy skins carry only the right arm and the right leg; the game drew
	 * the left ones by mirroring at render time. Widening the texture has to
	 * bake that mirror in.
	 *
	 * Each row: take the (x, y, w, h) rectangle, mirror it horizontally, and
	 * draw it at (x + offsetX, y + offsetY). Twelve rows, because each limb
	 * is six faces and the two side faces swap places when mirrored. */
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

	/* Legacy skins predate the second layer, and the top-right quadrant of
	 * the texture was simply unused -- but it was *not* required to be
	 * transparent, and the classic tools left it full of opaque garbage. Once
	 * such a skin is widened that quadrant becomes the head and arm second
	 * layer, and the garbage renders as a solid shell around the player.
	 *
	 * There is no flag to tell the two cases apart, so use the same heuristic
	 * the game does: a texture authored with a second layer will have *some*
	 * transparency in it. If every pixel up there is opaque, the layer was
	 * never meant to exist and is cleared. */
	void clearUnusedLegacyOverlay(QImage& skin)
	{
		for (int y = kLegacyUnusedQuadrant.top();
			 y <= kLegacyUnusedQuadrant.bottom(); ++y) {
			const QRgb* line = reinterpret_cast<const QRgb*>(skin.scanLine(y));
			for (int x = kLegacyUnusedQuadrant.left();
				 x <= kLegacyUnusedQuadrant.right(); ++x) {
				if (qAlpha(line[x]) < 128) {
					/* Deliberately authored second layer -- leave it. */
					return;
				}
			}
		}
		fillAlpha(skin, kLegacyUnusedQuadrant, 0);
	}

	/* Widen a legacy 64x32 texture into the modern 64x64 layout. */
	QImage widenLegacyTexture(const QImage& legacy)
	{
		QImage widened(64, 64, legacy.format());
		widened.fill(Qt::transparent);
		{
			QPainter painter(&widened);
			painter.setCompositionMode(QPainter::CompositionMode_Source);
			painter.drawImage(0, 0, legacy);
		}

		/* Read every copy out before writing any of them back. The source and
		 * destination rectangles do not overlap, but taking a copy() of an
		 * image that has a live QPainter on it is asking for trouble anyway:
		 * the painter holds its own reference to the buffer. */
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

	/* Put a skin image into the one shape the rest of the code expects:
	 * 64x64, ARGB32, base layer opaque.
	 *
	 * Anything that is not a skin is handed back untouched, so that
	 * SkinEntry::isUsable() can reject it on its original dimensions rather
	 * than on something this function invented. */
	QImage normaliseTexture(QImage skin)
	{
		const int width = skin.width();
		const int height = skin.height();
		if (width != 64 || (height != 32 && height != 64)) {
			return skin;
		}

		/* Everything below walks scanlines as QRgb, and QPainter refuses to
		 * paint onto a paletted image at all. Normalising the format up front
		 * is what makes both safe -- including for premultiplied input, where
		 * treating the bytes as plain ARGB would corrupt the colours. */
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

	/* Flat 36x36 sprite: the model seen from the front on the left half and
	 * from the back on the right half, each drawn base layer first and second
	 * layer over it.
	 *
	 * The draw order is part of the result -- arms come after the body so they
	 * sit on top of the torso, which is what makes a slim arm read as slim at
	 * this size -- so it is written out as a sequence of draws rather than
	 * looped over a table that could reorder it. */
	QImage renderThumbnail(const QImage& texture, bool slim)
	{
		QImage sprite(36, 36, QImage::Format_ARGB32);
		sprite.fill(Qt::transparent);
		if (texture.isNull()) {
			return sprite;
		}

		/* A slim arm is 3 pixels wide instead of 4, which both narrows the
		 * source rectangle and shifts the outer arm a pixel inwards so it
		 * still touches the shoulder. */
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
	/* Nothing on disk says which arm width a bare PNG was drawn for, so it
	 * starts out Classic and the thumbnail is generated to match. Importing
	 * from a profile overrides both afterwards. */
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
	/* Indexes written before "wornModel" kept the worn skin's width in step
	 * with the profile on every rescan, so its own width is what was worn. */
	const QString wornModel =
		record.value(QStringLiteral("wornModel")).toString();
	m_wornArms = wornModel.isEmpty()
					 ? m_arms
					 : (wornModel == QLatin1String("SLIM") ? Arms::Slim
														   : Arms::Classic);

	/* The index stores the display name, not a path: the library directory is
	 * relocatable and the ".png" is an implementation detail of how entries
	 * are stored. */
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
	/* 64 is what a normalised texture always ends up as; 32 is accepted so
	 * that a legacy file which somehow skipped widening is still treated as a
	 * skin rather than silently vanishing from the library. */
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
		/* Refusing beats overwriting: the other file is somebody's skin. */
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
