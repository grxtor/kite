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

#include "ServerPing.h"

#include <QDateTime>
#include <QDnsLookup>
#include <QHostAddress>
#include <QTcpSocket>
#include <QtEndian>

namespace
{
	constexpr int kHandshakeId = 0x00;
	constexpr int kStatusId = 0x00;
	constexpr int kPingId = 0x01;
	constexpr int kNextStateStatus = 1;
	constexpr int kMaxVarIntBytes = 5;
	constexpr int kVarIntShift = 7;
	constexpr quint8 kVarIntMask = 0x7F;
	constexpr quint8 kVarIntMore = 0x80;
	constexpr int kPortBytes = 2;
	constexpr int kPingPayloadBytes = 8;
	const QString kSrvPrefix = QStringLiteral("_minecraft._tcp.");

	enum class Read { Ok, Incomplete, Bad };

	/// The VarInt at @p at in @p data, and how many bytes it took.
	Read readVarInt(const QByteArray& data, int at, int* value, int* length)
	{
		quint32 result = 0;
		for (int i = 0; i < kMaxVarIntBytes; ++i) {
			if (at + i >= data.size()) {
				return Read::Incomplete;
			}
			const auto byte = static_cast<quint8>(data.at(at + i));
			result |= quint32(byte & kVarIntMask) << (kVarIntShift * i);
			if (!(byte & kVarIntMore)) {
				*value = static_cast<int>(result);
				*length = i + 1;
				return Read::Ok;
			}
		}
		return Read::Bad;
	}

	QByteArray string(const QString& text)
	{
		const QByteArray utf8 = text.toUtf8();
		return ServerPing::varInt(int(utf8.size())) + utf8;
	}
} // namespace

ServerPing::ServerPing(const QString& host, quint16 port, bool lookupSrv,
					   QObject* parent)
	: QObject(parent), m_host(host), m_port(port), m_lookupSrv(lookupSrv)
{
	m_timeout.setSingleShot(true);
	m_timeout.setInterval(kDefaultTimeoutMs);
	connect(&m_timeout, &QTimer::timeout, this, &ServerPing::onTimeout);
	m_srvTimeout.setSingleShot(true);
	m_srvTimeout.setInterval(kSrvTimeoutMs);
	connect(&m_srvTimeout, &QTimer::timeout, this, &ServerPing::onSrvTimeout);
	m_connectRetry.setSingleShot(true);
	m_connectRetry.setInterval(kConnectRetryMs);
	connect(&m_connectRetry, &QTimer::timeout, this, &ServerPing::onConnectRetry);
}

ServerPing::~ServerPing()
{
	m_phase = Phase::Done;
	cleanUp();
}

void ServerPing::setTimeoutMs(int timeoutMs)
{
	m_timeout.setInterval(timeoutMs);
}

QByteArray ServerPing::varInt(int value)
{
	auto rest = static_cast<quint32>(value);
	QByteArray out;
	do {
		quint8 byte = rest & kVarIntMask;
		rest >>= kVarIntShift;
		if (rest) {
			byte |= kVarIntMore;
		}
		out.append(static_cast<char>(byte));
	} while (rest);
	return out;
}

QByteArray ServerPing::packet(int id, const QByteArray& body)
{
	const QByteArray data = varInt(id) + body;
	return varInt(int(data.size())) + data;
}

QByteArray ServerPing::handshake(const QString& host, quint16 port)
{
	QByteArray portBytes(kPortBytes, Qt::Uninitialized);
	qToBigEndian(port, portBytes.data());
	return packet(kHandshakeId, varInt(kProtocolUnknown) + string(host) +
									portBytes + varInt(kNextStateStatus));
}

void ServerPing::start()
{
	if (m_phase != Phase::Idle) {
		return;
	}
	m_timeout.start();
	if (!m_lookupSrv || !QHostAddress(m_host).isNull()) {
		connectTo(m_host, m_port);
		return;
	}
	m_phase = Phase::Resolving;
	m_dns = new QDnsLookup(QDnsLookup::SRV, kSrvPrefix + m_host, this);
	connect(m_dns, &QDnsLookup::finished, this, &ServerPing::onSrvFinished);
	m_srvTimeout.start();
	m_dns->lookup();
}

void ServerPing::onSrvTimeout()
{
	if (m_phase != Phase::Resolving) {
		return;
	}
	m_dns->disconnect(this);
	m_dns->abort();
	m_dns->deleteLater();
	m_dns = nullptr;
	connectTo(m_host, m_port);
}

void ServerPing::onSrvFinished()
{
	if (m_phase != Phase::Resolving) {
		return;
	}
	m_srvTimeout.stop();
	QString host = m_host;
	quint16 port = m_port;
	const auto records = m_dns->serviceRecords();
	if (m_dns->error() == QDnsLookup::NoError && !records.isEmpty()) {
		QString target = records.first().target();
		if (target.endsWith(QLatin1Char('.'))) {
			target.chop(1);
		}
		if (!target.isEmpty()) {
			host = target;
			port = records.first().port();
		}
	}
	connectTo(host, port);
}

