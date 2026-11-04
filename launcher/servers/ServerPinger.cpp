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

#include "ServerPinger.h"

#include <limits>

#include "Logging.h"
#include "minecraft/launch/MinecraftServerTarget.h"
#include "servers/ServerPing.h"

namespace
{
	const QString kPlaying = QStringLiteral("playing");
	const QString kServerWorld = QStringLiteral("server");

	QString normalizedHost(QString host)
	{
		host = host.trimmed().toLower();
		if (host.startsWith(QLatin1Char('[')) && host.endsWith(QLatin1Char(']'))) {
			host = host.mid(1, host.size() - 2);
		}
		if (host.endsWith(QLatin1Char('.'))) {
			host.chop(1);
		}
		return host;
	}
} // namespace

ServerPinger::Endpoint ServerPinger::endpointOf(const QString& address)
{
	const auto target = MinecraftServerTarget::parse(address.trimmed(), false);
	return {normalizedHost(target.address), target.port};
}

QString ServerPinger::keyOf(const QString& host, quint16 port)
{
	const QString normalized = normalizedHost(host);
	if (normalized.isEmpty()) {
		return QString();
	}
	/* An IPv6 address has colons of its own. */
	const QString shown = normalized.contains(QLatin1Char(':'))
							  ? QLatin1Char('[') + normalized + QLatin1Char(']')
							  : normalized;
	return shown + QLatin1Char(':') + QString::number(port);
}

QString ServerPinger::keyOf(const QString& address)
{
	const Endpoint endpoint = endpointOf(address);
	return keyOf(endpoint.host, endpoint.port);
}

ServerPinger::ServerPinger(QObject* parent)
	: QObject(parent), m_timeoutMs(ServerPing::kDefaultTimeoutMs)
{
	m_clock.start();
}

void ServerPinger::setTimeoutMs(int timeoutMs)
{
	m_timeoutMs = timeoutMs;
}

void ServerPinger::setCacheMs(int cacheMs)
{
	m_cacheMs = cacheMs;
}

void ServerPinger::setRefreshFloorMs(int floorMs)
{
	m_refreshFloorMs = floorMs;
}

void ServerPinger::ping(const QString& address)
{
	const Endpoint endpoint = endpointOf(address);
	const QString key = keyOf(endpoint.host, endpoint.port);
	if (key.isEmpty()) {
		return;
	}
	const auto it = m_entries.constFind(key);
	if (it != m_entries.constEnd() &&
		(it->pending ||
		 (it->answered && m_clock.elapsed() - it->answeredAt < m_cacheMs))) {
		return;
	}
	enqueue(key, endpoint);
}

void ServerPinger::refresh(const QString& address)
{
	const Endpoint endpoint = endpointOf(address);
	const QString key = keyOf(endpoint.host, endpoint.port);
	if (key.isEmpty()) {
		return;
	}
	const auto it = m_entries.constFind(key);
	if (it != m_entries.constEnd() &&
		(it->pending ||
		 m_clock.elapsed() - it->askedAt < m_refreshFloorMs)) {
		return;
	}
	enqueue(key, endpoint);
}

void ServerPinger::enqueue(const QString& key, const Endpoint& endpoint)
{
	Entry& entry = m_entries[key];
	entry.endpoint = endpoint;
	entry.pending = true;
	entry.askedAt = m_clock.elapsed();
	m_queue.append(key);
	++m_revision;
	emit updated();
	startNext();
}

void ServerPinger::startNext()
{
	while (m_running < kMaxConcurrent && !m_queue.isEmpty()) {
		const QString key = m_queue.takeFirst();
		const Endpoint endpoint = m_entries.value(key).endpoint;
		auto* ping = new ServerPing(endpoint.host, endpoint.port,
									endpoint.port == kDefaultPort, this);
		ping->setTimeoutMs(m_timeoutMs);
		connect(ping, &ServerPing::succeeded, this,
				[this, key, ping](const ServerStatus::Status& status) {
					ping->deleteLater();
					finish(key, true, status);
				});
		connect(ping, &ServerPing::failed, this,
				[this, key, ping](const QString& reason) {
					qCDebug(netLog) << "Server ping of" << key << "failed:" << reason;
					ping->deleteLater();
					finish(key, false, {});
				});
		++m_running;
		ping->start();
	}
}

void ServerPinger::finish(const QString& key, bool online,
						  const ServerStatus::Status& status)
{
	--m_running;
	Entry& entry = m_entries[key];
	entry.pending = false;
	entry.answered = true;
	entry.online = online;
	entry.status = status;
	entry.answeredAt = m_clock.elapsed();
	++m_revision;
	emit updated();
	startNext();
}

QVariantMap ServerPinger::status(const QString& address) const
{
	const auto it = m_entries.constFind(keyOf(address));
	if (it == m_entries.constEnd()) {
		return {{QStringLiteral("state"), QString()}};
	}
	const QString state = it->answered ? (it->online ? QStringLiteral("online")
													 : QStringLiteral("offline"))
						  : it->pending ? QStringLiteral("pending")
										: QString();
	QVariantMap map{
		{QStringLiteral("state"), state},
		{QStringLiteral("refreshing"), it->answered && it->pending},
	};
	if (it->answered && it->online) {
		const ServerStatus::Status& status = it->status;
		map[QStringLiteral("online")] = status.online;
		map[QStringLiteral("max")] = status.max;
		map[QStringLiteral("version")] = status.version;
		map[QStringLiteral("motd")] = status.motd;
		map[QStringLiteral("motdLight")] = status.motdLight;
		map[QStringLiteral("motdText")] = status.motdText;
		map[QStringLiteral("favicon")] = status.favicon;
		map[QStringLiteral("ping")] = status.latencyMs;
	}
	return map;
}

QVariantList ServerPinger::friendsOn(const QString& address,
									 const QVariantList& friends) const
{
	const QString key = keyOf(address);
	QVariantList here;
	if (key.isEmpty()) {
		return here;
	}
	for (const QVariant& value : friends) {
		const QVariantMap person = value.toMap();
		const QString host = person.value(QStringLiteral("host")).toString();
		const int port = person.value(QStringLiteral("port")).toInt();
		if (person.value(QStringLiteral("state")).toString() != kPlaying ||
			person.value(QStringLiteral("worldType")).toString() != kServerWorld ||
			host.isEmpty()) {
			continue;
		}
		const quint16 shownPort =
			port > 0 && port <= std::numeric_limits<quint16>::max()
				? quint16(port)
				: kDefaultPort;
		if (keyOf(host, shownPort) == key) {
			here.append(value);
		}
	}
	return here;
}
