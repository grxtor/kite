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

#include "InstanceIconProvider.h"

#include <utility>

namespace
{
// Fallback for an unrecognised or zero requestedSize.
constexpr int kDefaultIconExtent = 64;
}

InstanceIconProvider::InstanceIconProvider(std::shared_ptr<IconList> icons)
	: QQuickImageProvider(QQuickImageProvider::Pixmap), m_icons(std::move(icons))
{
}

QPixmap InstanceIconProvider::requestPixmap(const QString& id, QSize* size,
											const QSize& requestedSize)
{
	// The cache-busting "?rev=N" plays no part in the lookup.
	const int queryStart = id.indexOf(QLatin1Char('?'));
	const QString key = queryStart < 0 ? id : id.left(queryStart);

	// requestedSize is already in device pixels.
	const QSize wanted = requestedSize.isEmpty()
							 ? QSize(kDefaultIconExtent, kDefaultIconExtent)
							 : requestedSize;

	QPixmap pixmap;
	if (m_icons) {
		// getIcon() falls back to "grass" for an unknown key. The explicit
		// ratio of 1 matters: QIcon::pixmap(QSize) alone multiplies by the
		// device pixel ratio again, handing QML a pixmap twice the item's
		// device size on a 2x screen.
		pixmap = m_icons->getIcon(key).pixmap(wanted, 1.0);
	}

	if (pixmap.isNull()) {
		// No IconList, or even "grass" is missing; a null QPixmap would reach
		// QML as a broken image.
		pixmap = QPixmap(wanted);
		pixmap.fill(Qt::gray);
	}

	*size = pixmap.size();
	return pixmap;
}
