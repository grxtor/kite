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

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <functional>

class FriendsHttp;

/*
 * Sends crash reports the player chose to send to Kite's server
 * (FriendsServerUrl + /v1/crash-reports, no sign-in, nothing that leads back to
 * the player), and keeps what could not go yet in <data>/crash-reports/
 * outbox.json to try again later: at the next start and every half hour, a few
 * times, for a week at most. A signature sent once is not sent again for a
 * month. Only what the player asked for (or allowed with "send without asking")
 * ever comes here.
 */
class CrashOutbox : public QObject
{
	Q_OBJECT

  public:
	enum class State { None, Sending, Queued, Sent, Failed };
	/// What to do after an answer: done, keep it for later, or give up.
	enum class Outcome { Sent, Retry, Drop };

	/// @p dir holds outbox.json; @p serverUrl gives the friends server's base
	/// URL at the time of sending ("" means none: the report waits).
	CrashOutbox(const QString& dir, std::function<QString()> serverUrl,
				QObject* parent = nullptr);
	~CrashOutbox() override;

	/// Sends @p payload now, or keeps it to send later; nothing when its
	/// signature was sent already.
	void send(const QJsonObject& payload);
	/// Sends what is waiting.
	void retry();
	State state(const QString& signature) const;
	int waiting() const
	{
		return int(m_queue.size());
	}

	static Outcome outcome(int status);

	inline static constexpr int kMaxQueued = 20;
	inline static constexpr int kMaxAttempts = 8;
	inline static constexpr int kMaxQueuedDays = 7;
	inline static constexpr int kRememberSentDays = 30;
	inline static constexpr int kRetryMs = 30 * 60 * 1000;

  signals:
	void stateChanged(const QString& signature);

  private:
	struct Pending {
		QJsonObject payload;
		QDateTime queuedAt;
		int attempts = 0;
	};

	void load();
	void save() const;
	void post(const QString& signature);
	qsizetype indexOf(const QString& signature) const;
	void finish(const QString& signature, Outcome outcome);
	void forgetOld();

	QString m_path;
	std::function<QString()> m_serverUrl;
	FriendsHttp* m_http = nullptr;
	QList<Pending> m_queue;
	QHash<QString, QDateTime> m_sent;
	QSet<QString> m_sending;
	QSet<QString> m_failed;
	QTimer m_timer;
};
