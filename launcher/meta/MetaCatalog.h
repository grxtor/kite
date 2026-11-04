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

#include <QByteArray>
#include <QJsonValue>
#include <QString>

#include <functional>

#include "net/NetJob.h"

class QObject;

/* What the curated JSON lists served from meta.kiteclient.com share
 * (BundleCatalog, FeaturedServers): {"en": ..., "tr": ...} strings, and a
 * newer copy of the embedded file downloaded into the meta cache. */
namespace MetaCatalog
{
	/// The language the launcher is set to; "" without a launcher.
	QString selectedLanguage();
	/// The text of a {"en": ..., "tr": ...} value in @p language (Turkish
	/// for any "tr..."), else English, else whichever is there.
	QString localized(const QJsonValue& value, const QString& language);
	/// The meta cache's copy of META_URL + @p path, empty if there is none.
	QByteArray cached(const QString& path);
	/// Downloads META_URL + @p path into the meta cache and hands its bytes
	/// to @p downloaded (on @p context's thread); nothing, and a debug line,
	/// when there is none. Null without a launcher.
	NetJob::Ptr refresh(const QString& path, const QString& title,
						QObject* context,
						std::function<void(const QByteArray&)> downloaded);
} // namespace MetaCatalog
