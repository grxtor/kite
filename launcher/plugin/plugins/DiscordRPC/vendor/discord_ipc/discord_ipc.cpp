/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 Project Tick
 *
 * Minimal Qt-only Discord IPC client — see discord_ipc.h.
 */

#include "discord_ipc.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QtEndian>
#include <QUuid>

DiscordIpc::DiscordIpc(QObject* parent) : QObject(parent)
{
	m_reconnectTimer.setSingleShot(true);
	m_reconnectTimer.setInterval(5000);
	connect(&m_reconnectTimer, &QTimer::timeout, this,
			&DiscordIpc::onReconnectTimeout);
}

DiscordIpc::~DiscordIpc()
{
	disconnectFromDiscord();
}

void DiscordIpc::setClientId(const QString& clientId)
{
	if (m_clientId == clientId)
		return;
	m_clientId = clientId;
	if (m_state == State::Handshaking || m_state == State::Ready) {
		disconnectFromDiscord();
		connectToDiscord();
	}
}

void DiscordIpc::setState(State s)
{
	if (m_state == s)
		return;
	m_state = s;
	emit stateChanged(s);
}

/* ── pipe path resolution ─────────────────────────────────────────── */

QString DiscordIpc::candidatePipePath(int index) const
{
#ifdef Q_OS_WIN
	/* Windows named pipes — QLocalSocket strips the \\.\pipe\ prefix. */
	return QStringLiteral("discord-ipc-%1").arg(index);
#else
	/* On Linux/macOS Discord places the socket under one of several runtime dirs, in priority:
	 * $XDG_RUNTIME_DIR, $TMPDIR, /tmp, $TMP / $TEMP (rare), and also nested under .flatpak-info,
	 * app/com.discordapp.Discord and snap.discord; walk those too. */
	QStringList bases;
	auto envBase = [&bases](const char* var) {
		QByteArray v = qgetenv(var);
		if (!v.isEmpty())
			bases << QString::fromLocal8Bit(v);
	};
	envBase("XDG_RUNTIME_DIR");
	envBase("TMPDIR");
	envBase("TMP");
	envBase("TEMP");
	bases << QStringLiteral("/tmp");

	QStringList suffixes;
	suffixes << QString() << QStringLiteral("app/com.discordapp.Discord/")
			 << QStringLiteral("snap.discord/")
			 << QStringLiteral("snap.discord-canary/");

	for (const QString& base : bases) {
		for (const QString& suf : suffixes) {
			QString path =
				QStringLiteral("%1/%2discord-ipc-%3").arg(base, suf).arg(index);
			QFileInfo fi(path);
			if (fi.exists())
				return path;
		}
	}
	/* Fallback: the canonical /run-style path even if it does not exist yet; Discord may create it
	 * later. */
	const QString base =
		bases.isEmpty() ? QStringLiteral("/tmp") : bases.first();
	return QStringLiteral("%1/discord-ipc-%2").arg(base).arg(index);
#endif
}

/* ── connection lifecycle ─────────────────────────────────────────── */

void DiscordIpc::connectToDiscord()
{
	if (m_clientId.isEmpty())
		return;
	if (m_state != State::Disconnected)
		return;

	m_pipeIndex = 0;
	tryNextPipe();
}

