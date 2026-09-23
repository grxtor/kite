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

#pragma once

#include <QImage>
#include <QPixmap>
#include <QQuickImageProvider>
#include <QSize>
#include <QString>

/*
 * Bridges an account's Minecraft skin -- the face, or (since MeshMC wants the
 * signature "standing on a stage" look for the Accounts hero) the full front-
 * facing body -- into QML, the same way InstanceIconProvider (see
 * InstanceIconProvider.h) bridges IconList. A delegate writes
 *
 *     Image { source: "image://accountface/" + accountId }              // face
 *     Image { source: "image://accountface/body/" + accountId }         // body
 *
 * where accountId is a profile id or an account's internalId() -- whichever
 * QmlShell ends up exposing as the default account's identifier. The
 * "body/" prefix is stripped before lookup, exactly like the "?rev=N" cache-
 * buster below, so both forms find the same account.
 *
 * THREADING. Pixmap type, not Image or an async provider, for exactly the
 * reason InstanceIconProvider.h gives: QPixmap/QImage compositing is
 * GUI-thread-only, and Pixmap is the only one of the three
 * QQuickImageProvider types Qt Quick guarantees calls requestPixmap() on the
 * GUI thread.
 *
 * SIZE. requestedSize arrives already in device pixels -- QML has folded in
 * the device pixel ratio before calling here. An invalid or empty
 * requestedSize falls back to a fixed extent (per kind -- see
 * requestPixmap()). Both the face and the body are scaled with
 * Qt::FastTransformation (nearest-neighbour), not the smooth default: this
 * is pixel art, and smooth-scaling it up would blur the very pixel edges
 * that make a skin recognisable.
 *
 * CACHE-BUSTING. Same "<id>?rev=N" convention as InstanceIconProvider: the
 * query string is stripped before lookup and changes nothing about which
 * account is found. It exists purely so a caller can force QML to refetch a
 * URL it has already resolved, once the account's skin might have changed
 * (AccountList::listChanged/defaultAccountChanged move which account is
 * default; the account's own changed() fires when its data, including the
 * skin, is refreshed). Bumping the revision when either fires is left to
 * whoever wires this provider into the engine.
 *
 * LOOKUP. Looks the account up in LAUNCHER->accounts() (core/LauncherContext.h)
 * by profile id first, falling back to a linear scan by internalId():
 * AccountList exposes findAccountByProfileId() but no internal-id finder,
 * and offline accounts have no profile id at all. No matching account, no
 * skin data, or a LauncherContext that is not up yet (see
 * LauncherContext::instance()) all return a transparent pixmap rather than a
 * null one -- QML's own fallback avatar is what should show, not a
 * broken-image icon. That fallback matters most for the body: offline
 * accounts (and any account before its profile texture is fetched) have no
 * skin bytes at all, and the Accounts hero draws its own neutral silhouette
 * underneath the (then fully transparent) body image for exactly that case.
 */
class AccountFaceProvider : public QQuickImageProvider
{
  public:
	explicit AccountFaceProvider();

	QPixmap requestPixmap(const QString& id, QSize* size,
						  const QSize& requestedSize) override;

	/* Composites the face (the 8x8 region at (8,8)) with the hat overlay
	 * (the 8x8 region at (40,8)) of a full skin texture, at native 8x8
	 * resolution -- the same two regions MinecraftAccount::getFace() and
	 * SkinUtils::getFaceFromCache() already draw. Kept as a pure function
	 * of the skin image, separate from any account lookup, so it can be
	 * unit tested without a QGuiApplication, a real account or a network --
	 * see AccountFaceProvider_test.cpp. Returns a null QImage if skin is too
	 * small to contain both regions.
	 *
	 * Hat pixels with alpha 0 leave the face beneath them untouched: the
	 * face layer is drawn first, filling the whole canvas, and the hat is
	 * composited over it with ordinary SourceOver painting, which is a
	 * no-op wherever the source alpha is 0. */
	static QImage faceFromSkin(const QImage& skin);

	/* Composites a front-facing full body from a skin texture: head (with
	 * hat), torso (with jacket), both arms (with sleeves) and both legs
	 * (with pants), laid out the way the game itself poses a standing
	 * player -- arms at the sides, so the character's right arm/leg render
	 * on the left of the image and the left arm/leg on the right (as they do
	 * when facing the viewer). @p slim narrows both arms from 4px to 3px,
	 * matching the account's skin variant ("SLIM" == Alex-style thin arms).
	 *
	 * Accepts both skin formats: the modern 64x64 texture (with the jacket/
	 * sleeve/pants overlay layer and a distinct left arm/leg) and the legacy
	 * 64x32 one, which has neither -- the left arm and leg are mirrored from
	 * the right for it instead. Returns a null QImage if skin is too small
	 * for even the legacy layout.
	 *
	 * A pure function of the skin image, same rationale as faceFromSkin():
	 * unit-testable without any account, engine or network. */
	static QImage bodyFromSkin(const QImage& skin, bool slim);
};
