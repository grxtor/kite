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

#include <QAbstractListModel>
#include <QList>
#include <QPointer>
#include <QVariantMap>

#include "news/NewsEntry.h"

class NewsChecker;

/*
 * The launcher's news for QML: one row per entry of the application's
 * NewsChecker (every feed merged, newest first -- see
 * NewsChecker::getNewsEntries()), following it whenever it finishes
 * loading. The checker belongs to the application (LauncherContext::
 * newsChecker()), which decides when to load; this model never fetches
 * anything itself.
 *
 * Roles: title, link, author, date (a QDateTime).
 */
class NewsModel : public QAbstractListModel
{
	Q_OBJECT

	Q_PROPERTY(int count READ count NOTIFY entriesChanged)
	/// The newest entry as {title, link, author, date}, or an empty map.
	Q_PROPERTY(QVariantMap latest READ latest NOTIFY entriesChanged)

  public:
	enum Roles {
		TitleRole = Qt::UserRole + 1,
		LinkRole,
		AuthorRole,
		DateRole,
	};

	/* Follows @p checker, which may be null (a build without news, or a
	 * test that feeds entries through setEntries() instead). */
	explicit NewsModel(NewsChecker* checker = nullptr,
					   QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	int count() const;
	QVariantMap latest() const;

	/* Replaces every row. Called with the checker's entries each time it
	 * finishes loading; public so a test can fill the model directly. */
	void setEntries(const QList<NewsEntryPtr>& entries);

  signals:
	void entriesChanged();

  private:
	QVariantMap entryMap(const NewsEntryPtr& entry) const;

	QPointer<NewsChecker> m_checker;
	QList<NewsEntryPtr> m_entries;
};
