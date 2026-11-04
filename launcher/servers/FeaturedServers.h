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
#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "net/NetJob.h"

/* The big servers Home offers to join in one click. The built-in list
 * (:/kite/servers.json) is replaced by meta.kiteclient.com's copy via
 * refresh(). Each entry: id, name, address, an optional {"en", "tr"} note,
 * an optional minVersion/maxVersion of Minecraft and optional versions: the
 * release lines it takes when they have gaps (["1.8", "1.21", "26"] for
 * 1.8.x, 1.21.x and 26.x). */
class FeaturedServers : public QObject
{
	Q_OBJECT

	/// id, name, address, note ("" if none), minVersion, maxVersion ("" for
	/// no limit), versions ([] for any line), in the list's order.
	Q_PROPERTY(QVariantList servers READ servers NOTIFY changed)

  public:
	explicit FeaturedServers(QObject* parent = nullptr);

	/// Replaces the list with @p json when it is a valid format-1 list;
	/// otherwise leaves it as it was and says why in @p error.
	bool loadFromJson(const QByteArray& json, QString* error = nullptr);
	/// Overrides the selected language, for tests.
	void setLanguage(const QString& language);

	QVariantList servers() const;

	/// Whether Minecraft @p version can join a server that takes
	/// @p minVersion to @p maxVersion (either "" for no limit) and, unless
	/// @p lines is empty, one of those release lines. A maximum of "1.21"
	/// takes every 1.21.x, and so does a line of "1.21".
	Q_INVOKABLE static bool versionFits(const QString& version,
										const QString& minVersion,
										const QString& maxVersion,
										const QStringList& lines = {});

	/// Replaces the list with the copy refresh() last downloaded, if valid;
	/// no network.
	bool loadCached();
	/// Asks meta.kiteclient.com for a newer list; once per run, silent when
	/// there is none.
	Q_INVOKABLE void refresh();

  signals:
	void changed();

  private:
	QString language() const;

	QJsonArray m_servers;
	QString m_language;
	bool m_refreshed = false;
	NetJob::Ptr m_refreshJob;
};
