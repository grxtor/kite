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

#include "models/PlayTimeSummary.h"

#include "InstanceList.h"
#include "core/LauncherContext.h"

PlayTimeSummary::PlayTimeSummary(QObject* parent) : QObject(parent)
{
	if (!LAUNCHER) {
		return;
	}
	if (auto list = LAUNCHER->instances()) {
		auto recount = [this]() { refresh(); };
		connect(list.get(), &QAbstractItemModel::dataChanged, this, recount);
		connect(list.get(), &QAbstractItemModel::rowsInserted, this, recount);
		connect(list.get(), &QAbstractItemModel::rowsRemoved, this, recount);
		connect(list.get(), &QAbstractItemModel::modelReset, this, recount);
		connect(list.get(), &InstanceList::instancesChanged, this, recount);
	}
	refresh();
}

PlayTimeSummary::~PlayTimeSummary() = default;

qint64 PlayTimeSummary::totalSeconds() const
{
	return m_total;
}

void PlayTimeSummary::refresh()
{
	qint64 total = 0;
	if (LAUNCHER) {
		if (auto list = LAUNCHER->instances()) {
			total = list->getTotalPlayTime();
		}
	}
	if (total != m_total) {
		m_total = total;
		emit totalChanged();
	}
}
