/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include "models/NewsModel.h"

#include "news/NewsChecker.h"

NewsModel::NewsModel(NewsChecker* checker, QObject* parent)
	: QAbstractListModel(parent), m_checker(checker)
{
	if (!m_checker) {
		return;
	}
	connect(m_checker, &NewsChecker::newsLoaded, this,
			[this]() { setEntries(m_checker->getNewsEntries()); });
	// Whatever has already arrived, if the checker finished before this
	// model existed. (Not isNewsLoaded(): NewsChecker declares it but has
	// never defined it.)
	const auto entries = m_checker->getNewsEntries();
	if (!entries.isEmpty()) {
		setEntries(entries);
	}
}

int NewsModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : m_entries.size();
}

QVariant NewsModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
		return QVariant();
	}
	const auto& entry = m_entries.at(index.row());
	if (!entry) {
		return QVariant();
	}
	switch (role) {
		case Qt::DisplayRole:
		case TitleRole:
			return entry->title;
		case LinkRole:
			return entry->link;
		case AuthorRole:
			return entry->author;
		case DateRole:
			return entry->pubDate;
		default:
			return QVariant();
	}
}

QHash<int, QByteArray> NewsModel::roleNames() const
{
	return {
		{ TitleRole, "title" },
		{ LinkRole, "link" },
		{ AuthorRole, "author" },
		{ DateRole, "date" },
	};
}

int NewsModel::count() const
{
	return m_entries.size();
}

QVariantMap NewsModel::latest() const
{
	return m_entries.isEmpty() ? QVariantMap() : entryMap(m_entries.first());
}

void NewsModel::setEntries(const QList<NewsEntryPtr>& entries)
{
	beginResetModel();
	m_entries.clear();
	for (const auto& entry : entries) {
		// An entry without a title has nothing to show in a headline.
		if (entry && !entry->title.trimmed().isEmpty()) {
			m_entries.append(entry);
		}
	}
	endResetModel();
	emit entriesChanged();
}

QVariantMap NewsModel::entryMap(const NewsEntryPtr& entry) const
{
	if (!entry) {
		return {};
	}
	return {
		{ QStringLiteral("title"), entry->title },
		{ QStringLiteral("link"), entry->link },
		{ QStringLiteral("author"), entry->author },
		{ QStringLiteral("date"), entry->pubDate },
	};
}
