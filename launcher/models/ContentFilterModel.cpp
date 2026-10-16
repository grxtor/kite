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

#include "ContentFilterModel.h"

#include <QSet>

#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/modrinth/ModrinthApi.h"

ContentFilterModel::ContentFilterModel(QObject* parent)
	: QSortFilterProxyModel(parent)
{
	setSortCaseSensitivity(Qt::CaseInsensitive);
	setDynamicSortFilter(true);

	/* `count` follows whatever changes the visible rows: a search, a file
	 * appearing or going, or the source being swapped out. */
	connect(this, &QAbstractItemModel::rowsInserted, this,
			&ContentFilterModel::countChanged);
	connect(this, &QAbstractItemModel::rowsRemoved, this,
			&ContentFilterModel::countChanged);
	connect(this, &QAbstractItemModel::modelReset, this,
			&ContentFilterModel::countChanged);
	connect(this, &QAbstractItemModel::layoutChanged, this,
			&ContentFilterModel::countChanged);
}

ContentFilterModel::~ContentFilterModel() = default;

void ContentFilterModel::setFolderModel(std::shared_ptr<ModFolderModel> model)
{
	if (m_folder == model) {
		return;
	}
	/* Swap the source first, then let go of the old model: the proxy must
	 * never point at a model that has already been destroyed. */
	auto previous = std::move(m_folder);
	m_folder = std::move(model);
	setSourceModel(m_folder.get());
	if (m_folder) {
		sort(0);
	}
	previous.reset();
	emit countChanged();
}

QHash<int, QByteArray> ContentFilterModel::roleNames() const
{
	QHash<int, QByteArray> roles = QSortFilterProxyModel::roleNames();
	roles.insert(KeyRole, "key");
	roles.insert(FileNameRole, "fileName");
	roles.insert(DescriptionRole, "description");
	roles.insert(AuthorsRole, "authors");
	roles.insert(HomepageRole, "homepage");
	roles.insert(SourceRole, "source");
	return roles;
}

QVariant ContentFilterModel::data(const QModelIndex& index, int role) const
{
	if (role < KeyRole || role > SourceRole) {
		return QSortFilterProxyModel::data(index, role);
	}
	if (!m_folder || !index.isValid()) {
		return QVariant();
	}
	const QModelIndex source = mapToSource(index);
	if (!source.isValid() || source.row() < 0 ||
		size_t(source.row()) >= m_folder->size()) {
		return QVariant();
	}
	const Mod& mod = m_folder->at(size_t(source.row()));
	switch (role) {
		case KeyRole:
			return keyFor(mod);
		case FileNameRole:
			return mod.filename().fileName();
		case DescriptionRole:
			return mod.description();
		case AuthorsRole:
			return mod.authors();
		case HomepageRole:
			return homepageFor(mod, m_folder->metadataIndex().get());
		case SourceRole:
			return sourceFor(mod, m_folder->metadataIndex().get());
		default:
			break;
	}
	return QVariant();
}

void ContentFilterModel::setFilterText(const QString& text)
{
	if (m_filterText == text) {
		return;
	}
	m_filterText = text;
	invalidateFilter();
	emit filterTextChanged();
	emit countChanged();
}

int ContentFilterModel::count() const
{
	return rowCount();
}

int ContentFilterModel::totalCount() const
{
	return m_folder ? int(m_folder->size()) : 0;
}

QModelIndexList
ContentFilterModel::sourceIndexesForKeys(const QStringList& keys) const
{
	QModelIndexList indexes;
	if (!m_folder || keys.isEmpty()) {
		return indexes;
	}
	const QSet<QString> wanted(keys.begin(), keys.end());
	for (size_t row = 0; row < m_folder->size(); ++row) {
		if (wanted.contains(keyFor(m_folder->at(row)))) {
			indexes.append(m_folder->index(int(row), 0));
		}
	}
	return indexes;
}

QStringList ContentFilterModel::visibleKeys() const
{
	QStringList keys;
	for (int row = 0; row < rowCount(); ++row) {
		keys.append(data(index(row, 0), KeyRole).toString());
	}
	return keys;
}

void ContentFilterModel::refreshRows(const QStringList& keys)
{
	for (int row = 0; row < rowCount(); ++row) {
		const QModelIndex at = index(row, 0);
		if (keys.contains(data(at, KeyRole).toString())) {
			emit dataChanged(at, at);
		}
	}
}

QString ContentFilterModel::keyFor(const Mod& mod)
{
	return ModMetadataIndex::canonicalFileName(mod.filename().fileName());
}

QString ContentFilterModel::homepageFor(const Mod& mod,
										const ModMetadataIndex* index)
{
	if (index) {
		const ModMetadataIndex::Entry entry = index->get(keyFor(mod));
		if (entry.hasPlatformOrigin()) {
			/* Each provider owns the shape of its own addresses. */
			if (entry.platform == ModrinthApi::get().id()) {
				return ModrinthApi::get()
					.projectPageUrl(entry.projectId)
					.toString();
			}
			if (entry.platform == FlameApi::get().id()) {
				return FlameApi::get().projectPageUrl(entry.projectId).toString();
			}
		}
	}
	/* What the archive says about itself, so a file added by hand still
	 * has somewhere to go. */
	return mod.homeurl();
}

QString ContentFilterModel::sourceFor(const Mod& mod,
									  const ModMetadataIndex* index)
{
	if (!index) {
		return QString();
	}
	const ModMetadataIndex::Entry entry = index->get(keyFor(mod));
	if (!entry.hasPlatformOrigin()) {
		return QString();
	}
	if (entry.platform == ModrinthApi::get().id()) {
		return ModrinthApi::get().displayName();
	}
	if (entry.platform == FlameApi::get().id()) {
		return FlameApi::get().displayName();
	}
	return QString();
}

bool ContentFilterModel::matches(const Mod& mod, const QString& text)
{
	if (text.isEmpty()) {
		return true;
	}
	if (mod.name().contains(text, Qt::CaseInsensitive) ||
		mod.description().contains(text, Qt::CaseInsensitive) ||
		mod.filename().fileName().contains(text, Qt::CaseInsensitive)) {
		return true;
	}
	for (const QString& author : mod.authors()) {
		if (author.contains(text, Qt::CaseInsensitive)) {
			return true;
		}
	}
	return false;
}

bool ContentFilterModel::filterAcceptsRow(int sourceRow,
										  const QModelIndex& sourceParent) const
{
	Q_UNUSED(sourceParent)
	if (!m_folder || sourceRow < 0 || size_t(sourceRow) >= m_folder->size()) {
		return false;
	}
	return matches(m_folder->at(size_t(sourceRow)), m_filterText.trimmed());
}

bool ContentFilterModel::lessThan(const QModelIndex& left,
								  const QModelIndex& right) const
{
	if (!m_folder) {
		return QSortFilterProxyModel::lessThan(left, right);
	}
	const Mod& a = m_folder->at(size_t(left.row()));
	const Mod& b = m_folder->at(size_t(right.row()));
	const int byName = a.name().compare(b.name(), Qt::CaseInsensitive);
	if (byName != 0) {
		return byName < 0;
	}
	/* Two copies of the same mod: keep them in a stable order. */
	return keyFor(a).compare(keyFor(b), Qt::CaseInsensitive) < 0;
}
