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
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QTcpServer>

#include <memory>

#include "friends/FriendsApi.h"
#include "ModBridgeProtocol.h"
#include "ModBridgeSource.h"

class BaseInstance;
class FriendsController;
class QTcpSocket;

/*
 * The launcher's end of the Kite mod <-> launcher bridge (protocol v1,
 * kite-mod/docs/BRIDGE.md): a tiny HTTP server on 127.0.0.1 that lets the
 * in-game Friends page read the friends cache and report the player's world.
 * It listens only while a game with the Kite mod runs: registerSession() gives
 * that game its own port and random token, unregisterSession() forgets it, and
 * the last one closes the socket. One request per connection; the other rules
 * are in ModBridgeProtocol.
 */
class ModBridge : public QObject
{
	Q_OBJECT

  public:
	using Grant = ModBridgeProtocol::Grant;

	/// @p launcher is what /v1/hello calls the launcher ("Kite 11.0.0").
	explicit ModBridge(QString launcher, QObject* parent = nullptr);
	~ModBridge() override;

	/// A session for @p instance's next game, opening the socket if it is
	/// not open: its port and token. Invalid when the socket cannot be
	/// opened. A session the instance still has is replaced.
	Grant registerSession(BaseInstance* instance);
	/// Idempotent. Closes the socket (and every connection) with the last
	/// session.
	void unregisterSession(BaseInstance* instance);

	bool listening() const;
	quint16 port() const;
	int sessionCount() const;
	int connectionCount() const;
	/// kRequestTimeoutMs unless a test wants it shorter.
	void setRequestTimeoutMs(int milliseconds);

	/// The friends data to serve (the launcher's controller), and the
	/// route of reported worlds to it. Null: Friends is off for the game.
	void setFriends(FriendsController* controller);
	void setSource(std::shared_ptr<ModBridgeSource> source);

  signals:
	void worldReported(BaseInstance* instance, FriendsApi::World world);

  private:
	struct Session {
		QByteArray token;
		QMetaObject::Connection onStopped;
		QMetaObject::Connection onDestroyed;
	};
	struct Connection {
		QByteArray buffer;
		bool answered = false;
	};

	bool forgetSession(BaseInstance* instance);

	// Sockets (ModBridge.cpp)
	void onNewConnection();
	void onReadyRead(QTcpSocket* socket);
	void answer(QTcpSocket* socket, const QByteArray& bytes);
	void dropConnection(QTcpSocket* socket);
	void closeAll();

	// What a request means (ModBridgeRoutes.cpp)
	QByteArray respond(const ModBridgeProtocol::Request& request);
	/// The instance whose session the request's token belongs to; counts a
	/// wrong one.
	BaseInstance* authenticate(const ModBridgeProtocol::Request& request);
	QByteArray route(BaseInstance* instance,
					 const ModBridgeProtocol::Request& request);
	QByteArray hello(BaseInstance* instance);
	QByteArray reportWorld(BaseInstance* instance,
						   const ModBridgeProtocol::Request& request);
	QByteArray join(BaseInstance* instance,
					const ModBridgeProtocol::Request& request);
	QByteArray face(const QString& uuid);
	ModBridgeJson::FriendsView friendsView() const;

	QString m_launcher;
	QTcpServer m_server;
	QHash<BaseInstance*, Session> m_sessions;
	QHash<QTcpSocket*, Connection> m_connections;
	ModBridgeProtocol::Lockout m_lockout;
	QElapsedTimer m_clock;
	int m_requestTimeoutMs = ModBridgeProtocol::kRequestTimeoutMs;
	std::shared_ptr<ModBridgeSource> m_source;
	QMetaObject::Connection m_worldRoute;
};
