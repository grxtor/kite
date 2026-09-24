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

#include "ServersListModel.h"

#include <QDebug>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <sstream>

#include "FileSystem.h"
#include <io/stream_reader.h>
#include <io/stream_writer.h>
#include <tag_compound.h>
#include <tag_list.h>
#include <tag_primitive.h>
#include <tag_string.h>

namespace
{
/* Same shape as ServersPage.cpp's file-local parseServersDat()/
 * serializeServerDat(): duplicated rather than shared, because the widget
 * version is a static function private to that .cpp, and the whole point
 * of this class is that nothing here links against ui/. */
std::unique_ptr<nbt::tag_compound> parseServersDat(const QString& filename)
{
	// A fresh instance has no servers.dat yet: that is the normal empty
	// state, not an error worth a critical log line from FS::read().
	if (!QFileInfo::exists(filename)) {
		return nullptr;
	}
	try {
		QByteArray input = FS::read(filename);
		std::istringstream stream(std::string(input.constData(), input.size()));
		auto pair = nbt::io::read_compound(stream);
		if (pair.first != "" || pair.second == nullptr) {
			return nullptr;
		}
		return std::move(pair.second);
	} catch (...) {
		return nullptr;
	}
}

bool serializeServersDat(const QString& filename, nbt::tag_compound* root)
{
	try {
		if (!FS::ensureFilePathExists(filename)) {
			return false;
		}
		std::ostringstream stream;
		nbt::io::write_tag("", *root, stream);
		const QByteArray bytes(stream.str().data(),
							   static_cast<int>(stream.str().size()));
		FS::write(filename, bytes);
		return true;
	} catch (...) {
		return false;
	}
}
} // namespace

ServersListModel::ServersListModel(QString gameRoot, QObject* parent)
	: QAbstractListModel(parent), m_gameRoot(std::move(gameRoot))
{
	m_watcher = new QFileSystemWatcher(this);
	connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
			&ServersListModel::directoryChanged);

	m_saveTimer.setSingleShot(true);
	// Same five-second debounce ServersPage's ServersModel used.
	m_saveTimer.setInterval(5000);
	connect(&m_saveTimer, &QTimer::timeout, this, &ServersListModel::saveNow);
}

ServersListModel::~ServersListModel()
{
	saveNow();
}

QString ServersListModel::serversPath() const
{
	return QFileInfo(FS::PathCombine(m_gameRoot, "servers.dat")).filePath();
}

void ServersListModel::load()
{
	cancelSave();
	beginResetModel();
	QList<ServerEntry> servers;
	if (auto root = parseServersDat(serversPath())) {
		if (root->has_key("servers", nbt::tag_type::List)) {
			auto& list = root->at("servers").as<nbt::tag_list>();
			for (auto& entry : list) {
				auto& compound = entry.as<nbt::tag_compound>();
				ServerEntry server;
				try {
					std::string address(compound["ip"]);
					server.address = QString::fromUtf8(address.c_str());
					std::string name(compound["name"]);
					server.name = QString::fromUtf8(name.c_str());
				} catch (...) {
					continue;
				}
				if (compound.has_key("acceptTextures", nbt::tag_type::Byte)) {
					const bool always =
						compound["acceptTextures"].as<nbt::tag_byte>().get();
					server.acceptTextures = always ? 1 : 2;
				}
				servers.append(server);
			}
		}
	}
	m_servers.swap(servers);
	m_loaded = true;
	endResetModel();
}

void ServersListModel::saveNow()
{
	cancelSave();
	if (!m_loaded) {
		// Never overwrite a file this model has not actually read yet.
		return;
	}
	nbt::tag_compound root;
	nbt::tag_list list;
	for (const auto& server : m_servers) {
		nbt::tag_compound entry;
		entry.insert("name", server.name.trimmed().toUtf8().toStdString());
		entry.insert("ip", server.address.trimmed().toUtf8().toStdString());
		if (server.acceptTextures != 0) {
			entry.insert("acceptTextures",
						nbt::tag_byte(server.acceptTextures == 1));
		}
		list.push_back(std::move(entry));
	}
	root.insert("servers", nbt::value(std::move(list)));

	if (!serializeServersDat(serversPath(), &root)) {
		qWarning() << "ServersListModel: failed to save" << serversPath()
				  << "- will retry";
		scheduleSave();
	}
}

void ServersListModel::scheduleSave()
{
	m_dirty = true;
	m_saveTimer.start();
}

void ServersListModel::cancelSave()
{
	m_dirty = false;
	m_saveTimer.stop();
}