void DiscordIpc::tryNextPipe()
{
	if (m_socket) {
		m_socket->disconnect(this);
		m_socket->abort();
		m_socket->deleteLater();
		m_socket.clear();
	}

	if (m_pipeIndex > 9) {
		/* Walked the whole pipe range without a server: Discord is offline or not installed. Quiet
		 * after the first announcement; the reconnect timer retries. */
		if (!m_errorEmitted) {
			emit errorOccurred(QStringLiteral("Discord IPC: no socket in 0..9 "
											  "(is Discord running?)"));
			m_errorEmitted = true;
		}
		setState(State::Disconnected);
		m_reconnectTimer.start();
		return;
	}

	/* Skip pipe indices whose socket file does not exist: QLocalSocket otherwise fails async with a
	 * confusing "Invalid name" warning, and skipping also speeds up the walk. */
#ifndef Q_OS_WIN
	{
		const QString path = candidatePipePath(m_pipeIndex);
		QFileInfo fi(path);
		if (!fi.exists()) {
			++m_pipeIndex;
			QMetaObject::invokeMethod(this, &DiscordIpc::tryNextPipe,
									  Qt::QueuedConnection);
			return;
		}
	}
#endif

	setState(State::Connecting);
	m_socket = new QLocalSocket(this);
	connect(m_socket, &QLocalSocket::connected, this, &DiscordIpc::onConnected);
	connect(m_socket, &QLocalSocket::disconnected, this,
			&DiscordIpc::onDisconnected);
	connect(m_socket, &QLocalSocket::readyRead, this, &DiscordIpc::onReadyRead);
	connect(m_socket, &QLocalSocket::errorOccurred, this, &DiscordIpc::onError);

	/* setServerName() + connectToServer(OpenMode), not the combined overload: that one runs Qt's
	 * name mangling, which rejects absolute UNIX paths containing "/" with an "Invalid name"
	 * warning on some Qt 6 builds. */
	const QString path = candidatePipePath(m_pipeIndex);
	m_socket->setServerName(path);
	m_socket->connectToServer(QIODevice::ReadWrite);
}

void DiscordIpc::disconnectFromDiscord()
{
	m_reconnectTimer.stop();
	if (m_socket) {
		m_socket->disconnect(this);
		m_socket->abort();
		m_socket->deleteLater();
		m_socket.clear();
	}
	m_readBuffer.clear();
	setState(State::Disconnected);
}

void DiscordIpc::onConnected()
{
	/* Fresh connection: reset the outage-suppression flag so the next outage emits one error. */
	m_errorEmitted = false;
	setState(State::Handshaking);
	sendHandshake();
}

void DiscordIpc::onDisconnected()
{
	m_readBuffer.clear();
	setState(State::Disconnected);
	/* Auto-reconnect after a delay; Discord may just have been (re)started. */
	m_reconnectTimer.start();
}

void DiscordIpc::onError(QLocalSocket::LocalSocketError /*err*/)
{
	if (m_state == State::Connecting && m_pipeIndex < 9) {
		++m_pipeIndex;
		tryNextPipe();
		return;
	}
	if (m_socket && !m_errorEmitted) {
		emit errorOccurred(m_socket->errorString());
		m_errorEmitted = true;
	}
	setState(State::Disconnected);
	m_reconnectTimer.start();
}

void DiscordIpc::onReconnectTimeout()
{
	if (m_state == State::Disconnected)
		connectToDiscord();
}

/* ── framing ──────────────────────────────────────────────────────── */

void DiscordIpc::writeFrame(Opcode op, const QByteArray& payload)
{
	if (!m_socket || m_socket->state() != QLocalSocket::ConnectedState)
		return;
	quint32 opLE = qToLittleEndian<quint32>(static_cast<quint32>(op));
	quint32 lenLE =
		qToLittleEndian<quint32>(static_cast<quint32>(payload.size()));
	m_socket->write(reinterpret_cast<const char*>(&opLE), sizeof(opLE));
	m_socket->write(reinterpret_cast<const char*>(&lenLE), sizeof(lenLE));
	m_socket->write(payload);
}

bool DiscordIpc::readFrame(quint32& op, QByteArray& payload)
{
	if (m_readBuffer.size() < 8)
		return false;
	const uchar* p = reinterpret_cast<const uchar*>(m_readBuffer.constData());
	quint32 opLE = qFromLittleEndian<quint32>(p);
	quint32 lenLE = qFromLittleEndian<quint32>(p + 4);
	const int total = 8 + static_cast<int>(lenLE);
	if (m_readBuffer.size() < total)
		return false;
	op = opLE;
	payload = m_readBuffer.mid(8, static_cast<int>(lenLE));
	m_readBuffer.remove(0, total);
	return true;
}

