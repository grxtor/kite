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

#include <QPixmap>
#include <QQuickImageProvider>
#include <QSize>
#include <QString>

#include <memory>

#include <icons/IconList.h>

/*
 * Bridges IconList into QML, which cannot display a QIcon:
 *
 *     Image { source: "image://instanceicon/" + iconKey }
 *
 * Pixmap provider: QIcon and QPixmap are GUI-thread-only, and Pixmap is the
 * only type Qt Quick guarantees calls requestPixmap() on the GUI thread.
 *
 * requestedSize is in device pixels; an empty one falls back to a fixed
 * extent. *size is the pixmap actually returned.
 *
 * QML caches a resolved URL by id, so id may carry a "<key>?rev=N" query. It
 * is stripped before lookup and only forces a refetch.
 *
 * An unknown or empty key falls back to IconList::getIcon()'s "grass"
 * builtin; only if that is also null is a plain pixmap drawn, so callers never
 * get a null QPixmap.
 *
 * The IconList is injected as a shared_ptr, so a test can pass a throwaway
 * one.
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
