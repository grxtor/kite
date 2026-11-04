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
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>

class QWebSocket;

/*
 * The friends server's WebSocket (GET /v1/ws, server/friends/README.md
 * "Realtime socket"): signed in with the session token in the upgrade's
 * Authorization header, never in the URL. Up once the server says `ready`;
 * every other frame is handed on as it is. It pings, and a socket that has
 * been silent too long is dropped. Any close but 4401 reconnects with a
 * growing, jittered delay (4001, too many sockets, waits longest); 4401 means
 * the session ended, so it stops and says so.
 */
class FriendsSocket : public QObject
{
	Q_OBJECT

  public:
	/// The server closes with these.
	static constexpr int kCloseTooManySockets = 4001;
	static constexpr int kCloseSessionEnded = 4401;

	struct Timing {
		/// Our own ping; its pong proves the line still carries.
		int pingMs = 30 * 1000;
		/// Nothing heard for this long (a ping is due every 30 s): reconnect.
		int silenceMs = 75 * 1000;
		int firstRetryMs = 1000;
		int maxRetryMs = 60 * 1000;
		/// After 4001: five other sockets of this player are open.
		int crowdedRetryMs = 5 * 60 * 1000;
		/// Up this long, a drop starts the delays from the first again.
		int stableMs = 60 * 1000;
	};

	explicit FriendsSocket(QObject* parent = nullptr);
	~FriendsSocket() override;

	void setTiming(const Timing& timing);

	/// Connects to @p url with @p token and keeps reconnecting until stop().
	void start(const QUrl& url, const QString& token);
	void stop();
	bool running() const;
	/// The token start() was given; empty while stopped.
	QString token() const;
	/// Connected and `ready` heard.
	bool isUp() const;
	/// Sends @p frame as a text frame; false while not up.
	bool send(const QJsonObject& frame);

	/// ws(s)://<server>/v1/ws for the server's http(s) URL.
	static QUrl socketUrl(const QString& serverUrl);
	/// The wait before reconnecting after @p failures drops in a row: 1 s,
	/// 2 s, 4 s... up to maxRetryMs, scaled into its upper half by @p jitter
	/// (0 to 1) so that clients dropped together do not return together.
	static int retryDelayMs(int failures, const Timing& timing, double jitter);

  signals:
	void upChanged(bool up);
	/// Every frame but `ready`, as the JSON object it is.
	void frameReceived(const QJsonObject& frame);
	/// Closed with 4401: sign in again before start().
	void sessionEnded();

  private:
	void connectNow();
	void onConnected();
	void onClosed();
	void onText(const QString& text);
	void heard();
	void onSilence();
	/// Drops the socket and connects again after @p waitMs.
	void retryIn(int waitMs);
	int nextRetryMs();
	void setUp(bool up);
	void dropSocket();

	Timing m_timing;
	QUrl m_url;
	QString m_token;
	QPointer<QWebSocket> m_socket;
	QTimer m_retryTimer;
	QTimer m_pingTimer;
	QTimer m_silenceTimer;
	QElapsedTimer m_upSince;
	bool m_running = false;
	bool m_up = false;
	int m_failures = 0;
};
