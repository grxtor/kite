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

#include <QObject>
#include <functional>

/*
 * The launcher's update settings for QML: whether it checks for updates by
 * itself, how often, and a "Check now". The updater (Sparkle, the built-in
 * one) lives in Application, which core code cannot reach, so Application
 * hands in a Backend of plain functions once the updater exists. Until then
 * (and in a build with no updater) available is false and everything here
 * does nothing, which is also how the Settings page knows to hide itself.
 *
 * The interval is exposed in whole hours, like the widget page; 0 means
 * "only when the launcher starts", which is what the updater means by 0
 * seconds too.
 */
class UpdateSettingsController : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool available READ available NOTIFY changed)
	Q_PROPERTY(bool autoCheck READ autoCheck WRITE setAutoCheck NOTIFY changed)
	/// 0 = only at startup.
	Q_PROPERTY(int intervalHours READ intervalHours WRITE setIntervalHours
				   NOTIFY changed)

  public:
	struct Backend {
		std::function<bool()> automaticallyChecks;
		std::function<void(bool)> setAutomaticallyChecks;
		/// Seconds between checks; 0 = only at startup.
		std::function<double()> intervalSeconds;
		std::function<void(double)> setIntervalSeconds;
		std::function<void()> checkNow;
	};

	explicit UpdateSettingsController(QObject* parent = nullptr);
	~UpdateSettingsController() override;

	/// Called by Application once the updater exists.
	void setBackend(Backend backend);

	bool available() const;
	bool autoCheck() const;
	void setAutoCheck(bool check);
	int intervalHours() const;
	void setIntervalHours(int hours);

	/// Runs an update check now and shows whatever the updater shows.
	Q_INVOKABLE void checkNow();

	static constexpr int secondsPerHour = 3600;

  signals:
	void changed();

  private:
	Backend m_backend;
	bool m_available = false;
};
