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

#include "InstanceIconProvider.h"

#include <utility>

namespace
{
// Only a fallback for an unrecognised or zero requestedSize; the exact value
// is not load-bearing anywhere else.
constexpr int kDefaultIconExtent = 64;
}

InstanceIconProvider::InstanceIconProvider(std::shared_ptr<IconList> icons)
	: QQuickImageProvider(QQuickImageProvider::Pixmap), m_icons(std::move(icons))
{
}

QPixmap InstanceIconProvider::requestPixmap(const QString& id, QSize* size,
											const QSize& requestedSize)
{
	// Cache-busting query ("<key>?rev=N") plays no part in the lookup -- see
	// the header for why it exists at all.
	const int queryStart = id.indexOf(QLatin1Char('?'));
	const QString key = queryStart < 0 ? id : id.left(queryStart);

	// requestedSize is already in device pixels -- QML applied the device
	// pixel ratio before calling here -- so it is used as-is, never an
	// invalid or empty one.
	const QSize wanted = requestedSize.isEmpty()
							 ? QSize(kDefaultIconExtent, kDefaultIconExtent)
							 : requestedSize;

	QPixmap pixmap;
	if (m_icons) {
		// getIcon() already falls back to the "grass" builtin for an unknown
		// key (and for "default"); nothing extra to do for that here.
		pixmap = m_icons->getIcon(key).pixmap(wanted);
	}

	if (pixmap.isNull()) {
		// Reached only if no IconList was given, or even "grass" could not
		// be found (e.g. the icon theme resource was never registered). A
		// null QPixmap here would reach QML as a broken image, so this
		// draws something instead of ever returning one.
		pixmap = QPixmap(wanted);
		pixmap.fill(Qt::gray);
	}

	*size = pixmap.size();
	return pixmap;
}
