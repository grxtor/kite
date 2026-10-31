/* SPDX-License-Identifier: MIT
 * Copyright (C) 2026 Project Tick
 *
 * Minimal Qt-only Discord IPC client for Discord's local RPC socket
 * (https://discord.com/developers/docs/topics/rpc): handshake, heartbeat-free framed JSON messages and
 * the SET_ACTIVITY command. QLocalSocket hides the difference between UNIX domain sockets
 * (Linux/macOS, ~/.config/discord-ipc-N) and Windows named pipes (discord-ipc-N).
 *
 * Usage:
 *   DiscordIpc ipc;
 *   ipc.setClientId("1234567890");
 *   ipc.connectToDiscord();              // non-blocking
 *   ipc.setActivity({ "Playing", "Minecraft 1.21" });
 *   ipc.clearActivity();
 *   ipc.disconnectFromDiscord();
 *
 * Connect to stateChanged() / errorOccurred() for diagnostics.
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

	/* Begins a non-blocking connection attempt; a no-op if already connecting or connected. */
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

	/* Suppresses repeated errors: with Discord not running we cycle pipe 0..9 every 5 seconds,
	 * which would fill the log forever. At most one error per outage, reset by a successful
	 * onConnected(). */
	bool m_errorEmitted = false;
};
