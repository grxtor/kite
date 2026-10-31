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

class ExternalUpdater : public QObject
{
	Q_OBJECT

  public:
	virtual void checkForUpdates() = 0;

	//! Whether unattended checks happen at all.
	virtual bool getAutomaticallyChecksForUpdates() = 0;

	//! Seconds between unattended checks. 0 means "only at startup".
	virtual double getUpdateCheckInterval() = 0;

	//! Whether pre-releases are offered alongside stable releases.
	virtual bool getBetaAllowed() = 0;

	virtual void setAutomaticallyChecksForUpdates(bool check) = 0;

	virtual void setUpdateCheckInterval(double seconds) = 0;

	virtual void setBetaAllowed(bool allowed) = 0;

  signals:
	void canCheckForUpdatesChanged(bool canCheck);
};
