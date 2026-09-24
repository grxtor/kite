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

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QTimer>

class QFileSystemWatcher;

/*
 * QML-facing list of one instance's servers.dat - the widget-free
 * replacement for ServersPage's private ServersModel.
 *
 * Same on-disk format and save-debounce behaviour as the widget version:
 * NBT, saved five seconds after the last edit (or immediately on
 * destruction/lock), reloaded whenever the directory changes underneath it
 * (multiplayer.txt written by another launcher, or the game itself while
 * running). Locked (edits refused) while the instance is running, the same
 * rule ServersPage applied - the game already has the file open.
 *
 * No icon decoding: servers.dat carries a base64 favicon the server itself
 * sent on ping, but nothing here lets the user set one by hand (the widget
 * page did not either - only "accept textures" was ever user-editable), so
 * this model does not expose it. Every row shows the same generic glyph.
 */
class ServersListModel : public QAbstractListModel
{
	Q_OBJECT

	Q_PROPERTY(bool locked READ locked NOTIFY lockedChanged)

  public:
	enum Roles {
		NameRole = Qt::UserRole + 1,
		AddressRole,
		/// 0 = ask, 1 = always, 2 = never - same values as the widget's
		/// Server::AcceptsTextures enum, so a QML combo box can index
		/// straight into it.
		AcceptTexturesRole,
	};

	/// @p gameRoot: the instance's gameRoot() - servers.dat lives directly
	/// under it, same path ServersPage used.
	explicit ServersListModel(QString gameRoot, QObject* parent = nullptr);
	~ServersListModel() override;

	QVariant data(const QModelIndex& index,
				 int role = Qt::DisplayRole) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QHash<int, QByteArray> roleNames() const override;

	bool locked() const
	{
		return m_locked;
	}
	/// Called by InstanceDetails when the instance's running state changes.
	void setLocked(bool locked);

	/// Starts/stops watching servers.dat's directory for external changes
	/// and, on stop, flushes any pending save - mirrors ServersModel::
	/// observe()/unobserve() and BackupPage-style lifetime management.
	void startWatching();
	void stopWatching();

	/// Appends an empty server ("Minecraft Server", no address) and
	/// returns its row, or -1 while locked.
	Q_INVOKABLE int addServer();
	Q_INVOKABLE bool removeServer(int row);
	Q_INVOKABLE bool moveUp(int row);
	Q_INVOKABLE bool moveDown(int row);
	Q_INVOKABLE void setName(int row, const QString& name);
	Q_INVOKABLE void setAddress(int row, const QString& address);
	/// @p mode: 0/1/2, see AcceptTexturesRole.
	Q_INVOKABLE void setAcceptTextures(int row, int mode);
	/// The address to join, or empty if @p row is out of range or has no
	/// address set yet - callers should refuse to join in that case.
	Q_INVOKABLE QString addressOf(int row) const;

  signals:
	void lockedChanged();

  private slots:
	void directoryChanged(const QString& path);

  private:
	struct ServerEntry {
		QString name;
		QString address;
		int acceptTextures = 0;
	};

	void load();
	void saveNow();
	void scheduleSave();
	void cancelSave();
	QString serversPath() const;
	void updateFsWatch();

	QString m_gameRoot;
	QList<ServerEntry> m_servers;
	bool m_loaded = false;
	bool m_locked = false;
	bool m_observed = false;
	bool m_dirty = false;
	QFileSystemWatcher* m_watcher = nullptr;
	QTimer m_saveTimer;
};
