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

#include <QPixmap>
#include <QQuickImageProvider>
#include <QSize>
#include <QString>

#include <memory>

#include <icons/IconList.h>

/*
 * Bridges IconList (QIcon, launcher/icons/IconList.h) into QML, which has no
 * way to display a QIcon. A delegate writes
 *
 *     Image { source: "image://instanceicon/" + iconKey }
 *
 * where iconKey is the string role InstanceList::roleNames() exposes for
 * Qt::DecorationRole (see InstanceList.h/.cpp) -- the same key
 * InstanceProxyModel::data() already resolves through IconList::getIcon()
 * for the QtWidgets grid.
 *
 * THREADING. This derives from QQuickImageProvider with ImageType::Pixmap,
 * not QQuickAsyncImageProvider and not ImageType::Image. Qt Quick only
 * guarantees a Pixmap-type provider's requestPixmap() is called on the GUI
 * thread; ImageType::Image can be serviced from a loader thread once the
 * engine's threaded image loading kicks in, and QQuickAsyncImageProvider is
 * explicitly meant to do its work off-thread. QIcon and QPixmap are GUI-only
 * types -- constructing, copying or painting either off the GUI thread is
 * undefined behaviour -- so Pixmap is the only one of the three that keeps
 * this code where it is safe to touch them.
 *
 * SIZE. requestedSize arrives already in device pixels; QML has already
 * folded in the device pixel ratio before calling requestPixmap(), so it is
 * used as-is here, with no extra multiplication. An invalid or empty
 * requestedSize (the default QSize(), or an explicit zero) falls back to a
 * fixed default extent instead. *size is always set to the pixmap actually
 * returned, which is not necessarily requestedSize.
 *
 * CACHE-BUSTING. QML caches a resolved image://<provider>/<id> URL by id, so
 * once IconList::iconUpdated(key) fires there is otherwise no way to make an
 * already-bound Image re-fetch the same key. id may carry a "<key>?rev=N"
 * query string; requestPixmap() strips everything from the first '?' onward
 * before looking the key up, so N has no effect on which icon comes back --
 * it exists purely so a caller can change it to make QML treat the URL as
 * new. Bumping N when iconUpdated fires is left to whoever wires this
 * provider into the engine.
 *
 * UNKNOWN KEYS. An unknown or empty key is not special-cased here:
 * IconList::getIcon() already falls back to the "grass" builtin for those
 * (and for the literal key "default"), which is the same fallback
 * InstanceProxyModel::data() relies on today. Only if that also somehow
 * comes back null (no IconList given, or "grass" itself unavailable) does
 * this provider draw a plain fallback pixmap of its own, so a caller never
 * receives a null QPixmap.
 *
 * OWNERSHIP. Takes the IconList as a std::shared_ptr rather than a raw
 * pointer because that is how the rest of the launcher already holds it --
 * Application::icons() returns std::shared_ptr<IconList>, not a
 * QObject-parented instance -- so this provider shares the same object
 * instead of assuming a global. It reaches for neither APPLICATION nor
 * LAUNCHER: the list is injected by whoever constructs the provider, which
 * is what makes it constructible with a throwaway IconList in a test.
 */
class InstanceIconProvider : public QQuickImageProvider
{
  public:
	explicit InstanceIconProvider(std::shared_ptr<IconList> icons);

	QPixmap requestPixmap(const QString& id, QSize* size,
						  const QSize& requestedSize) override;

  private:
	std::shared_ptr<IconList> m_icons;
};
