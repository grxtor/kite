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

#include <map>
#include <QString>
#include <QAbstractListModel>

struct GameOptionItem {
	QString key;
	QString value;
};

class GameOptions : public QAbstractListModel
{
	Q_OBJECT
  public:
	/* Named roles, so a QML ListView (which only ever sees column 0) can
	 * still get at what the widget view shows as column 1. */
	enum ModelRoles {
		KeyRole = Qt::UserRole,
		ValueRole
	};

	explicit GameOptions(const QString& path);
	virtual ~GameOptions() = default;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	int columnCount(const QModelIndex& parent) const override;
	QVariant data(const QModelIndex& index,
				  int role = Qt::DisplayRole) const override;
	QVariant headerData(int section, Qt::Orientation orientation,
						int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	bool isLoaded() const;
	bool reload();
	bool save();

  private:
	std::vector<GameOptionItem> contents;
	bool loaded = false;
	QString path;
	int version = 0;
};
