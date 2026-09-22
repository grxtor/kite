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

#include "IdSelectionModel.h"

IdSelectionModel::IdSelectionModel(QObject* parent) : QObject(parent) {}

int IdSelectionModel::count() const
{
	return m_selected.count();
}

QStringList IdSelectionModel::selectedIds() const
{
	return QStringList(m_selected.values());
}

QString IdSelectionModel::currentId() const
{
	return m_currentId;
}

bool IdSelectionModel::setCurrentIdInternal(const QString& id)
{
	if (m_currentId == id) {
		return false;
	}
	m_currentId = id;
	Q_EMIT currentIdChanged();
	return true;
}

void IdSelectionModel::select(const QString& id)
{
	// Empty is the "nothing selected" sentinel currentId() uses; selecting
	// it would make an empty selection indistinguishable from one that
	// contains it.
	if (id.isEmpty() || m_selected.contains(id)) {
		return;
	}
	m_selected.insert(id);
	setCurrentIdInternal(id);
	Q_EMIT selectedIdsChanged();
	Q_EMIT countChanged();
	Q_EMIT changed();
}

void IdSelectionModel::deselect(const QString& id)
{
	if (!m_selected.remove(id)) {
		return;
	}
	if (m_currentId == id) {
		setCurrentIdInternal(QString());
	}
	Q_EMIT selectedIdsChanged();
	Q_EMIT countChanged();
	Q_EMIT changed();
}

void IdSelectionModel::toggle(const QString& id)
{
	if (m_selected.contains(id)) {
		deselect(id);
	} else {
		select(id);
	}
}

void IdSelectionModel::clear()
{
	if (m_selected.isEmpty() && m_currentId.isEmpty()) {
		return;
	}
	m_selected.clear();
	setCurrentIdInternal(QString());
	Q_EMIT selectedIdsChanged();
	Q_EMIT countChanged();
	Q_EMIT changed();
}

void IdSelectionModel::selectOnly(const QString& id)
{
	if (id.isEmpty()) {
		clear();
		return;
	}
	if (m_selected.size() == 1 && m_selected.contains(id) &&
		m_currentId == id) {
		return;
	}
	m_selected.clear();
	m_selected.insert(id);
	setCurrentIdInternal(id);
	Q_EMIT selectedIdsChanged();
	Q_EMIT countChanged();
	Q_EMIT changed();
}

bool IdSelectionModel::isSelected(const QString& id) const
{
	return m_selected.contains(id);
}