void DiscordIpc::onReadyRead()
{
	if (!m_socket)
		return;
	m_readBuffer.append(m_socket->readAll());

	quint32 op = 0;
	QByteArray payload;
	while (readFrame(op, payload)) {
		switch (op) {
			case OpFrame: {
				/* Look for the READY event: the handshake completed and commands may be sent. */
				QJsonDocument doc = QJsonDocument::fromJson(payload);
				QJsonObject obj = doc.object();
				QString evt = obj.value("evt").toString();
				if (evt == QStringLiteral("READY")) {
					setState(State::Ready);
					if (m_haveActivity)
						sendCurrentActivity();
				}
				break;
			}
			case OpPing:
				writeFrame(OpPong, payload);
				break;
			case OpClose:
				disconnectFromDiscord();
				return;
			default:
				/* OpHandshake, OpPong — nothing to do */
				break;
		}
	}
}

/* ── handshake & SET_ACTIVITY ─────────────────────────────────────── */

void DiscordIpc::sendHandshake()
{
	QJsonObject hello;
	hello[QStringLiteral("v")] = 1;
	hello[QStringLiteral("client_id")] = m_clientId;
	writeFrame(OpHandshake,
			   QJsonDocument(hello).toJson(QJsonDocument::Compact));
}

void DiscordIpc::setActivity(const DiscordActivity& a)
{
	m_pendingActivity = a;
	m_haveActivity = true;
	if (m_state == State::Ready)
		sendCurrentActivity();
}

void DiscordIpc::clearActivity()
{
	m_haveActivity = false;
	if (m_state != State::Ready)
		return;

	QJsonObject args;
	args[QStringLiteral("pid")] =
		static_cast<int>(QCoreApplication::applicationPid());
	/* "activity": null → clears */
	args[QStringLiteral("activity")] = QJsonValue();

	QJsonObject msg;
	msg[QStringLiteral("cmd")] = QStringLiteral("SET_ACTIVITY");
	msg[QStringLiteral("args")] = args;
	msg[QStringLiteral("nonce")] =
		QString::number(++m_nonce) + QStringLiteral("-clear");
	writeFrame(OpFrame, QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void DiscordIpc::sendCurrentActivity()
{
	QJsonObject activity;
	if (!m_pendingActivity.details.isEmpty())
		activity[QStringLiteral("details")] = m_pendingActivity.details;
	if (!m_pendingActivity.state.isEmpty())
		activity[QStringLiteral("state")] = m_pendingActivity.state;
	if (m_pendingActivity.startTimestamp > 0) {
		QJsonObject ts;
		ts[QStringLiteral("start")] =
			static_cast<qint64>(m_pendingActivity.startTimestamp);
		activity[QStringLiteral("timestamps")] = ts;
	}
	if (!m_pendingActivity.largeImageKey.isEmpty() ||
		!m_pendingActivity.smallImageKey.isEmpty()) {
		QJsonObject assets;
		if (!m_pendingActivity.largeImageKey.isEmpty())
			assets[QStringLiteral("large_image")] =
				m_pendingActivity.largeImageKey;
		if (!m_pendingActivity.largeImageText.isEmpty())
			assets[QStringLiteral("large_text")] =
				m_pendingActivity.largeImageText;
		if (!m_pendingActivity.smallImageKey.isEmpty())
			assets[QStringLiteral("small_image")] =
				m_pendingActivity.smallImageKey;
		if (!m_pendingActivity.smallImageText.isEmpty())
			assets[QStringLiteral("small_text")] =
				m_pendingActivity.smallImageText;
		activity[QStringLiteral("assets")] = assets;
	}

	QJsonObject args;
	args[QStringLiteral("pid")] =
		static_cast<int>(QCoreApplication::applicationPid());
	args[QStringLiteral("activity")] = activity;

	QJsonObject msg;
	msg[QStringLiteral("cmd")] = QStringLiteral("SET_ACTIVITY");
	msg[QStringLiteral("args")] = args;
	msg[QStringLiteral("nonce")] = QString::number(++m_nonce);

	writeFrame(OpFrame, QJsonDocument(msg).toJson(QJsonDocument::Compact));
}
