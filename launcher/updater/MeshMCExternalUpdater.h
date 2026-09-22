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

#include <QDateTime>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <memory>

class QSettings;

class MeshMCExternalUpdater : public ExternalUpdater
{
	Q_OBJECT

  public:
	/*!
	 * \a appDir   installation root (Application::root()).
	 * \a dataDir  where the config, the log and the markers live.
	 * \a autoCheckDefault what "check automatically" means for an
	 *                     installation that has never answered the question.
	 *
	 * The default exists so that a user who had already turned automatic
	 * checks off in the launcher's settings, before the updater kept a config
	 * of its own, does not silently get them back on. It applies once: the
	 * answer is written to the config the first time.
	 *
	 * Starts the automatic check schedule, and -- when the interval is set to
	 * "On Launch" -- performs a silent check before returning.
	 */
	MeshMCExternalUpdater(const QString& appDir, const QString& dataDir,
						  bool autoCheckDefault = true);
	~MeshMCExternalUpdater() override;

	//! Interactive check: shows progress, and reports even a negative result.
	void checkForUpdates() override;

	/*!
	 * \a triggeredByUser distinguishes the menu entry from the timer.
	 *
	 * An automatic check stays silent unless it has something to offer, and
	 * it honours "Skip This Version"; an interactive one always answers, and
	 * ignores the skip list, because the user just asked.
	 */
	void checkForUpdates(bool triggeredByUser);

	bool getAutomaticallyChecksForUpdates() override;
	double getUpdateCheckInterval() override;
	bool getBetaAllowed() override;

	void setAutomaticallyChecksForUpdates(bool check) override;
	void setUpdateCheckInterval(double seconds) override;
	void setBetaAllowed(bool allowed) override;

	/*!
	 * Where the updater sits relative to the installation root.
	 *
	 * Windows keeps the executables at the root, everything else puts them in
	 * bin/. Application uses this to decide whether updates are available at
	 * all, so it lives here rather than being spelled out twice.
	 */
	static QString updaterBinaryRelativePath();

  public slots:
	//! The automatic check schedule came due.
	void autoCheckTimerFired();

  private:
	//! Ask, remember the answer, then act on it.
	void offerUpdate(const QString& versionName, const QString& versionTag,
					 const QString& releaseNotes, bool triggeredByUser);

	//! Hand the install over to the updater and get out of its way.
	void performUpdate(const QString& versionTag);

	//! Arm, re-arm or disarm the schedule to match the current preferences.
	void resetAutoCheckTimer();

	//! Stamp this moment as the last check and re-arm.
	void noteCheckCompleted();

	QString updaterExecutablePath() const;
	QStringList commonArguments() const;

	QDir m_appDir;
	QDir m_dataDir;

	std::unique_ptr<QSettings> m_settings;
	QTimer m_updateTimer;
	QDateTime m_lastCheck;

	bool m_allowBeta = false;
	bool m_autoCheck = true;
	double m_updateInterval = 0.0;

	//! Guards against a second check while one is already blocking.
	bool m_checking = false;
};