void ServerPing::connectTo(const QString& host, quint16 port)
{
	m_phase = Phase::Connecting;
	m_targetHost = host;
	m_targetPort = port;
	if (!m_retried) {
		m_connectRetry.start();
	}
	m_socket = new QTcpSocket(this);
	connect(m_socket, &QTcpSocket::connected, this, &ServerPing::onConnected);
	connect(m_socket, &QTcpSocket::readyRead, this, &ServerPing::onReadyRead);
	connect(m_socket, &QTcpSocket::disconnected, this, &ServerPing::onClosed);
	connect(m_socket, &QTcpSocket::errorOccurred, this, &ServerPing::onClosed);
	m_socket->connectToHost(host, port);
}

void ServerPing::onConnectRetry()
{
	if (m_phase != Phase::Connecting || m_retried) {
		return;
	}
	m_retried = true;
	dropSocket();
	connectTo(m_targetHost, m_targetPort);
}

void ServerPing::onConnected()
{
	m_connectRetry.stop();
	m_phase = Phase::Status;
	m_socket->write(handshake(m_host, m_port) + packet(kStatusId, {}));
	m_clock.start();
}

void ServerPing::onReadyRead()
{
	m_buffer += m_socket->readAll();
	int id = 0;
	QByteArray body;
	while ((m_phase == Phase::Status || m_phase == Phase::Pong) &&
		   takePacket(&id, &body)) {
		if (m_phase == Phase::Status) {
			if (id != kStatusId) {
				fail(QStringLiteral("The server answered with something else."));
				return;
			}
			readStatus(body);
		} else {
			if (id == kPingId && body.size() == kPingPayloadBytes) {
				m_status.latencyMs = int(m_clock.elapsed());
			}
			succeed();
			return;
		}
	}
}

bool ServerPing::takePacket(int* id, QByteArray* body)
{
	int length = 0;
	int lengthBytes = 0;
	const Read read = readVarInt(m_buffer, 0, &length, &lengthBytes);
	if (read == Read::Incomplete) {
		return false;
	}
	if (read == Read::Bad || length <= 0) {
		fail(QStringLiteral("The server's answer is malformed."));
		return false;
	}
	if (length > kMaxPacketBytes) {
		fail(QStringLiteral("The server's answer is too long."));
		return false;
	}
	if (m_buffer.size() < lengthBytes + length) {
		return false;
	}
	const QByteArray data = m_buffer.mid(lengthBytes, length);
	m_buffer.remove(0, lengthBytes + length);
	int idBytes = 0;
	if (readVarInt(data, 0, id, &idBytes) != Read::Ok) {
		fail(QStringLiteral("The server's answer is malformed."));
		return false;
	}
	*body = data.mid(idBytes);
	return true;
}

void ServerPing::readStatus(const QByteArray& body)
{
	int length = 0;
	int lengthBytes = 0;
	if (readVarInt(body, 0, &length, &lengthBytes) != Read::Ok || length < 0 ||
		lengthBytes + length > body.size()) {
		fail(QStringLiteral("The server's answer is malformed."));
		return;
	}
	QString error;
	if (!ServerStatus::parse(body.mid(lengthBytes, length), &m_status, &error)) {
		fail(error);
		return;
	}
	m_status.latencyMs = int(m_clock.elapsed());
	m_phase = Phase::Pong;
	QByteArray payload(kPingPayloadBytes, Qt::Uninitialized);
	qToBigEndian(QDateTime::currentMSecsSinceEpoch(), payload.data());
	m_clock.restart();
	m_socket->write(packet(kPingId, payload));
}

void ServerPing::onClosed()
{
	if (m_phase == Phase::Pong) {
		succeed();
	} else if (m_phase != Phase::Done) {
		fail(m_socket ? m_socket->errorString()
					  : QStringLiteral("The connection closed."));
	}
}

void ServerPing::onTimeout()
{
	/* A busy event loop can let the timer in before an answer that has
	 * already come. */
	if (m_socket && m_socket->bytesAvailable() > 0) {
		onReadyRead();
		if (m_phase == Phase::Done) {
			return;
		}
	}
	if (m_phase == Phase::Pong) {
		succeed();
	} else {
		fail(QStringLiteral("The server did not answer in time."));
	}
}

void ServerPing::succeed()
{
	if (m_phase == Phase::Done) {
		return;
	}
	m_phase = Phase::Done;
	cleanUp();
	emit succeeded(m_status);
}

void ServerPing::fail(const QString& reason)
{
	if (m_phase == Phase::Done) {
		return;
	}
	m_phase = Phase::Done;
	cleanUp();
	emit failed(reason);
}

void ServerPing::dropSocket()
{
	if (m_socket) {
		m_socket->disconnect(this);
		m_socket->abort();
		m_socket->deleteLater();
		m_socket = nullptr;
	}
}

void ServerPing::cleanUp()
{
	m_timeout.stop();
	m_srvTimeout.stop();
	m_connectRetry.stop();
	dropSocket();
	if (m_dns) {
		m_dns->disconnect(this);
		m_dns->abort();
		m_dns->deleteLater();
		m_dns = nullptr;
	}
}
