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

#include "ExternalUpdater.h"

#include <QSet>
#include <QString>

/*! Sparkle 2 with EdDSA-signed updates and Sparkle's own windows. The feed and the public key come
 * from Info.plist (SUFeedURL, SUPublicEDKey). Kite is ad-hoc signed, so Sparkle trusts an update on
 * its EdDSA signature alone (it still requires a valid, if different, code signature).
 *
 * Games: Sparkle relaunches Kite to install, and quitting Kite would take a game started from it
 * down, so while one runs the relaunch waits for it to end. An update downloaded and not installed
 * yet goes in when Kite quits, which is Sparkle's "install on quit". */
class MacSparkleUpdater : public ExternalUpdater
{
	Q_OBJECT

  public:
	MacSparkleUpdater();
	~MacSparkleUpdater() override;

	void checkForUpdates() override;

	bool getAutomaticallyChecksForUpdates() override;
	bool getBetaAllowed() override;

	void setAutomaticallyChecksForUpdates(bool check) override;
	void setBetaAllowed(bool allowed) override;

	QDateTime lastCheck() override;

	void setGamesRunning(bool running) override;

	QSet<QString> getAllowedChannels();
	void setAllowedChannel(const QString& channel);
	void setAllowedChannels(const QSet<QString>& channels);

	//! Back to release-only.
	void clearAllowedChannels();

	// For the Sparkle delegate.
	bool gamesRunning() const
	{
		return m_gamesRunning;
	}
	//! Sparkle wanted to relaunch into an update while a game runs: installWaitsForGame().
	void notifyRelaunchPostponed();
	//! An update cycle is over: lastCheck() may have moved.
	void notifyCycleFinished();

  private:
	class Private;
	Private* priv;
};
