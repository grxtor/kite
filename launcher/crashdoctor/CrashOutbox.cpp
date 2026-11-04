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

#include "CrashOutbox.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

#include <utility>

#include "Logging.h"
#include "friends/FriendsApi.h"
#include "friends/FriendsHttp.h"

namespace
{
	const QString kFile = QStringLiteral("outbox.json");
	const QString kPath = QStringLiteral("/v1/crash-reports");
	constexpr int kAccepted = 202;
	constexpr int kTimeout = 408;
	constexpr int kTooMany = 429;
	constexpr int kClientErrors = 400;
	constexpr int kServerErrors = 500;

	QString signatureOf(const QJsonObject& payload)
	{
		return payload.value("signature").toString();
	}
} // namespace

CrashOutbox::CrashOutbox(const QString& dir, std::function<QString()> serverUrl, QObject* parent)
	: QObject(parent), m_path(QDir(dir).filePath(kFile)), m_serverUrl(std::move(serverUrl)),
	  m_http(new FriendsHttp(this))
{
	m_timer.setInterval(kRetryMs);
	connect(&m_timer, &QTimer::timeout, this, &CrashOutbox::retry);
	load();
}

CrashOutbox::~CrashOutbox() = default;

CrashOutbox::Outcome CrashOutbox::outcome(int status)
{
	if (status == kAccepted) {
		return Outcome::Sent;
	}
	// Refused for what it is: sending it again changes nothing.
	if (status >= kClientErrors && status < kServerErrors && status != kTimeout && status != kTooMany) {
		return Outcome::Drop;
	}
	return Outcome::Retry;
}

void CrashOutbox::load()
{
	QFile file(m_path);
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}
	const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
	for (const QJsonValue& value : root.value("queued").toArray()) {
		const QJsonObject item = value.toObject();
		Pending pending{item.value("payload").toObject(),
						QDateTime::fromString(item.value("queuedAt").toString(), Qt::ISODate),
						item.value("attempts").toInt()};
		if (!signatureOf(pending.payload).isEmpty() && pending.queuedAt.isValid()) {
			m_queue.append(pending);
		}
	}
	const QJsonObject sent = root.value("sent").toObject();
	for (auto it = sent.begin(); it != sent.end(); ++it) {
		m_sent.insert(it.key(), QDateTime::fromString(it.value().toString(), Qt::ISODate));
	}
	forgetOld();
	if (!m_queue.isEmpty()) {
		m_timer.start();
	}
}

void CrashOutbox::save() const
{
	QJsonArray queued;
	for (const Pending& pending : m_queue) {
		queued.append(QJsonObject{{"payload", pending.payload},
								  {"queuedAt", pending.queuedAt.toString(Qt::ISODate)},
								  {"attempts", pending.attempts}});
	}
	QJsonObject sent;
	for (auto it = m_sent.begin(); it != m_sent.end(); ++it) {
		sent.insert(it.key(), it.value().toString(Qt::ISODate));
	}
	QDir().mkpath(QFileInfo(m_path).absolutePath());
	QSaveFile file(m_path);
	if (!file.open(QIODevice::WriteOnly)) {
		qCWarning(crashDoctorLog) << "Could not keep the crash reports to send:" << file.errorString();
		return;
	}
	file.write(QJsonDocument(QJsonObject{{"queued", queued}, {"sent", sent}}).toJson(QJsonDocument::Compact));
	file.commit();
}

void CrashOutbox::forgetOld()
{
	const QDateTime now = QDateTime::currentDateTimeUtc();
	m_queue.removeIf([&now](const Pending& pending) {
		return pending.attempts >= kMaxAttempts || pending.queuedAt.daysTo(now) > kMaxQueuedDays;
	});
	m_sent.removeIf([&now](QHash<QString, QDateTime>::iterator sent) {
		return !sent.value().isValid() || sent.value().daysTo(now) > kRememberSentDays;
	});
}

void CrashOutbox::send(const QJsonObject& payload)
{
	const QString signature = signatureOf(payload);
	if (signature.isEmpty() || m_sent.contains(signature) || m_sending.contains(signature)) {
		return;
	}
	m_failed.remove(signature);
	if (indexOf(signature) < 0) {
		if (m_queue.size() >= kMaxQueued) {
			m_queue.removeFirst();
		}
		m_queue.append({payload, QDateTime::currentDateTimeUtc(), 0});
		save();
	}
	post(signature);
}

void CrashOutbox::retry()
{
	forgetOld();
	QStringList waiting;
	for (const Pending& pending : m_queue) {
		waiting << signatureOf(pending.payload);
	}
	for (const QString& signature : waiting) {
		post(signature);
	}
	if (m_queue.isEmpty()) {
		m_timer.stop();
	}
}

qsizetype CrashOutbox::indexOf(const QString& signature) const
{
	for (qsizetype i = 0; i < m_queue.size(); ++i) {
		if (signatureOf(m_queue.at(i).payload) == signature) {
			return i;
		}
	}
	return -1;
}

void CrashOutbox::post(const QString& signature)
{
	const qsizetype index = indexOf(signature);
	const QString base = m_serverUrl ? m_serverUrl().trimmed() : QString();
	if (index < 0 || m_sending.contains(signature)) {
		return;
	}
	Pending& pending = m_queue[index];
	if (base.isEmpty()) {
		emit stateChanged(signature);
		return;
	}
	++pending.attempts;
	save();
	m_sending.insert(signature);
	emit stateChanged(signature);
	const QByteArray body = QJsonDocument(pending.payload).toJson(QJsonDocument::Compact);
	m_http->send("POST", FriendsApi::url(base, kPath), body, QString(),
				 [this, signature](const FriendsApi::Reply& reply) { finish(signature, outcome(reply.status)); });
}

void CrashOutbox::finish(const QString& signature, Outcome result)
{
	m_sending.remove(signature);
	if (result != Outcome::Retry) {
		m_queue.removeIf([&signature](const Pending& one) { return signatureOf(one.payload) == signature; });
	}
	if (result == Outcome::Sent) {
		m_sent.insert(signature, QDateTime::currentDateTimeUtc());
	} else if (result == Outcome::Drop) {
		qCWarning(crashDoctorLog) << "Kite's server refused a crash report";
		m_failed.insert(signature);
	}
	forgetOld();
	save();
	if (m_queue.isEmpty()) {
		m_timer.stop();
	} else if (!m_timer.isActive()) {
		m_timer.start();
	}
	emit stateChanged(signature);
}

CrashOutbox::State CrashOutbox::state(const QString& signature) const
{
	if (m_sent.contains(signature)) {
		return State::Sent;
	}
	if (m_sending.contains(signature)) {
		return State::Sending;
	}
	if (m_failed.contains(signature)) {
		return State::Failed;
	}
	return indexOf(signature) >= 0 ? State::Queued : State::None;
}
