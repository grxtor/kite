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

#include "ContentFilterModel.h"

#include <QSet>
#include <QTimer>

#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/ContentLogoFetch.h"

namespace
{
	/* Files are read one after another, each announcing itself; look for
	 * missing logos once that settles, not per file. */
	constexpr int kLogoScanDelayMs = 500;
} // namespace

ContentFilterModel::ContentFilterModel(QObject* parent)
	: QSortFilterProxyModel(parent)
{
	setSortCaseSensitivity(Qt::CaseInsensitive);
	setDynamicSortFilter(true);

	m_logoTimer = new QTimer(this);
	m_logoTimer->setSingleShot(true);
	m_logoTimer->setInterval(kLogoScanDelayMs);
	connect(m_logoTimer, &QTimer::timeout, this,
			&ContentFilterModel::fetchLogos);

	/* `count` follows whatever changes the visible rows. */
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
	for (const auto& connection : std::as_const(m_folderConnections)) {
		disconnect(connection);
	}
	m_folderConnections.clear();
	m_folder = std::move(model);
	setSourceModel(m_folder.get());
	if (m_folder) {
		sort(0);
		/* A file just read or just added may need its platform logo. */
		const auto schedule = [this] { scheduleLogoFetch(); };
		m_folderConnections = {
			connect(m_folder.get(), &QAbstractItemModel::dataChanged, this, schedule),
			connect(m_folder.get(), &QAbstractItemModel::rowsInserted, this,
					schedule)};
		scheduleLogoFetch();
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
	const bool fromSidecar = role == ModFolderModel::NameRole ||
							 role == ModFolderModel::VersionRole ||
							 role == ModFolderModel::IconUrlRole;
	if (!fromSidecar && (role < KeyRole || role > SourceRole)) {
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
	const ModMetadataIndex* metadata = m_folder->metadataIndex().get();
	switch (role) {
		case ModFolderModel::NameRole:
			return nameFor(mod, metadata);
		case ModFolderModel::VersionRole: {
			/* Empty: the folder model's own placeholder ("Folder", "File"). */
			const QString version = versionFor(mod, metadata);
			return version.isEmpty() ? QSortFilterProxyModel::data(index, role)
									 : QVariant(version);
		}
		case ModFolderModel::IconUrlRole: {
			/* The archive's logo, else the platform's once it is on disk. */
			const QVariant own = QSortFilterProxyModel::data(index, role);
			if (!own.toString().isEmpty() || !metadata) {
				return own;
			}
			return storedLogoUrl(metadata->get(keyFor(mod)).iconUrl,
								 m_folder->iconCacheDir());
		}
		case KeyRole:
			return keyFor(mod);
		case FileNameRole:
			return mod.filename().fileName();
		case DescriptionRole:
			return descriptionFor(mod, metadata);
		case AuthorsRole:
			return mod.authors();
		case HomepageRole:
			return homepageFor(mod, metadata);
		case SourceRole:
			return sourceFor(mod, metadata);
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
	/* A recorded name can be the shown one, which sorts and is searched. */
	invalidate();
	announce(keys, {});
	/* The sidecar may name a logo now. */
	scheduleLogoFetch();
}

void ContentFilterModel::scheduleLogoFetch()
{
	if (!m_logoTimer->isActive()) {
		m_logoTimer->start();
	}
}

void ContentFilterModel::fetchLogos()
{
	if (!m_folder) {
		return;
	}
	auto* fetch = new ContentLogoFetch(m_folder, this);
	connect(fetch, &ContentLogoFetch::stored, this,
			[this](const QStringList& keys) {
				announce(keys, {ModFolderModel::IconUrlRole});
			});
	fetch->start();
}

void ContentFilterModel::announce(const QStringList& keys,
								  const QList<int>& roles)
{
	const QSet<QString> wanted(keys.begin(), keys.end());
	const int rows = rowCount();
	int first = -1;
	for (int row = 0; row <= rows; ++row) {
		const bool hit =
			row < rows &&
			wanted.contains(data(index(row, 0), KeyRole).toString());
		if (hit && first < 0) {
			first = row;
		} else if (!hit && first >= 0) {
			emit dataChanged(index(first, 0), index(row - 1, 0), roles);
			first = -1;
		}
	}
}

bool ContentFilterModel::filterAcceptsRow(int sourceRow,
										  const QModelIndex& sourceParent) const
{
	Q_UNUSED(sourceParent)
	if (!m_folder || sourceRow < 0 || size_t(sourceRow) >= m_folder->size()) {
		return false;
	}
	return matches(m_folder->at(size_t(sourceRow)), m_filterText.trimmed(),
				   m_folder->metadataIndex().get());
}

bool ContentFilterModel::lessThan(const QModelIndex& left,
								  const QModelIndex& right) const
{
	if (!m_folder) {
		return QSortFilterProxyModel::lessThan(left, right);
	}
	const Mod& a = m_folder->at(size_t(left.row()));
	const Mod& b = m_folder->at(size_t(right.row()));
	const ModMetadataIndex* index = m_folder->metadataIndex().get();
	const int byName =
		nameFor(a, index).compare(nameFor(b, index), Qt::CaseInsensitive);
	if (byName != 0) {
		return byName < 0;
	}
	return keyFor(a).compare(keyFor(b), Qt::CaseInsensitive) < 0;
}
