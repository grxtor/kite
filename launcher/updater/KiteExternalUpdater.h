/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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
#include "core/UiHost.h"

#include <QDateTime>
#include <QDir>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <memory>

class QSettings;

class KiteExternalUpdater : public ExternalUpdater
{
	Q_OBJECT

  public:
	/*! \a appDir is the installation root (Application::root()), \a dataDir where the config, log and markers live.
	 * \a autoCheckDefault is what "check automatically" means for an installation that has never answered; it
	 * applies once and is then written to the config, so a user who turned checks off before the updater kept
	 * a config of its own does not get them back on.
	 * \a managedBy names the package manager that updates this installation (UpdateLockFile::managedBy(), or
	 * "flatpak"); then a release is only announced, with what to run.
	 * With automatic checks on, a silent check runs shortly after start and then once a day. */
	KiteExternalUpdater(const QString& appDir, const QString& dataDir,
						bool autoCheckDefault = true,
						const QString& managedBy = QString());
	~KiteExternalUpdater() override;

	//! Interactive check: shows progress, and reports even a negative result.
	void checkForUpdates() override;

	/*! \a triggeredByUser separates the menu entry from the timer: an automatic check stays silent unless it has
	 * something to offer and honours "Skip This Version"; an interactive one always answers and ignores the
	 * skip list. */
	void checkForUpdates(bool triggeredByUser);

	bool getAutomaticallyChecksForUpdates() override;
	bool getBetaAllowed() override;

	void setAutomaticallyChecksForUpdates(bool check) override;
	void setBetaAllowed(bool allowed) override;

	QDateTime lastCheck() override;
	QString pendingInstall() override;
	QString managedBy() override;
	void installPendingNow() override;
	void applicationExiting() override;

	//! How to name \a managedBy to the user ("scoop" -> "Scoop").
	static QString managerDisplayName(const QString& managedBy);

	//! What to tell the user to run, when \a managedBy updates Kite.
	static QString managedUpdateHint(const QString& managedBy);

	/*! Where the updater sits relative to the installation root: at the root on Windows, in bin/ elsewhere.
	 * Application uses it to decide whether updates are available at all. */
	static QString updaterBinaryRelativePath();

  public slots:
	//! The automatic check schedule came due.
	void autoCheckTimerFired();

  private:
	//! Ask, remember the answer, then act on it.
	void offerUpdate(const QString& versionTag, const QString& releaseNotes,
					 bool triggeredByUser);

	/*! Starts kite-updater on the install of \a versionTag, detached. \a relaunch: whether it starts
	 * Kite again once it is done. False, with the reason shown, when it could not be started. */
	bool startInstall(const QString& versionTag, bool relaunch);

	//! startInstall(), then get out of the updater's way: it waits for this process to be gone.
	void performUpdate(const QString& versionTag);

	/*! An AppImage's updater runs from the AppImage's mount, which goes when Kite exits: Kite waits
	 * for it here, then starts the new AppImage itself. */
	void installFromAppImage(const QString& versionTag);

	//! kite-updater installing \a versionTag and leaving the launcher closed.
	QProcess* makeInstallProcess(const QString& versionTag);

	//! Arm, re-arm or disarm the schedule to match the current preferences.
	void resetAutoCheckTimer();

	/*! Ends a check and re-arms the schedule. \a answered: the updater reached the release list and
	 * said yes or no, the only kind of check "Last checked" counts; after a failure the next
	 * automatic try is an hour away instead of a day. */
	void noteCheckCompleted(bool answered);

	//! The check process ended on its own; reports the result.
	void onCheckFinished(int exitCode);

	//! The check process never started or never ended in time; kills it and reports.
	void onCheckTimedOut();

	//! Reports a check that did not start.
	void onCheckStartFailed(const QString& reason);

	//! Interprets the exit code and output of a finished check.
	void handleCheckResult(int exitCode, const QByteArray& stdOutput,
						   const QByteArray& stdError);

	//! Drops the check process and its busy indication.
	void releaseCheckProcess();

	QString updaterExecutablePath() const;
	QStringList commonArguments() const;

	QDir m_appDir;
	QDir m_dataDir;

	std::unique_ptr<QSettings> m_settings;
	QTimer m_updateTimer;
	QDateTime m_lastCheck;

	bool m_allowBeta = false;
	bool m_autoCheck = true;

	QString m_managedBy;

	//! Chosen with "Update when I close Kite": installed by applicationExiting().
	QString m_pendingTag;
	QString m_pendingVersion;

	//! Guards against a second check while one is already running.
	bool m_checking = false;

	bool m_checkByUser = false;
	QPointer<QProcess> m_checkProcess;
	QTimer m_checkTimeout;
	std::unique_ptr<UiHost::BusyIndicator> m_busy;
};
