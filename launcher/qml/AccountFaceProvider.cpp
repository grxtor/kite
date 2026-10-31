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

#include "AccountFaceProvider.h"

#include <QPainter>
#include <QUrl>

#include "core/LauncherContext.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/skins/DefaultSkins.h"
#include "minecraft/skins/SkinEntry.h"
#include "minecraft/skins/SkinFace.h"

namespace
{
// Fallback for an unrecognised or zero requestedSize.
constexpr int kDefaultFaceExtent = 64;

// Standard skin layout: the face is the front of the head, the hat its
// second-layer overlay.
constexpr int kFaceX = 8;
constexpr int kFaceY = 8;
constexpr int kHatX = 40;
constexpr int kHatY = 8;

const QString kBodyPrefix = QStringLiteral("body/");

// Raw textures normalised to 64x64: an account's own skin by account key, or
// a library PNG by its percent-encoded path; "skinthumb" is a library PNG's
// flat sprite. Never scaled.
const QString kSkinTexturePrefix = QStringLiteral("skin/");
const QString kSkinFilePrefix = QStringLiteral("skinfile/");
const QString kSkinThumbPrefix = QStringLiteral("skinthumb/");
// A bare PNG does not say its arm width, so the id carries it.
const QString kSkinThumbSlimPrefix = QStringLiteral("skinthumbslim/");
// A library PNG's front view, like "body/" for an account.
const QString kSkinBodyPrefix = QStringLiteral("skinbody/");
const QString kSkinBodySlimPrefix = QStringLiteral("skinbodyslim/");

// Front-view layout in standard skin UV: only the arms narrow for slim.
constexpr int kLimbHeight = 12;
constexpr int kTorsoWidth = 8;
constexpr int kLegWidth = 4;
constexpr int kClassicArmWidth = 4;
constexpr int kSlimArmWidth = 3;
constexpr int kHeadSize = 8;
// Default box for a body request without a requestedSize.
constexpr int kDefaultBodyWidth = kClassicArmWidth * 2 + kTorsoWidth;
constexpr int kDefaultBodyHeight = kHeadSize + kLimbHeight * 2;

// Every real skin texture is this wide.
constexpr int kSkinWidth = 64;
// The modern format has jacket/sleeves/pants overlays and a distinct left
// arm/leg; anything shorter is legacy, whose left side mirrors the right.
constexpr int kModernSkinHeight = 64;
constexpr int kLegacySkinHeight = 32;

QPixmap transparentPixmap(const QSize& size)
{
	QImage image(size, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);
	return QPixmap::fromImage(image);
}

// AccountList has no internal-id finder and offline accounts have no profile
// id, so a key that misses as a profile id is scanned as an internalId().
MinecraftAccountPtr findAccount(AccountList* accounts, const QString& key)
{
	if (!accounts || key.isEmpty()) {
		return MinecraftAccountPtr();
	}

	const int byProfileId = accounts->findAccountByProfileId(key);
	if (byProfileId != -1) {
		return accounts->at(byProfileId);
	}

	for (int i = 0; i < accounts->count(); ++i) {
		MinecraftAccountPtr account = accounts->at(i);
		if (account && account->internalId() == key) {
			return account;
		}
	}
	return MinecraftAccountPtr();
}

// The account's own skin, else the default Steve or Alex. Null only while
// that is downloading. @p slim gets the arm width either way.
QImage skinFor(const MinecraftAccountPtr& account, bool* slim)
{
	*slim = DefaultSkins::isSlim(account);
	QImage skin;
	if (skin.loadFromData(account->accountData()->minecraftProfile.skin.data,
						  "PNG")) {
		return skin;
	}
	DefaultSkins* defaults = DefaultSkins::instance();
	defaults->ensureLoaded();
	return defaults->texture(*slim);
}
} // namespace

AccountFaceProvider::AccountFaceProvider()
	: QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QImage AccountFaceProvider::faceFromSkin(const QImage& skin)
{
	return SkinFace::fromSkin(skin);
}

