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

#include "LauncherImportModel.h"

#include <QLocale>

#include <algorithm>

using LauncherImport::FoundInstance;
using LauncherImport::FoundLauncher;
using LauncherImport::Mode;

LauncherImportModel::LauncherImportModel(QObject* parent)
	: QAbstractListModel(parent)
{
}

int LauncherImportModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : m_rows.size();
}

QHash<int, QByteArray> LauncherImportModel::roleNames() const
{
	return {{KeyRole, "key"},
			{LauncherRole, "launcher"},
			{LauncherNameRole, "launcherName"},
			{NameRole, "name"},
			{MinecraftVersionRole, "minecraftVersion"},
			{LoaderRole, "loader"},
			{LoaderVersionRole, "loaderVersion"},
			{SizeTextRole, "sizeText"},
			{LastPlayedRole, "lastPlayed"},
			{CheckedRole, "checked"},
			{LinkedRole, "linked"},
			{LinkableRole, "linkable"},
			{LinkBlockerRole, "linkBlocker"},
			{ProblemRole, "problem"},
			{ImportedRole, "imported"},
			{LeftBehindRole, "leftBehind"}};
}

QVariant LauncherImportModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() >= m_rows.size()) {
		return {};
	}
	const Row& row = m_rows.at(index.row());
	const FoundInstance& instance = row.instance;
	switch (role) {
	case KeyRole: return instance.key;
	case LauncherRole: return LauncherImport::sourceKey(instance.source);
	case LauncherNameRole: return LauncherImport::sourceName(instance.source);
	case NameRole: return instance.name;
	case MinecraftVersionRole: return instance.minecraftVersion;
	case LoaderRole: return instance.loader;
	case LoaderVersionRole: return instance.loaderVersion;
	case SizeTextRole:
		return instance.copyBytes < 0
				   ? QString()
				   : QLocale().formattedDataSize(instance.copyBytes, 1,
												 QLocale::DataSizeTraditionalFormat);
	/* Milliseconds, as Format.lastPlayed() takes; 0 for never. */
	case LastPlayedRole:
		return instance.lastPlayed.isValid() ? instance.lastPlayed.toMSecsSinceEpoch() : 0;
	case CheckedRole: return row.checked;
	case LinkedRole: return row.mode == Mode::Link;
	case LinkableRole: return instance.linkable();
	case LinkBlockerRole: return instance.linkBlocker;
	case ProblemRole: return instance.problem;
	case ImportedRole: return row.imported;
	case LeftBehindRole: return instance.leftBehind.join(QStringLiteral(", "));
	default: return {};
	}
}

void LauncherImportModel::reset(const QList<FoundLauncher>& launchers,
								const QSet<QString>& importedKeys)
{
	beginResetModel();
	m_rows.clear();
	for (const FoundLauncher& launcher : launchers) {
		QList<Row> rows;
		for (const FoundInstance& instance : launcher.instances) {
			Row row;
			row.instance = instance;
			row.imported = importedKeys.contains(instance.key);
			rows.append(row);
		}
		/* Within a launcher: importable first, then the last played. */
		std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
			if (a.instance.importable() != b.instance.importable()) {
				return a.instance.importable();
			}
			const qint64 left = a.instance.lastPlayed.isValid() ? a.instance.lastPlayed.toMSecsSinceEpoch() : 0;
			const qint64 right = b.instance.lastPlayed.isValid() ? b.instance.lastPlayed.toMSecsSinceEpoch() : 0;
			if (left != right) {
				return left > right;
			}
			return a.instance.name.localeAwareCompare(b.instance.name) < 0;
		});
		m_rows.append(rows);
	}
	endResetModel();
	emit countChanged();
}

int LauncherImportModel::indexOf(const QString& key) const
{
	for (int i = 0; i < m_rows.size(); ++i) {
		if (m_rows.at(i).instance.key == key) {
			return i;
		}
	}
	return -1;
}

bool LauncherImportModel::setChecked(int row, bool checked)
{
	if (row < 0 || row >= m_rows.size() || m_rows.at(row).checked == checked) {
		return false;
	}
	if (checked && !m_rows.at(row).instance.importable()) {
		return false;
	}
	m_rows[row].checked = checked;
	changed(row, {CheckedRole});
	return true;
}

bool LauncherImportModel::setMode(int row, Mode mode)
{
	if (row < 0 || row >= m_rows.size() || m_rows.at(row).mode == mode) {
		return false;
	}
	if (mode == Mode::Link && !m_rows.at(row).instance.linkable()) {
		return false;
	}
	m_rows[row].mode = mode;
	changed(row, {LinkedRole});
	return true;
}

void LauncherImportModel::setImported(int row)
{
	if (row < 0 || row >= m_rows.size()) {
		return;
	}
	m_rows[row].imported = true;
	m_rows[row].checked = false;
	changed(row, {ImportedRole, CheckedRole});
}

void LauncherImportModel::changed(int row, const QList<int>& roles)
{
	const QModelIndex at = index(row);
	emit dataChanged(at, at, roles);
}
