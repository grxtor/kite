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

#include "GameOptions.h"
#include "FileSystem.h"
#include "Logging.h"
#include <QDebug>
#include <QSaveFile>

namespace
{
	bool load(const QString& path, std::vector<GameOptionItem>& contents,
			  int& version)
	{
		contents.clear();
		QFile file(path);
		if (!file.open(QFile::ReadOnly)) {
			qWarning() << "Failed to read options file.";
			return false;
		}
		version = 0;
		while (!file.atEnd()) {
			auto line = file.readLine();
			if (line.endsWith('\n')) {
				line.chop(1);
			}
			auto separatorIndex = line.indexOf(':');
			if (separatorIndex == -1) {
				continue;
			}
			auto key = QString::fromUtf8(line.data(), separatorIndex);
			auto value = QString::fromUtf8(line.data() + separatorIndex + 1,
										   line.size() - 1 - separatorIndex);
			qCDebug(minecraftgoptionsLog) << "!!" << key << "!!";
			if (key == "version") {
				version = value.toInt();
				continue;
			}
			contents.emplace_back(GameOptionItem{key, value});
		}
		qCDebug(minecraftgoptionsLog) << "Loaded" << path << "with version:" << version;
		return true;
	}
	bool save(const QString& path, std::vector<GameOptionItem>& mapping,
			  int version)
	{
		QSaveFile out(path);
		if (!out.open(QIODevice::WriteOnly)) {
			return false;
		}
		if (version != 0) {
			QString versionLine = QString("version:%1\n").arg(version);
			out.write(versionLine.toUtf8());
		}
		auto iter = mapping.begin();
		while (iter != mapping.end()) {
			out.write(iter->key.toUtf8());
			out.write(":");
			out.write(iter->value.toUtf8());
			out.write("\n");
			iter++;
		}
		return out.commit();
	}
} // namespace

GameOptions::GameOptions(const QString& path) : path(path)
{
	reload();
}

QVariant GameOptions::headerData(int section, Qt::Orientation orientation,
								 int role) const
{
	if (role != Qt::DisplayRole) {
		return QAbstractListModel::headerData(section, orientation, role);
	}
	switch (section) {
		case 0:
			return tr("Key");
		case 1:
			return tr("Value");
		default:
			return QVariant();
	}
}

QVariant GameOptions::data(const QModelIndex& index, int role) const
{
	if (!index.isValid())
		return QVariant();

	int row = index.row();
	int column = index.column();

	if (row < 0 || row >= int(contents.size()))
		return QVariant();

	switch (role) {
		case Qt::DisplayRole:
			if (column == 0) {
				return contents[row].key;
			} else {
				return contents[row].value;
			}

		// Named roles for QML: a ListView only ever binds to column 0, so
		// these return the key/value pair regardless of `column`, making
		// both reachable from a single delegate.
		case KeyRole:
			return contents[row].key;
		case ValueRole:
			return contents[row].value;

		default:
			return QVariant();
	}
	return QVariant();
}

int GameOptions::rowCount(const QModelIndex&) const
{
	return static_cast<int>(contents.size());
}

int GameOptions::columnCount(const QModelIndex&) const
{
	return 2;
}

QHash<int, QByteArray> GameOptions::roleNames() const
{
	QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
	roles.insert(KeyRole, "key");
	roles.insert(ValueRole, "value");
	return roles;
}

bool GameOptions::isLoaded() const
{
	return loaded;
}

bool GameOptions::reload()
{
	beginResetModel();
	loaded = load(path, contents, version);
	endResetModel();
	return loaded;
}

bool GameOptions::save()
{
	return ::save(path, contents, version);
}