QImage AccountFaceProvider::bodyFromSkin(const QImage& skin, bool slim)
{
	if (skin.isNull() || skin.width() < kSkinWidth ||
		skin.height() < kLegacySkinHeight) {
		return QImage();
	}

	// The legacy 64x32 format has no overlay layer and no left arm/leg; those
	// are mirrored from the right.
	const bool legacy = skin.height() < kModernSkinHeight;

	const int armWidth = slim ? kSlimArmWidth : kClassicArmWidth;
	const int bodyWidth = armWidth * 2 + kTorsoWidth;
	const int bodyHeight = kHeadSize + kLimbHeight * 2;
	const int limbY = kHeadSize;
	const int legY = limbY + kLimbHeight;
	const int torsoX = armWidth;
	const int rightLegX = armWidth;
	const int leftLegX = rightLegX + kLegWidth;
	const int leftArmX = armWidth + kTorsoWidth;

	QImage body(bodyWidth, bodyHeight, QImage::Format_ARGB32_Premultiplied);
	body.fill(Qt::transparent);

	QPainter painter(&body);

	painter.drawImage(torsoX, 0, skin.copy(kFaceX, kFaceY, kHeadSize, kHeadSize));
	painter.drawImage(torsoX, 0, skin.copy(kHatX, kHatY, kHeadSize, kHeadSize));

	painter.drawImage(torsoX, limbY, skin.copy(20, 20, kTorsoWidth, kLimbHeight));
	if (!legacy) {
		painter.drawImage(
			torsoX, limbY, skin.copy(20, 36, kTorsoWidth, kLimbHeight));
	}

	// The character faces the viewer, so its right arm/leg render on the
	// left. Only armWidth columns are sampled; a slim arm leaves the 4th
	// column blank, as the game does.
	const QImage rightArm = skin.copy(44, 20, armWidth, kLimbHeight);
	painter.drawImage(0, limbY, rightArm);
	if (!legacy) {
		painter.drawImage(0, limbY, skin.copy(44, 36, armWidth, kLimbHeight));
	}
	if (legacy) {
		painter.drawImage(leftArmX, limbY, rightArm.mirrored(true, false));
	} else {
		painter.drawImage(
			leftArmX, limbY, skin.copy(36, 52, armWidth, kLimbHeight));
		painter.drawImage(
			leftArmX, limbY, skin.copy(52, 52, armWidth, kLimbHeight));
	}

	const QImage rightLeg = skin.copy(4, 20, kLegWidth, kLimbHeight);
	painter.drawImage(rightLegX, legY, rightLeg);
	if (!legacy) {
		painter.drawImage(
			rightLegX, legY, skin.copy(4, 36, kLegWidth, kLimbHeight));
	}
	if (legacy) {
		painter.drawImage(leftLegX, legY, rightLeg.mirrored(true, false));
	} else {
		painter.drawImage(
			leftLegX, legY, skin.copy(20, 52, kLegWidth, kLimbHeight));
		painter.drawImage(
			leftLegX, legY, skin.copy(4, 52, kLegWidth, kLimbHeight));
	}

	painter.end();
	return body;
}

QImage AccountFaceProvider::skinTexture(const QString& id)
{
	const bool slimThumb = id.startsWith(kSkinThumbSlimPrefix);
	const bool thumb = slimThumb || id.startsWith(kSkinThumbPrefix);
	if (thumb || id.startsWith(kSkinFilePrefix)) {
		const int prefixLength = slimThumb ? kSkinThumbSlimPrefix.length()
								 : thumb   ? kSkinThumbPrefix.length()
										   : kSkinFilePrefix.length();
		const QString path =
			QUrl::fromPercentEncoding(id.mid(prefixLength).toUtf8());
		SkinEntry entry(path);
		if (!entry.isUsable()) {
			return QImage();
		}
		if (slimThumb) {
			entry.setArms(SkinEntry::Arms::Slim);
		}
		return thumb ? entry.thumbnail() : entry.texture();
	}
	if (id.startsWith(kSkinTexturePrefix)) {
		auto* context = LauncherContext::instance();
		const MinecraftAccountPtr account =
			context ? findAccount(context->accounts().get(),
								  id.mid(kSkinTexturePrefix.length()))
					: MinecraftAccountPtr();
		if (!account) {
			return QImage();
		}
		bool slim = false;
		const QImage skin = skinFor(account, &slim);
		if (skin.isNull()) {
			return QImage();
		}
		const QImage texture = SkinEntry::normalisedTexture(skin);
		return texture.width() == 64 && texture.height() == 64 ? texture
															   : QImage();
	}
	return QImage();
}

