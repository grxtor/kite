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

/*
 * The launcher's total play time across every instance, for the quiet
 * "Total play time" line the Library shows when ShowGlobalGameTime is on
 * (what MainWindow::updateStatusCenter() put in the status bar). It follows
 * the instance list: any change to the list or to an instance's data
 * recounts, which is a short loop over a handful of numbers.
 */
class PlayTimeSummary : public QObject
{
	Q_OBJECT

	/// Seconds; 0 when nothing has been played or there is no library yet.
	Q_PROPERTY(qint64 totalSeconds READ totalSeconds NOTIFY totalChanged)

  public:
	explicit PlayTimeSummary(QObject* parent = nullptr);
	~PlayTimeSummary() override;

	qint64 totalSeconds() const;

	/// Counts again.
	Q_INVOKABLE void refresh();

  signals:
	void totalChanged();

  private:
	qint64 m_total = 0;
};
