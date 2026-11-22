/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 */

#include "NetworkStatus.h"

NetworkStatus::NetworkStatus(QObject* parent, int startDelayMs, int dropDelayMs)
	: QObject(parent), m_dropDelayMs(dropDelayMs)
{
	m_hold.setSingleShot(true);
	connect(&m_hold, &QTimer::timeout, this, &NetworkStatus::apply);
	// Loads the backend, so instance() below is the one that will report.
	systemReachability();
	if (auto* info = QNetworkInformation::instance()) {
		connect(info, &QNetworkInformation::reachabilityChanged, this,
				&NetworkStatus::changed);
	}
	m_hold.start(startDelayMs);
}

NetworkStatus::Reachability NetworkStatus::systemReachability()
{
	// Loaded once; with no backend the answer is "unknown".
	static const bool loaded = QNetworkInformation::loadDefaultBackend();
	if (!loaded || !QNetworkInformation::instance()) {
		return Reachability::Unknown;
	}
	return QNetworkInformation::instance()->reachability();
}

bool NetworkStatus::isOnline(Reachability reachability)
{
	return reachability != Reachability::Disconnected;
}

bool NetworkStatus::recheck()
{
	m_hold.stop();
	apply();
	return m_online;
}

void NetworkStatus::setReachability(Reachability reachability)
{
	m_forced = reachability;
	changed();
}

void NetworkStatus::debugSetReachability(const QString& name)
{
	m_forced = name == QLatin1String("offline")	 ? Reachability::Disconnected
			   : name == QLatin1String("online") ? Reachability::Online
												 : Reachability::Unknown;
	recheck();
}

NetworkStatus::Reachability NetworkStatus::current() const
{
	return m_forced ? *m_forced : systemReachability();
}

void NetworkStatus::changed()
{
	if (isOnline(current())) {
		// Still inside the start-up delay: the timer applies it.
		if (m_online && m_hold.isActive()) {
			return;
		}
		m_hold.stop();
		apply();
	} else if (m_online && !m_hold.isActive()) {
		m_hold.start(m_dropDelayMs);
	}
}

void NetworkStatus::apply()
{
	const bool online = isOnline(current());
	if (online == m_online) {
		return;
	}
	m_online = online;
	emit onlineChanged();
}
