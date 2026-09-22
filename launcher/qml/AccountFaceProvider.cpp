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

QPixmap AccountFaceProvider::requestPixmap(const QString& id, QSize* size,
										   const QSize& requestedSize)
{
	// Cache-busting query ("<key>?rev=N") plays no part in the lookup -- see
	// the header for why it exists at all.
	const int queryStart = id.indexOf(QLatin1Char('?'));
	const QString key = queryStart < 0 ? id : id.left(queryStart);

	// requestedSize is already in device pixels -- QML applied the device
	// pixel ratio before calling here -- so it is used as-is.
	const QSize wanted = requestedSize.isEmpty()
							 ? QSize(kDefaultFaceExtent, kDefaultFaceExtent)
							 : requestedSize;

	QPixmap pixmap;
	if (auto* context = LauncherContext::instance()) {
		MinecraftAccountPtr account =
			findAccount(context->accounts().get(), key);
		if (account) {
			QImage skin;
			if (skin.loadFromData(
					account->accountData()->minecraftProfile.skin.data,
					"PNG")) {
				const QImage face = faceFromSkin(skin);
				if (!face.isNull()) {
					pixmap = QPixmap::fromImage(face).scaled(
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
