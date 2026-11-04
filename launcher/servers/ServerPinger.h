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

#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "servers/ServerStatus.h"

/* The live status of servers for QML: ping(address) asks, status(address)
 * reads. One answer per server serves every view for a minute; a few pings
 * run at once and the rest wait their turn. `revision` changes with every
 * answer, so a binding that reads it with status() follows along. */
class ServerPinger : public QObject
{
	Q_OBJECT
	Q_PROPERTY(int revision READ revision NOTIFY updated)

  public:
	static constexpr int kMaxConcurrent = 4;
	static constexpr int kCacheMs = 60 * 1000;
	/// refresh() asks again only once this much has passed.
	static constexpr int kRefreshFloorMs = 5 * 1000;
	static constexpr quint16 kDefaultPort = 25565;

	struct Endpoint {
		/// Lower case, without brackets or a trailing dot; "" if none.
		QString host;
		quint16 port = kDefaultPort;
	};
	/// Reads an address the way the game does ("host", "host:port",
	/// "[ipv6]:port"). Like the game, only the default port asks DNS for an
	/// SRV record.
	static Endpoint endpointOf(const QString& address);
	/// "host:port" with the host as endpointOf() gives it: two spellings of
	/// one server ("MC.Example.com." and "mc.example.com:25565") share a key.
	/// "" for an address with no host.
	static QString keyOf(const QString& address);
	static QString keyOf(const QString& host, quint16 port);

	explicit ServerPinger(QObject* parent = nullptr);

	/// For tests.
	void setTimeoutMs(int timeoutMs);
	void setCacheMs(int cacheMs);
	void setRefreshFloorMs(int floorMs);

	int revision() const
	{
		return m_revision;
	}

	/// Pings @p address unless an answer younger than the cache, or a ping,
	/// is already there.
	Q_INVOKABLE void ping(const QString& address);
	/// Pings @p address again (the Refresh button), keeping what is shown
	/// until the answer.
	Q_INVOKABLE void refresh(const QString& address);
	/// state: "" (never asked), "pending" (no answer yet), "online" or
	/// "offline"; refreshing (a newer ping is out); and for "online" what
	/// ServerStatus::Status holds: online, max (-1 if hidden), version, motd,
	/// motdLight, motdText, favicon, ping (ms, -1 if unmeasured).
	Q_INVOKABLE QVariantMap status(const QString& address) const;
	/// The FriendsController.friends entries playing on @p address, in their
	/// order: state "playing", worldType "server" and the same host and port.
	Q_INVOKABLE QVariantList friendsOn(const QString& address,
									   const QVariantList& friends) const;

  signals:
	void updated();

  private:
	struct Entry {
		Endpoint endpoint;
		ServerStatus::Status status;
		bool answered = false;
		bool online = false;
		bool pending = false;
		qint64 askedAt = -1;
		qint64 answeredAt = -1;
	};

	void enqueue(const QString& key, const Endpoint& endpoint);
	void startNext();
	void finish(const QString& key, bool online,
				const ServerStatus::Status& status);

	QHash<QString, Entry> m_entries;
	QStringList m_queue;
	int m_running = 0;
	int m_revision = 0;
	int m_timeoutMs;
	int m_cacheMs = kCacheMs;
	int m_refreshFloorMs = kRefreshFloorMs;
	QElapsedTimer m_clock;
};
