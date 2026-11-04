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

#include "FriendsSocket.h"

#include <QDebug>
#include <QJsonDocument>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QWebSocket>

#include <algorithm>

#include "BuildConfig.h"
#include "FriendsApi.h"
#include "FriendsChatApi.h"

namespace
{
	/* 2^this times the first delay is past any sane cap; stops the shift there. */
	constexpr int kMaxDoublings = 16;
	/* The jitter takes up to this share off a delay. */
	constexpr double kJitterShare = 0.5;
	const QString kReady = QStringLiteral("ready");
} // namespace

FriendsSocket::FriendsSocket(QObject* parent) : QObject(parent)
{
	m_retryTimer.setSingleShot(true);
	connect(&m_retryTimer, &QTimer::timeout, this, &FriendsSocket::connectNow);
	connect(&m_pingTimer, &QTimer::timeout, this, [this]() {
		if (m_socket) {
			m_socket->ping();
		}
	});
	m_silenceTimer.setSingleShot(true);
	connect(&m_silenceTimer, &QTimer::timeout, this, &FriendsSocket::onSilence);
}

FriendsSocket::~FriendsSocket()
{
	dropSocket();
}

void FriendsSocket::setTiming(const Timing& timing)
{
	m_timing = timing;
}

QUrl FriendsSocket::socketUrl(const QString& serverUrl)
{
	QUrl url = FriendsApi::url(serverUrl, FriendsChatApi::Path::socket());
	url.setScheme(url.scheme() == QLatin1String("http") ? QStringLiteral("ws")
														: QStringLiteral("wss"));
	return url;
}

int FriendsSocket::retryDelayMs(int failures, const Timing& timing,
								double jitter)
{
	const int doublings = std::clamp(failures, 0, kMaxDoublings);
	const qint64 grown = static_cast<qint64>(timing.firstRetryMs) << doublings;
	const qint64 capped = std::min<qint64>(grown, timing.maxRetryMs);
	const double off = kJitterShare * std::clamp(jitter, 0.0, 1.0);
	return static_cast<int>(capped * (1.0 - off));
}

void FriendsSocket::start(const QUrl& url, const QString& token)
{
	if (m_running && url == m_url && token == m_token) {
		return;
	}
	stop();
	m_url = url;
	m_token = token;
	m_running = true;
	m_failures = 0;
	connectNow();
}

void FriendsSocket::stop()
{
	m_running = false;
	m_token.clear();
	m_retryTimer.stop();
	dropSocket();
	setUp(false);
}

bool FriendsSocket::running() const
{
	return m_running;
}

QString FriendsSocket::token() const
{
	return m_token;
}

bool FriendsSocket::isUp() const
{
	return m_up;
}

bool FriendsSocket::send(const QJsonObject& frame)
{
	if (!m_up || !m_socket) {
		return false;
	}
	const QString text = QString::fromUtf8(
		QJsonDocument(frame).toJson(QJsonDocument::Compact));
	return m_socket->sendTextMessage(text) > 0;
}

void FriendsSocket::connectNow()
{
	if (!m_running) {
		return;
	}
	dropSocket();
	auto* socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
	m_socket = socket;
	connect(socket, &QWebSocket::connected, this, &FriendsSocket::onConnected);
	connect(socket, &QWebSocket::disconnected, this, &FriendsSocket::onClosed);
	connect(socket, &QWebSocket::textMessageReceived, this,
			&FriendsSocket::onText);
	connect(socket, &QWebSocket::pong, this, &FriendsSocket::heard);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
	const auto failed = &QWebSocket::errorOccurred;
#else
	const auto failed = qOverload<QAbstractSocket::SocketError>(&QWebSocket::error);
#endif
	connect(socket, failed, this, [this, socket]() {
		/* Before the upgrade (refused, 401, TLS) no disconnected() may follow. */
		if (socket == m_socket && socket->state() != QAbstractSocket::ConnectedState) {
			qDebug() << "Friends socket: failed," << socket->errorString();
			retryIn(nextRetryMs());
		}
	});
	QNetworkRequest request(m_url);
	request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
	request.setRawHeader("User-Agent", BuildConfig.USER_AGENT.toUtf8());
	/* A handshake that hangs counts as silence too. */
	m_silenceTimer.start(m_timing.silenceMs);
	socket->open(request);
}

void FriendsSocket::onConnected()
{
	heard();
	m_pingTimer.start(m_timing.pingMs);
}

void FriendsSocket::onClosed()
{
	auto* socket = qobject_cast<QWebSocket*>(sender());
	if (!socket || socket != m_socket) {
		return;
	}
	const int code = socket->closeCode();
	qDebug() << "Friends socket: closed with" << code;
	if (code == kCloseSessionEnded) {
		stop();
		emit sessionEnded();
		return;
	}
	if (code == kCloseTooManySockets) {
		retryIn(m_timing.crowdedRetryMs);
		return;
	}
	retryIn(nextRetryMs());
}

void FriendsSocket::onText(const QString& text)
{
	heard();
	QJsonParseError error{};
	const auto doc = QJsonDocument::fromJson(text.toUtf8(), &error);
	if (error.error != QJsonParseError::NoError || !doc.isObject()) {
		return;
	}
	const QJsonObject frame = doc.object();
	if (frame.value("type").toString() == kReady) {
		m_upSince.start();
		setUp(true);
		return;
	}
	emit frameReceived(frame);
}

void FriendsSocket::heard()
{
	if (m_socket) {
		m_silenceTimer.start(m_timing.silenceMs);
	}
}

void FriendsSocket::onSilence()
{
	qDebug() << "Friends socket: silent for" << m_timing.silenceMs << "ms";
	retryIn(nextRetryMs());
}

int FriendsSocket::nextRetryMs()
{
	/* A socket that stayed up a while is a fresh start; one that keeps
	 * dropping right after `ready` still backs off. */
	if (m_up && m_upSince.isValid() && m_upSince.elapsed() >= m_timing.stableMs) {
		m_failures = 0;
	}
	const double jitter = QRandomGenerator::global()->generateDouble();
	return retryDelayMs(m_failures++, m_timing, jitter);
}

void FriendsSocket::retryIn(int waitMs)
{
	dropSocket();
	setUp(false);
	if (m_running) {
		m_retryTimer.start(waitMs);
	}
}

void FriendsSocket::setUp(bool up)
{
	if (up == m_up) {
		return;
	}
	m_up = up;
	emit upChanged(up);
}

void FriendsSocket::dropSocket()
{
	m_pingTimer.stop();
	m_silenceTimer.stop();
	if (!m_socket) {
		return;
	}
	QWebSocket* socket = m_socket;
	m_socket = nullptr;
	/* Its last signals (the close below) are not ours any more. */
	socket->disconnect(this);
	socket->abort();
	socket->deleteLater();
}
