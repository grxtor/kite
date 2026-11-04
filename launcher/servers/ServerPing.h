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

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include "servers/ServerStatus.h"

class QDnsLookup;
class QTcpSocket;

/* One Server List Ping (Minecraft 1.7+): an optional SRV lookup of
 * _minecraft._tcp.<host>, a handshake, the status request and a ping for the
 * latency. Ends in exactly one of succeeded() or failed(), within the timeout;
 * a server that answers the status but not the ping still succeeds, timed by
 * the status' round trip. */
class ServerPing : public QObject
{
	Q_OBJECT

  public:
	/// What the handshake says about the client: -1, by convention, when it
	/// only wants to know the server's version.
	static constexpr int kProtocolUnknown = -1;
	/// A vanilla client reads at most 32767 characters of status; allow for
	/// three bytes each and the packet's own fields.
	static constexpr int kMaxPacketBytes = 32767 * 3 + 16;
	static constexpr int kDefaultTimeoutMs = 5000;
	/// A lost DNS packet costs the resolver its 5 s retry; past this the
	/// ping goes to the host as written, as the game does without SRV.
	static constexpr int kSrvTimeoutMs = 1500;
	/// A connection still not made by then starts over once, from a new
	/// port: a dropped SYN is retried by the system on the same one.
	static constexpr int kConnectRetryMs = 2000;

	/// @p host and @p port as the player wrote them (the handshake carries
	/// them, which name-based proxies route by); @p lookupSrv first asks
	/// DNS where the server really is.
	ServerPing(const QString& host, quint16 port, bool lookupSrv,
			   QObject* parent = nullptr);
	~ServerPing() override;

	void setTimeoutMs(int timeoutMs);
	void start();

	/// Minecraft's VarInt, for the packets and the tests' fake servers.
	static QByteArray varInt(int value);
	/// A packet: its length, then @p id and @p body.
	static QByteArray packet(int id, const QByteArray& body);
	/// The handshake (next state: status) for @p host and @p port.
	static QByteArray handshake(const QString& host, quint16 port);

  signals:
	void succeeded(const ServerStatus::Status& status);
	void failed(const QString& reason);

  private:
	enum class Phase { Idle, Resolving, Connecting, Status, Pong, Done };

	void connectTo(const QString& host, quint16 port);
	void onSrvFinished();
	void onSrvTimeout();
	void onConnectRetry();
	void dropSocket();
	void onConnected();
	void onReadyRead();
	void onClosed();
	void onTimeout();
	/// The next whole packet in m_buffer: true and its id and body, false if
	/// more bytes are needed. A bad or oversized one fails the ping.
	bool takePacket(int* id, QByteArray* body);
	void readStatus(const QByteArray& body);
	void succeed();
	void fail(const QString& reason);
	void cleanUp();

	QString m_host;
	quint16 m_port;
	bool m_lookupSrv;
	Phase m_phase = Phase::Idle;
	QDnsLookup* m_dns = nullptr;
	QTcpSocket* m_socket = nullptr;
	QTimer m_timeout;
	QTimer m_srvTimeout;
	QTimer m_connectRetry;
	bool m_retried = false;
	/// Where connectTo() went: the SRV target, else the host as written.
	QString m_targetHost;
	quint16 m_targetPort = 0;
	QElapsedTimer m_clock;
	QByteArray m_buffer;
	ServerStatus::Status m_status;
};
