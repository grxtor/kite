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

#include "GameTextureRecipes.h"

#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QSize>

namespace
{
struct Layer {
	/* Where the texture sits in the jar's texture root, newest layout
	 * first: 1.13 renamed most block and item textures, and some mobs have
	 * moved since. */
	QStringList paths;
	/* The part of it to take; a null rect is the whole texture. */
	QRect source;
	/* Where that part goes on the icon. */
	QPoint at;
};

struct Recipe {
	QString key;
	/* The icon's size before it is padded square; empty is the size of
	 * the first layer. */
	QSize size;
	/* The first layer is the icon itself and must be in the jar; the rest
	 * go over it where the jar has them. */
	QList<Layer> layers;
};

Recipe wholeTexture(const QString& key, const QStringList& paths)
{
	return {key, QSize(), {{paths, QRect(), QPoint()}}};
}

/* An 8x8 mob head in the standard entity layout: the front of the head
 * box at (8,8), and the front of its second, outer layer at (40,8) --
 * the same coordinates a player skin uses. */
const QSize kHeadSize(8, 8);
const QRect kHeadFront(8, 8, 8, 8);
const QRect kHatFront(40, 8, 8, 8);

const QList<Recipe>& recipes()
{
	static const QList<Recipe> table = [] {
		const QStringList creeper = {
			QStringLiteral("entity/creeper/creeper.png")};
		const QStringList skeleton = {
			QStringLiteral("entity/skeleton/skeleton.png")};
		const QStringList enderman = {
			QStringLiteral("entity/enderman/enderman.png")};
		const QStringList steve = {
			QStringLiteral("entity/player/wide/steve.png"),
			QStringLiteral("entity/steve.png")};
		const QStringList chicken = {
			QStringLiteral("entity/chicken/chicken_temperate.png"),
			QStringLiteral("entity/chicken/temperate_chicken.png"),
			QStringLiteral("entity/chicken.png")};
		const QStringList fox = {QStringLiteral("entity/fox/fox.png")};

		return QList<Recipe>{
			wholeTexture(QStringLiteral("grass"),
						 {QStringLiteral("block/grass_block_side.png"),
						  QStringLiteral("blocks/grass_side.png")}),
			wholeTexture(QStringLiteral("dirt"),
						 {QStringLiteral("block/dirt.png"),
						  QStringLiteral("blocks/dirt.png")}),
			wholeTexture(QStringLiteral("stone"),
						 {QStringLiteral("block/stone.png"),
						  QStringLiteral("blocks/stone.png")}),
			wholeTexture(QStringLiteral("planks"),
						 {QStringLiteral("block/oak_planks.png"),
						  QStringLiteral("blocks/planks_oak.png")}),
			wholeTexture(QStringLiteral("brick"),
						 {QStringLiteral("block/bricks.png"),
						  QStringLiteral("blocks/brick.png")}),
			wholeTexture(QStringLiteral("tnt"),
						 {QStringLiteral("block/tnt_side.png"),
						  QStringLiteral("blocks/tnt_side.png")}),
			wholeTexture(QStringLiteral("diamond"),
						 {QStringLiteral("block/diamond_block.png"),
						  QStringLiteral("blocks/diamond_block.png")}),
			wholeTexture(QStringLiteral("gold"),
						 {QStringLiteral("block/gold_block.png"),
						  QStringLiteral("blocks/gold_block.png")}),
			wholeTexture(QStringLiteral("iron"),
						 {QStringLiteral("block/iron_block.png"),
						  QStringLiteral("blocks/iron_block.png")}),
			wholeTexture(QStringLiteral("netherstar"),
						 {QStringLiteral("item/nether_star.png"),
						  QStringLiteral("items/nether_star.png")}),
			wholeTexture(QStringLiteral("enderpearl"),
						 {QStringLiteral("item/ender_pearl.png"),
						  QStringLiteral("items/ender_pearl.png")}),
			wholeTexture(QStringLiteral("meat"),
						 {QStringLiteral("item/cooked_beef.png"),
						  QStringLiteral("items/beef_cooked.png")}),
			// An animated strip; wholeTexture() takes its first frame.
			wholeTexture(QStringLiteral("flame"),
						 {QStringLiteral("block/fire_0.png"),
						  QStringLiteral("blocks/fire_layer_0.png")}),
			{QStringLiteral("creeper"), kHeadSize, {{creeper, kHeadFront, {}}}},
			{QStringLiteral("squarecreeper"),
			 kHeadSize,
			 {{creeper, kHeadFront, {}}}},
			{QStringLiteral("skeleton"),
			 kHeadSize,
			 {{skeleton, kHeadFront, {}}, {skeleton, kHatFront, {}}}},
			// The outer layer at (8,24) is its jaw; the eyes are a texture of
			// their own, drawn over the face.
			{QStringLiteral("enderman"),
			 kHeadSize,
			 {{enderman, kHeadFront, {}},
			  {enderman, QRect(8, 24, 8, 8), {}},
			  {{QStringLiteral("entity/enderman/enderman_eyes.png")},
			   kHeadFront,
			   {}}}},
			{QStringLiteral("steve"),
			 kHeadSize,
			 {{steve, kHeadFront, {}}, {steve, kHatFront, {}}}},
			// A 4x6 face with the beak over its middle and the wattle under it.
			{QStringLiteral("chicken"),
			 QSize(4, 6),
			 {{chicken, QRect(3, 3, 4, 6), {}},
			  {chicken, QRect(16, 2, 4, 2), QPoint(0, 2)},
			  {chicken, QRect(16, 6, 2, 2), QPoint(1, 4)}}},
			// An 8x6 face under two ears, with the snout over its bottom rows.
			{QStringLiteral("fox"),
			 QSize(8, 8),
			 {{fox, QRect(7, 11, 8, 6), QPoint(0, 2)},
			  {fox, QRect(9, 2, 2, 2), QPoint(0, 0)},
			  {fox, QRect(16, 2, 2, 2), QPoint(6, 0)},
			  {fox, QRect(9, 21, 4, 2), QPoint(2, 6)}}},
			// The front of its body, which is its face.
			{QStringLiteral("bee"),
			 QSize(7, 7),
			 {{{QStringLiteral("entity/bee/bee.png")},
			   QRect(10, 10, 7, 7),
			   {}}}},
		};
	}();
	return table;
}

/* The part of a texture @p layer asks for, or a null image when
 * @p textures has none of its paths or the texture is too small for it. */
QImage layerImage(const Layer& layer, const QHash<QString, QImage>& textures)
{
	for (const QString& path : layer.paths) {
		const auto texture = textures.constFind(path);
		if (texture == textures.constEnd()) {
			continue;
		}
		QRect source = layer.source;
		if (source.isNull()) {
			// An animated texture is a strip of square frames, one under
			// the other; the first one stands for it.
			source = QRect(0, 0, texture->width(),
						   qMin(texture->width(), texture->height()));
		}
		if (!texture->rect().contains(source)) {
			return QImage();
		}
		return texture->copy(source);
	}
	return QImage();
}

QImage paddedSquare(const QImage& image)
{
	if (image.width() == image.height()) {
		return image;
	}
	const int side = qMax(image.width(), image.height());
	QImage square(side, side, QImage::Format_ARGB32);
	square.fill(Qt::transparent);
	QPainter painter(&square);
	painter.drawImage((side - image.width()) / 2, (side - image.height()) / 2,
					  image);
	return square;
}

QImage composeOne(const Recipe& recipe,
				  const QHash<QString, QImage>& textures)
{
	const QImage base = layerImage(recipe.layers.first(), textures);
	if (base.isNull()) {
		return QImage();
	}
	QImage icon(recipe.size.isEmpty() ? base.size() : recipe.size,
				QImage::Format_ARGB32);
	icon.fill(Qt::transparent);
	QPainter painter(&icon);
	painter.drawImage(recipe.layers.first().at, base);
	for (int i = 1; i < recipe.layers.size(); ++i) {
		const QImage part = layerImage(recipe.layers[i], textures);
		if (!part.isNull()) {
			painter.drawImage(recipe.layers[i].at, part);
		}
	}
	painter.end();
	return paddedSquare(icon);
}
} // namespace

QStringList GameTextureRecipes::keys()
{
	QStringList keys;
	for (const Recipe& recipe : recipes()) {
		keys.append(recipe.key);
	}
	return keys;
}

QSet<QString> GameTextureRecipes::texturePaths()
{
	QSet<QString> paths;
	for (const Recipe& recipe : recipes()) {
		for (const Layer& layer : recipe.layers) {
			for (const QString& path : layer.paths) {
				paths.insert(path);
			}
		}
	}
	return paths;
}

QHash<QString, QImage>
GameTextureRecipes::compose(const QHash<QString, QImage>& textures)
{
	QHash<QString, QImage> images;
	for (const Recipe& recipe : recipes()) {
		const QImage image = composeOne(recipe, textures);
		if (!image.isNull()) {
			images.insert(recipe.key, image);
		}
	}
	return images;
}
