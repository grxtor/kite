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

#include "AtlOptionalModsModel.h"

AtlOptionalModsModel::AtlOptionalModsModel(
	const QVector<ATLauncher::VersionMod>& mods, QObject* parent)
	: QAbstractListModel(parent), m_mods(mods)
{
	for (int i = 0; i < m_mods.size(); ++i) {
		m_index.insert(m_mods.at(i).name, i);
		m_selection.insert(m_mods.at(i).name, false);
	}
	/* Where the pack says to start. */
	for (int i = 0; i < m_mods.size(); ++i) {
		setMod(i, m_mods.at(i).selected);
	}
}

int AtlOptionalModsModel::selectedCount() const
{
	int n = 0;
	for (const auto& mod : m_mods) {
		if (m_selection.value(mod.name)) {
			++n;
		}
	}
	return n;
}

int AtlOptionalModsModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : int(m_mods.size());
}

QVariant AtlOptionalModsModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_mods.size()) {
		return QVariant();
	}
	const auto& mod = m_mods.at(index.row());
	switch (role) {
		case Qt::DisplayRole:
		case NameRole:
			return mod.name;
		case DescriptionRole:
			return mod.description;
		case SelectedRole:
			return m_selection.value(mod.name);
		case RecommendedRole:
			return mod.recommended;
		case HiddenRole:
			return mod.effectively_hidden;
		case GroupRole:
			return mod.group;
		default:
			break;
	}
	return QVariant();
}

QHash<int, QByteArray> AtlOptionalModsModel::roleNames() const
{
	return {
		{NameRole, "name"},
		{DescriptionRole, "description"},
		{SelectedRole, "selected"},
		{RecommendedRole, "recommended"},
		{HiddenRole, "hidden"},
		{GroupRole, "group"},
	};
}

void AtlOptionalModsModel::toggle(int row)
{
	if (row < 0 || row >= m_mods.size()) {
		return;
	}
	setSelected(row, !m_selection.value(m_mods.at(row).name));
}

void AtlOptionalModsModel::setSelected(int row, bool selected)
{
	if (row < 0 || row >= m_mods.size()) {
		return;
	}
	setMod(row, selected);
	emit dataChanged(index(0), index(int(m_mods.size()) - 1), {SelectedRole});
	emit selectionChanged();
}

void AtlOptionalModsModel::selectRecommended()
{
	for (const auto& mod : m_mods) {
		m_selection[mod.name] = mod.recommended;
	}
	if (!m_mods.isEmpty()) {
		emit dataChanged(index(0), index(int(m_mods.size()) - 1), {SelectedRole});
	}
	emit selectionChanged();
}

void AtlOptionalModsModel::clearAll()
{
	for (const auto& mod : m_mods) {
		m_selection[mod.name] = false;
	}
	m_dependants.clear();
	if (!m_mods.isEmpty()) {
		emit dataChanged(index(0), index(int(m_mods.size()) - 1), {SelectedRole});
	}
	emit selectionChanged();
}

QVector<QString> AtlOptionalModsModel::selectedNames() const
{
	QVector<QString> names;
	for (const auto& mod : m_mods) {
		if (m_selection.value(mod.name)) {
			names.append(mod.name);
		}
	}
	return names;
}

void AtlOptionalModsModel::setMod(int index, bool enable)
{
	const ATLauncher::VersionMod mod = m_mods.at(index);
	if (m_selection.value(mod.name) == enable) {
		return;
	}
	m_selection[mod.name] = enable;

	/* One of a group at a time. */
	if (enable && !mod.group.isEmpty()) {
		for (int i = 0; i < m_mods.size(); ++i) {
			if (i != index && m_mods.at(i).group == mod.group) {
				setMod(i, false);
			}
		}
	}

	for (const QString& dependencyName : mod.depends) {
		/* A pack naming a dependency it does not list (the widget model
		 * would have taken the first mod instead). */
		if (!m_index.contains(dependencyName)) {
			continue;
		}
		const int dependencyIndex = m_index.value(dependencyName);
		if (enable) {
			setMod(dependencyIndex, true);
		}
		/* Who needs a hidden dependency, so it can go when nothing does. */
		QStringList& dependants = m_dependants[dependencyName];
		if (enable) {
			if (!dependants.contains(mod.name)) {
				dependants.append(mod.name);
			}
		} else {
			dependants.removeAll(mod.name);
			if (m_mods.at(dependencyIndex).effectively_hidden &&
				dependants.isEmpty()) {
				setMod(dependencyIndex, false);
			}
		}
	}

	/* Unticking a mod unticks what depends on it. */
	if (!enable) {
		const QStringList dependants = m_dependants.value(mod.name);
		for (const QString& dependantName : dependants) {
			if (m_index.contains(dependantName)) {
				setMod(m_index.value(dependantName), false);
			}
		}
	}
}
