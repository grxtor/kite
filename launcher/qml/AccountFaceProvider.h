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

#pragma once

#include <QImage>
#include <QPixmap>
#include <QQuickImageProvider>
#include <QSize>
#include <QString>

/*
 * Bridges an account's Minecraft skin into QML:
 *
 *     Image { source: "image://accountface/" + accountId }         // face
 *     Image { source: "image://accountface/body/" + accountId }    // body
 *
 * accountId is a profile id or an account's internalId() (offline accounts
 * have no profile id). A "?rev=N" query is stripped before lookup; it only
 * forces QML to refetch a URL it has cached.
 *
 * More prefixes hand out raw skins for the 3D preview and the skin library:
 * "skin/<accountId>" is the account's own skin and "skinfile/<percent-encoded
 * path>" a library PNG, both normalised to 64x64. "skinthumb/<path>" is a
 * library PNG's flat sprite, "skinthumbslim/<path>" the same with slim arms;
 * "skinbody/<path>" and "skinbodyslim/<path>" are its front view, like
 * "body/". These come back unscaled.
 *
 * Pixmap provider: compositing is GUI-thread-only, and Pixmap is the only type
 * Qt Quick guarantees calls requestPixmap() on the GUI thread. requestedSize
 * arrives in device pixels; an empty one falls back to a fixed extent. Face and
 * body scale with Qt::FastTransformation so the pixel edges stay sharp.
 *
 * An account without skin bytes (offline, or before its texture is fetched) is
 * drawn in the default Steve or Alex skin (DefaultSkins). Until that has
 * downloaded, and for no matching account or no LauncherContext yet, the
 * result is a transparent pixmap so QML's own fallback shows instead of a
 * broken-image icon. DefaultSkins::loaded() says when to fetch again.
 */
class AccountFaceProvider : public QQuickImageProvider
{
  public:
	explicit AccountFaceProvider();

	QPixmap requestPixmap(const QString& id, QSize* size,
						  const QSize& requestedSize) override;

	/* The face (8x8 at (8,8)) with the hat overlay (8x8 at (40,8)) at native
	 * resolution. Null if @p skin is too small to contain both. Pure, so it
	 * can be tested without an account or a network. */
	static QImage faceFromSkin(const QImage& skin);

	/* The raw image behind a "skin/", "skinfile/" or "skinthumb[slim]/" id
	 * (query stripped): a 64x64 texture or a library sprite. Null for any
	 * other id or an unusable skin. "skin/" needs LauncherContext; the file
	 * kinds do not. */
	static QImage skinTexture(const QString& id);

	/* A front-facing full body: head, torso, arms and legs with their overlay
	 * layers, posed like the game's standing player (the character's right
	 * arm/leg on the image's left). @p slim narrows the arms from 4px to 3px.
	 * Takes the modern 64x64 texture or the legacy 64x32 one, whose left
	 * arm/leg are mirrored from the right. Null if @p skin is too small even
	 * for the legacy layout. Pure, like faceFromSkin(). */
	static QImage bodyFromSkin(const QImage& skin, bool slim);
};