QPixmap AccountFaceProvider::requestPixmap(const QString& id, QSize* size,
										   const QSize& requestedSize)
{
	// The cache-busting "?rev=N" plays no part in the lookup.
	const int queryStart = id.indexOf(QLatin1Char('?'));
	const QString withoutQuery = queryStart < 0 ? id : id.left(queryStart);

	const bool slimBody = withoutQuery.startsWith(kSkinBodySlimPrefix);
	if (slimBody || withoutQuery.startsWith(kSkinBodyPrefix)) {
		const QString path = QUrl::fromPercentEncoding(
			withoutQuery
				.mid(slimBody ? kSkinBodySlimPrefix.length()
							  : kSkinBodyPrefix.length())
				.toUtf8());
		const SkinEntry entry(path);
		const QSize bodyWanted =
			requestedSize.isEmpty()
				? QSize(kDefaultBodyWidth, kDefaultBodyHeight)
				: requestedSize;
		QPixmap pixmap;
		if (entry.isUsable()) {
			const QImage body = bodyFromSkin(entry.texture(), slimBody);
			if (!body.isNull()) {
				pixmap = QPixmap::fromImage(body).scaled(
					bodyWanted, Qt::KeepAspectRatio, Qt::FastTransformation);
			}
		}
		if (pixmap.isNull()) {
			pixmap = transparentPixmap(bodyWanted);
		}
		*size = pixmap.size();
		return pixmap;
	}

	if (withoutQuery.startsWith(kSkinTexturePrefix) ||
		withoutQuery.startsWith(kSkinFilePrefix) ||
		withoutQuery.startsWith(kSkinThumbPrefix) ||
		withoutQuery.startsWith(kSkinThumbSlimPrefix)) {
		const QImage image = skinTexture(withoutQuery);
		const QPixmap pixmap = image.isNull()
								   ? transparentPixmap(QSize(kDefaultFaceExtent,
															 kDefaultFaceExtent))
								   : QPixmap::fromImage(image);
		*size = pixmap.size();
		return pixmap;
	}

	const bool wantsBody = withoutQuery.startsWith(kBodyPrefix);
	const QString key =
		wantsBody ? withoutQuery.mid(kBodyPrefix.length()) : withoutQuery;

	// requestedSize is already in device pixels.
	const QSize defaultSize = wantsBody
								  ? QSize(kDefaultBodyWidth, kDefaultBodyHeight)
								  : QSize(kDefaultFaceExtent, kDefaultFaceExtent);
	const QSize wanted = requestedSize.isEmpty() ? defaultSize : requestedSize;

	QPixmap pixmap;
	if (auto* context = LauncherContext::instance()) {
		MinecraftAccountPtr account =
			findAccount(context->accounts().get(), key);
		if (account) {
			bool slim = false;
			const QImage skin = skinFor(account, &slim);
			if (!skin.isNull()) {
				const QImage composited =
					wantsBody ? bodyFromSkin(skin, slim) : faceFromSkin(skin);
				if (!composited.isNull()) {
					pixmap = QPixmap::fromImage(composited).scaled(
						wanted, Qt::KeepAspectRatio, Qt::FastTransformation);
				}
			}
		}
	}

	if (pixmap.isNull()) {
		// No account, no skin or no context yet: transparent, so QML's own
		// fallback shows.
		pixmap = transparentPixmap(wanted);
	}

	*size = pixmap.size();
	return pixmap;
}
