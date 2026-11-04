/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include <functional>

/*
 * The launcher's update settings for QML: automatic checks (when Kite starts
 * and once a day), the release channel, when Kite last checked, "Check now",
 * and an update waiting for Kite to close. The updater lives in Application,
 * which core code cannot reach, so Application hands in a Backend of plain
 * functions once it exists. Until then (and in a build with no updater)
 * available is false and everything does nothing, which is how the Settings
 * page knows to hide itself.
 */
class UpdateSettingsController : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool available READ available NOTIFY changed)
	Q_PROPERTY(bool autoCheck READ autoCheck WRITE setAutoCheck NOTIFY changed)
	/// "stable" (the only one there is yet) or "beta".
	Q_PROPERTY(QString channel READ channel NOTIFY changed)
	/// When the last check finished, in ms since the epoch; 0 for never.
	Q_PROPERTY(qint64 lastCheckedMs READ lastCheckedMs NOTIFY changed)
	/// The version that installs once Kite closes; empty when none waits.
	Q_PROPERTY(QString pendingVersion READ pendingVersion NOTIFY changed)
	/// Whether Kite may restart into an update now: not while a game runs.
	Q_PROPERTY(bool canInstallNow READ canInstallNow NOTIFY changed)
	/// The package manager that updates Kite here ("Scoop"), empty when Kite
	/// updates itself.
	Q_PROPERTY(QString managedBy READ managedBy NOTIFY changed)

  public:
	struct Backend {
		// Required: without these the controller stays unavailable.
		std::function<bool()> automaticallyChecks;
		std::function<void(bool)> setAutomaticallyChecks;
		std::function<QDateTime()> lastCheck;
		std::function<void()> checkNow;

		// Optional: Sparkle keeps its own pending update and has none of these.
		std::function<QString()> pendingVersion;
		std::function<bool()> canInstallNow;
		std::function<void()> installPendingNow;

		QString channel = QStringLiteral("stable");
		QString managedBy;
	};

	explicit UpdateSettingsController(QObject* parent = nullptr);
	~UpdateSettingsController() override;

	void setBackend(Backend backend);

	bool available() const;
	bool autoCheck() const;
	void setAutoCheck(bool check);
	QString channel() const;
	qint64 lastCheckedMs() const;
	QString pendingVersion() const;
	bool canInstallNow() const;
	QString managedBy() const;

	Q_INVOKABLE void checkNow();
	Q_INVOKABLE void installPendingNow();

  public slots:
	/// The updater's state moved (a check finished, a game started or
	/// ended, an update began waiting): QML reads it again.
	void refresh();

  signals:
	void changed();

  private:
	Backend m_backend;
	bool m_available = false;
};
