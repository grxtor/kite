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

#include "ModBridge.h"

#include <QDebug>
#include <QHostAddress>
#include <QTcpSocket>
#include <QTimer>

#include "BaseInstance.h"
#include "modbridge/FriendsBridgeSource.h"
#include "friends/FriendsController.h"

using namespace ModBridgeProtocol;

ModBridge::ModBridge(QString launcher, QObject* parent)
	: QObject(parent), m_launcher(std::move(launcher)), m_server(this)
{
	m_clock.start();
	connect(&m_server, &QTcpServer::newConnection, this,
			&ModBridge::onNewConnection);
}

ModBridge::~ModBridge()
{
	closeAll();
}

bool ModBridge::listening() const
{
	return m_server.isListening();
}

quint16 ModBridge::port() const
{
	return m_server.isListening() ? m_server.serverPort() : 0;
}

int ModBridge::sessionCount() const
{
	return static_cast<int>(m_sessions.size());
}

int ModBridge::connectionCount() const
{
	return static_cast<int>(m_connections.size());
}

void ModBridge::setRequestTimeoutMs(int milliseconds)
{
	m_requestTimeoutMs = milliseconds;
}

void ModBridge::setFriends(FriendsController* controller)
{
	setSource(std::make_shared<FriendsBridgeSource>(controller));
	if (controller) {
		/* A world the game reports goes to the controller's watcher. The
		 * context is the controller: it ends this connection with it. */
		m_worldRoute = connect(
			this, &ModBridge::worldReported, controller,
			[controller](BaseInstance* instance, FriendsApi::World world) {
				controller->reportWorld(instance, world);
			});
	}
}

void ModBridge::setSource(std::shared_ptr<ModBridgeSource> source)
{
	disconnect(m_worldRoute);
	m_source = std::move(source);
}

ModBridge::Grant ModBridge::registerSession(BaseInstance* instance)
{
	if (!instance) {
		return {};
	}
	if (!m_server.isListening() &&
		!m_server.listen(QHostAddress::LocalHost, 0)) {
		qWarning() << "Kite bridge: cannot listen:" << m_server.errorString();
		return {};
	}
	/* A new launch of the same game: the old token goes, the socket stays. */
	forgetSession(instance);
	Session session;
	session.token = newToken().toLatin1();
	session.onStopped = connect(instance, &BaseInstance::runningStatusChanged,
								this, [this, instance](bool running) {
									if (!running) {
										unregisterSession(instance);
									}
								});
	session.onDestroyed = connect(instance, &QObject::destroyed, this,
								  [this, instance]() {
									  unregisterSession(instance);
								  });
	Grant grant{m_server.serverPort(), QString::fromLatin1(session.token)};
	m_sessions.insert(instance, std::move(session));
	return grant;
}

bool ModBridge::forgetSession(BaseInstance* instance)
{
	auto it = m_sessions.find(instance);
	if (it == m_sessions.end()) {
		return false;
	}
	disconnect(it->onStopped);
	disconnect(it->onDestroyed);
	m_sessions.erase(it);
	return true;
}

void ModBridge::unregisterSession(BaseInstance* instance)
{
	if (forgetSession(instance) && m_sessions.isEmpty()) {
		closeAll();
	}
}

void ModBridge::closeAll()
{
	m_server.close();
	for (QTcpSocket* socket : m_connections.keys()) {
		socket->abort();
	}
}

void ModBridge::onNewConnection()
{
	while (QTcpSocket* socket = m_server.nextPendingConnection()) {
		if (m_connections.size() >= kMaxConnections) {
			socket->abort();
			socket->deleteLater();
			continue;
		}
		m_connections.insert(socket, Connection());
		/* A request that is not whole in time is dropped; the timer dies
		 * with the socket. */
		auto* timer = new QTimer(socket);
		timer->setSingleShot(true);
		connect(timer, &QTimer::timeout, socket, &QTcpSocket::abort);
		timer->start(m_requestTimeoutMs);
		connect(socket, &QTcpSocket::readyRead, this,
				[this, socket]() { onReadyRead(socket); });
		connect(socket, &QTcpSocket::disconnected, this,
				[this, socket]() { dropConnection(socket); });
	}
}

void ModBridge::onReadyRead(QTcpSocket* socket)
{
	auto it = m_connections.find(socket);
	if (it == m_connections.end() || it->answered) {
		socket->readAll();
		return;
	}
	it->buffer += socket->readAll();
	const Parsed parsed = parseRequest(it->buffer);
	switch (parsed.state) {
		case Parsed::State::NeedMore:
			return;
		case Parsed::State::Rejected:
			answer(socket, errorResponse(parsed.status, "bad_request"));
			return;
		case Parsed::State::Complete:
			break;
	}
	const QByteArray bytes = respond(parsed.request);
	answer(socket, bytes);
}

void ModBridge::answer(QTcpSocket* socket, const QByteArray& bytes)
{
	/* respond() may have run a slot that closed everything. */
	auto it = m_connections.find(socket);
	if (it == m_connections.end() || it->answered) {
		return;
	}
	it->answered = true;
	socket->write(bytes);
	socket->disconnectFromHost();
}

void ModBridge::dropConnection(QTcpSocket* socket)
{
	m_connections.remove(socket);
	socket->deleteLater();
}
