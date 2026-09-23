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

#include "AccountFaceProvider.h"

#include <QPainter>

#include "core/LauncherContext.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"

namespace
{
// Only a fallback for an unrecognised or zero requestedSize; mirrors
// InstanceIconProvider's own default extent.
constexpr int kDefaultFaceExtent = 64;

// Standard Minecraft skin texture layout: the face is the front of the
// head, the hat is its second-layer overlay -- same coordinates
// MinecraftAccount::getFace() and SkinUtils::getFaceFromCache() draw.
constexpr int kFaceX = 8;
constexpr int kFaceY = 8;
constexpr int kHatX = 40;
constexpr int kHatY = 8;
constexpr int kFaceExtent = 8;

// Prefix an id is checked for (after the "?rev=N" cache-buster is already
// stripped) to ask for the full body instead of the face.
const QString kBodyPrefix = QStringLiteral("body/");

// Front-view body layout, all still the standard Minecraft skin UV: torso
// and legs are always this wide, only the arms narrow for the slim variant.
constexpr int kLimbHeight = 12;
constexpr int kTorsoWidth = 8;
constexpr int kLegWidth = 4;
constexpr int kClassicArmWidth = 4;
constexpr int kSlimArmWidth = 3;
constexpr int kHeadSize = 8;
// A body render's default box, at the classic (non-slim) aspect ratio;
// only hit for a caller that asks for a body without a requestedSize.
constexpr int kDefaultBodyWidth = kClassicArmWidth * 2 + kTorsoWidth;
constexpr int kDefaultBodyHeight = kHeadSize + kLimbHeight * 2;

// Every real Minecraft skin texture is this wide, legacy or modern alike.
constexpr int kSkinWidth = 64;
// Skin texture height that carries the modern jacket/sleeves/pants overlay
// layer and a distinct left arm/leg. Anything shorter (down to
// kLegacySkinHeight) is the legacy format, which has neither -- the left
// side is mirrored from the right instead.
constexpr int kModernSkinHeight = 64;
constexpr int kLegacySkinHeight = 32;

QPixmap transparentPixmap(const QSize& size)
{
	QImage image(size, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::transparent);
	return QPixmap::fromImage(image);
}

// AccountList exposes a profile-id finder but not an internal-id one, and
// offline accounts have no profile id at all -- so a key that misses as a
// profile id is tried again as an internalId(), by linear scan over the
// same public count()/at() API AccountList::findAccountByProfileId() itself
// uses.
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
} // namespace

AccountFaceProvider::AccountFaceProvider()
	: QQuickImageProvider(QQuickImageProvider::Pixmap)
{
}

QImage AccountFaceProvider::faceFromSkin(const QImage& skin)
{
	if (skin.isNull() || skin.width() < kHatX + kFaceExtent ||
		skin.height() < kFaceY + kFaceExtent) {
		return QImage();
	}

	QImage face(kFaceExtent, kFaceExtent, QImage::Format_ARGB32_Premultiplied);
	face.fill(Qt::transparent);

	QPainter painter(&face);
	painter.drawImage(
		0, 0, skin.copy(kFaceX, kFaceY, kFaceExtent, kFaceExtent));
	painter.drawImage(0, 0, skin.copy(kHatX, kHatY, kFaceExtent, kFaceExtent));
	painter.end();
	return face;
}

QImage AccountFaceProvider::bodyFromSkin(const QImage& skin, bool slim)
{
	if (skin.isNull() || skin.width() < kSkinWidth ||
		skin.height() < kLegacySkinHeight) {
		return QImage();
	}

	// The legacy 64x32 format has no overlay layer (jacket/sleeves/pants)
	// and no distinct left arm/left leg region -- both are synthesised
	// below by mirroring the right side.
	const bool legacy = skin.height() < kModernSkinHeight;

	const int armWidth = slim ? kSlimArmWidth : kClassicArmWidth;
	const int bodyWidth = armWidth * 2 + kTorsoWidth;
	const int bodyHeight = kHeadSize + kLimbHeight * 2;
	const int limbY = kHeadSize;
	const int legY = limbY + kLimbHeight;
	// The torso sits between the two arms; both leg columns sit directly
	// under it, together spanning the same width.
	const int torsoX = armWidth;
	const int rightLegX = armWidth;
	const int leftLegX = rightLegX + kLegWidth;
	const int leftArmX = armWidth + kTorsoWidth;

	QImage body(bodyWidth, bodyHeight, QImage::Format_ARGB32_Premultiplied);
	body.fill(Qt::transparent);

	QPainter painter(&body);

	// Head, centred above the torso, hat overlay included -- present in
	// both skin formats.
	painter.drawImage(torsoX, 0, skin.copy(kFaceX, kFaceY, kHeadSize, kHeadSize));
	painter.drawImage(torsoX, 0, skin.copy(kHatX, kHatY, kHeadSize, kHeadSize));

	// Torso, with its jacket overlay (modern format only).
	painter.drawImage(torsoX, limbY, skin.copy(20, 20, kTorsoWidth, kLimbHeight));
	if (!legacy) {
		painter.drawImage(
			torsoX, limbY, skin.copy(20, 36, kTorsoWidth, kLimbHeight));
	}

	// The character faces the viewer, so -- as in a mirror -- its right
	// arm/leg render on the left of the image and its left arm/leg on the
	// right. Only armWidth columns of each region are sampled: for the slim
	// variant that is 3 of the texture's 4, leaving the same blank 4th
	// column the game itself never draws.
	const QImage rightArm = skin.copy(44, 20, armWidth, kLimbHeight);
	painter.drawImage(0, limbY, rightArm);
	if (!legacy) {
		painter.drawImage(0, limbY, skin.copy(44, 36, armWidth, kLimbHeight));
	}
	if (legacy) {
		// No separate left arm texture to draw -- the right one already
		// includes its (only) layer, so just mirror the composited result.
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

QPixmap AccountFaceProvider::requestPixmap(const QString& id, QSize* size,
										   const QSize& requestedSize)
{
	// Cache-busting query ("<key>?rev=N") plays no part in the lookup -- see
	// the header for why it exists at all.
	const int queryStart = id.indexOf(QLatin1Char('?'));
	const QString withoutQuery = queryStart < 0 ? id : id.left(queryStart);

	const bool wantsBody = withoutQuery.startsWith(kBodyPrefix);
	const QString key =
		wantsBody ? withoutQuery.mid(kBodyPrefix.length()) : withoutQuery;

	// requestedSize is already in device pixels -- QML applied the device
	// pixel ratio before calling here -- so it is used as-is.
	const QSize defaultSize = wantsBody
								  ? QSize(kDefaultBodyWidth, kDefaultBodyHeight)
								  : QSize(kDefaultFaceExtent, kDefaultFaceExtent);
	const QSize wanted = requestedSize.isEmpty() ? defaultSize : requestedSize;

	QPixmap pixmap;
	if (auto* context = LauncherContext::instance()) {
		MinecraftAccountPtr account =
			findAccount(context->accounts().get(), key);
		if (account) {
			QImage skin;
			if (skin.loadFromData(
					account->accountData()->minecraftProfile.skin.data,
					"PNG")) {
				const bool slim = account->accountData()
									   ->minecraftProfile.skin.variant.compare(
										   QLatin1String("SLIM"),
										   Qt::CaseInsensitive) == 0;
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
		// No account, no skin, or the context is not up yet -- QML's own
		// fallback avatar is what should show, not a broken-image icon.
		pixmap = transparentPixmap(wanted);
	}

	*size = pixmap.size();
	return pixmap;
}
