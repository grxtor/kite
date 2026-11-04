/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 Project Tick
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
#include <QObject>
#include <QString>

/*! What Application and the Settings page need from an updater: Sparkle on macOS, kite-updater
 * (KiteExternalUpdater) elsewhere. Automatic checks run when Kite starts and then once a day. */
class ExternalUpdater : public QObject
{
	Q_OBJECT

  public:
	virtual void checkForUpdates() = 0;

	//! Whether unattended checks happen at all.
	virtual bool getAutomaticallyChecksForUpdates() = 0;

	//! Whether pre-releases are offered alongside stable releases.
	virtual bool getBetaAllowed() = 0;

	virtual void setAutomaticallyChecksForUpdates(bool check) = 0;

	virtual void setBetaAllowed(bool allowed) = 0;

	//! When the last check finished; invalid if there has been none.
	virtual QDateTime lastCheck() = 0;

	/*! Whether a game started from Kite is running. Kite is never replaced under one: an install
	 * waits until Kite closes (or, with Sparkle, until the game ends). Application keeps this
	 * current. */
	virtual void setGamesRunning(bool running)
	{
		m_gamesRunning = running;
	}

	//! The version that installs once Kite closes ("1.3.0"); empty when there is none.
	virtual QString pendingInstall()
	{
		return {};
	}

	/*! The package manager that updates Kite here ("Scoop", "Flatpak"), named for the user; empty
	 * when Kite updates itself. */
	virtual QString managedBy()
	{
		return {};
	}

	//! Restarts into the pending update now; does nothing while a game runs.
	virtual void installPendingNow() {}

	//! Kite is quitting (aboutToQuit): the moment a pending update installs.
	virtual void applicationExiting() {}

  signals:
	//! A check finished or an update started waiting: lastCheck() or pendingInstall() changed.
	void stateChanged();

	//! An install is ready but waits for a game started from Kite to end; Application says so.
	void installWaitsForGame();

  protected:
	bool m_gamesRunning = false;
};
