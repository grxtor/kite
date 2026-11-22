/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QNetworkInformation>
#include <QObject>
#include <QTimer>

#include <optional>

/*
 * Whether this machine has a network connection, for the QML shell's offline
 * notice: one `online` property over Qt's QNetworkInformation.
 *
 * Only a known "Disconnected" counts as offline. With no backend, or a backend
 * that cannot tell, `online` stays true and nothing is ever shown. The first
 * answer is held back for startDelayMs (the system's state is not settled while
 * the launcher starts) and a later drop for dropDelayMs (a network switch
 * passes through "disconnected"); coming back online is reported at once.
 */
class NetworkStatus : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool online READ online NOTIFY onlineChanged)

  public:
	using Reachability = QNetworkInformation::Reachability;

	explicit NetworkStatus(QObject* parent = nullptr, int startDelayMs = 3000,
						   int dropDelayMs = 2000);

	/// What the system says now; Unknown where Qt has no backend for it.
	static Reachability systemReachability();
	/// False only for Disconnected.
	static bool isOnline(Reachability reachability);

	bool online() const
	{
		return m_online;
	}

	/// Reads the state again without any delay ("Try again"); returns online().
	Q_INVOKABLE bool recheck();

	/// Stands in for the system's answer from now on: tests, and the dev
	/// route "network=offline|online|unknown" (applied without delay).
	void setReachability(Reachability reachability);
	Q_INVOKABLE void debugSetReachability(const QString& name);

  signals:
	void onlineChanged();

  private:
	Reachability current() const;
	void changed();
	void apply();

	std::optional<Reachability> m_forced;
	QTimer m_hold;
	int m_dropDelayMs;
	bool m_online = true;
};
