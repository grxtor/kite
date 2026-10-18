/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 Project Tick
 *
 * Minimal Qt-only Discord IPC client.
 *
 * Implements the wire side of Discord's local RPC socket
 * (https://discord.com/developers/docs/topics/rpc) — handshake,
 * heartbeat-free framed JSON messages, and the SET_ACTIVITY command.
 *
 * The class hides the platform difference between UNIX domain sockets
 * (Linux/macOS, ~/.config/discord-ipc-N) and Windows named pipes
 * (\\.\pipe\discord-ipc-N) behind QLocalSocket.
 *
 * Usage:
 *   DiscordIpc ipc;
 *   ipc.setClientId("1234567890");
 *   ipc.connectToDiscord();              // non-blocking
 *   ipc.setActivity({ "Playing", "Minecraft 1.21" });
 *   ipc.clearActivity();
 *   ipc.disconnectFromDiscord();
 *
 * The class is a QObject so callers can hook into stateChanged() /
 * errorOccurred() signals if they need richer diagnostics.
 */

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QLocalSocket>
#include <cstdint>

class DiscordActivity
{
  public:
	QString state;	 /* second line, e.g. "Playing on 1.21" */
	QString details; /* first line,  e.g. "Kite" */
	QString largeImageKey;
	QString largeImageText;
	QString smallImageKey;
	QString smallImageText;
	qint64 startTimestamp = 0; /* Unix seconds; 0 = unset */
};

class DiscordIpc : public QObject
{
	Q_OBJECT
  public:
	enum class State { Disconnected, Connecting, Handshaking, Ready };
	Q_ENUM(State)

	explicit DiscordIpc(QObject* parent = nullptr);
	~DiscordIpc() override;

	void setClientId(const QString& clientId);
	QString clientId() const
	{
		return m_clientId;
	}
	State state() const
	{
		return m_state;
	}

	/* Begin a non-blocking connection attempt. Safe to call repeatedly;
	 * a no-op if we are already connecting or connected. */
	void connectToDiscord();
	void disconnectFromDiscord();

	void setActivity(const DiscordActivity& activity);
	void clearActivity();

  signals:
	void stateChanged(State newState);
	void errorOccurred(const QString& message);

  private slots:
	void onConnected();
	void onDisconnected();
	void onReadyRead();
	void onError(QLocalSocket::LocalSocketError err);
	void tryNextPipe();
	void onReconnectTimeout();

  private:
	enum Opcode : quint32 {
		OpHandshake = 0,
		OpFrame = 1,
		OpClose = 2,
		OpPing = 3,
		OpPong = 4
	};

	void setState(State s);
	void writeFrame(Opcode op, const QByteArray& payload);
	void sendHandshake();
	void sendCurrentActivity();
	bool readFrame(quint32& op, QByteArray& payload);
	QString candidatePipePath(int index) const;

	QString m_clientId;
	State m_state = State::Disconnected;
	QPointer<QLocalSocket> m_socket;
	QByteArray m_readBuffer;
	int m_pipeIndex = 0; /* 0..9, walked during connect */
	QTimer m_reconnectTimer;

	bool m_haveActivity = false;
	DiscordActivity m_pendingActivity;
	int m_nonce = 0;

	/* Suppress duplicate error spam: when Discord is not running we
	 * cycle through pipe 0..9 every 5 seconds. Without this flag the
	 * log fills with "Geçersiz ad" / "ServerNotFoundError" lines
	 * forever. We emit at most one error per outage window — reset on
	 * a successful onConnected(). */
	bool m_errorEmitted = false;
};