void ServersListModel::updateFsWatch()
{
	// Mirrors ServersModel::updateFSObserver(): watch only while the page is
	// open AND the instance is running (i.e. editing is refused) - never
	// while the user could be mid-edit, so an external directory change can
	// never clobber unsaved work.
	const bool watching = m_watcher->directories().contains(m_gameRoot);
	if (m_observed && m_locked) {
		if (!watching) {
			m_watcher->addPath(m_gameRoot);
		}
	} else if (watching) {
		m_watcher->removePath(m_gameRoot);
	}
}

void ServersListModel::startWatching()
{
	if (m_observed) {
		return;
	}
	m_observed = true;
	if (!m_loaded) {
		load();
	}
	updateFsWatch();
}

void ServersListModel::stopWatching()
{
	if (!m_observed) {
		return;
	}
	m_observed = false;
	saveNow();
	updateFsWatch();
}

void ServersListModel::directoryChanged(const QString&)
{
	// A launch writes servers.dat itself (joining a server adds it back to
	// the top of the list) - reload rather than clobber that with whatever
	// this model still has queued.
	load();
}

void ServersListModel::setLocked(bool locked)
{
	if (m_locked == locked) {
		return;
	}
	m_locked = locked;
	if (m_locked) {
		saveNow();
	}
	updateFsWatch();
	emit lockedChanged();
}

QVariant ServersListModel::data(const QModelIndex& index, int role) const
{
	const int row = index.row();
	if (row < 0 || row >= m_servers.size()) {
		return {};
	}
	const auto& server = m_servers.at(row);
	switch (role) {
		case NameRole:
		case Qt::DisplayRole:
			return server.name;
		case AddressRole:
			return server.address;
		case AcceptTexturesRole:
			return server.acceptTextures;
		default:
			return {};
	}
}

int ServersListModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid()) {
		return 0;
	}
	return m_servers.size();
}

QHash<int, QByteArray> ServersListModel::roleNames() const
{
	return {
		{ NameRole, "name" },
		{ AddressRole, "address" },
		{ AcceptTexturesRole, "acceptTextures" },
	};
}

int ServersListModel::addServer()
{
	if (m_locked) {
		return -1;
	}
	if (!m_loaded) {
		load();
	}
	const int row = m_servers.size();
	beginInsertRows(QModelIndex(), row, row);
	ServerEntry server;
	server.name = tr("Minecraft Server");
	m_servers.append(server);
	endInsertRows();
	scheduleSave();
	return row;
}

bool ServersListModel::removeServer(int row)
{
	if (m_locked || row < 0 || row >= m_servers.size()) {
		return false;
	}
	beginRemoveRows(QModelIndex(), row, row);
	m_servers.removeAt(row);
	endRemoveRows();
	scheduleSave();
	return true;
}

bool ServersListModel::moveUp(int row)
{
	if (m_locked || row <= 0 || row >= m_servers.size()) {
		return false;
	}
	beginMoveRows(QModelIndex(), row, row, QModelIndex(), row - 1);
	m_servers.swapItemsAt(row - 1, row);
	endMoveRows();
	scheduleSave();
	return true;
}

bool ServersListModel::moveDown(int row)
{
	if (m_locked || row < 0 || row + 1 >= m_servers.size()) {
		return false;
	}
	beginMoveRows(QModelIndex(), row, row, QModelIndex(), row + 2);
	m_servers.swapItemsAt(row + 1, row);
	endMoveRows();
	scheduleSave();
	return true;
}

void ServersListModel::setName(int row, const QString& name)
{
	if (m_locked || row < 0 || row >= m_servers.size()) {
		return;
	}
	if (m_servers[row].name == name) {
		return;
	}
	m_servers[row].name = name;
	emit dataChanged(index(row), index(row), { NameRole, Qt::DisplayRole });
	scheduleSave();
}

void ServersListModel::setAddress(int row, const QString& address)
{
	if (m_locked || row < 0 || row >= m_servers.size()) {
		return;
	}
	if (m_servers[row].address == address) {
		return;
	}
	m_servers[row].address = address;
	emit dataChanged(index(row), index(row), { AddressRole });
	scheduleSave();
}

void ServersListModel::setAcceptTextures(int row, int mode)
{
	if (m_locked || row < 0 || row >= m_servers.size() || mode < 0 ||
		mode > 2) {
		return;
	}
	if (m_servers[row].acceptTextures == mode) {
		return;
	}
	m_servers[row].acceptTextures = mode;
	emit dataChanged(index(row), index(row), { AcceptTexturesRole });
	scheduleSave();
}

QString ServersListModel::addressOf(int row) const
{
	if (row < 0 || row >= m_servers.size()) {
		return {};
	}
	return m_servers.at(row).address;
}
