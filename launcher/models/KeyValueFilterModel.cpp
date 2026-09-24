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

#include "KeyValueFilterModel.h"

KeyValueFilterModel::KeyValueFilterModel(QObject* parent)
	: QSortFilterProxyModel(parent)
{
	setDynamicSortFilter(true);
}

void KeyValueFilterModel::setFilterText(const QString& text)
{
	if (m_filterText == text) {
		return;
	}
	m_filterText = text;
	emit filterTextChanged();
	invalidateFilter();
}

bool KeyValueFilterModel::filterAcceptsRow(int sourceRow,
										   const QModelIndex& sourceParent) const
{
	if (m_filterText.isEmpty() || !sourceModel()) {
		return true;
	}
	const auto index = sourceModel()->index(sourceRow, 0, sourceParent);
	const auto roles = sourceModel()->roleNames();
	for (auto it = roles.constBegin(); it != roles.constEnd(); ++it) {
		const QVariant value = index.data(it.key());
		if (value.canConvert<QString>() &&
			value.toString().contains(m_filterText, Qt::CaseInsensitive)) {
			return true;
		}
	}
	return false;
}
