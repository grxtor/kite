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
#include <QString>

class LogUploader;
class OtherLogsModel;

/*
 * The launcher's own log files (MeshMC-0.log ... MeshMC-4.log in its data
 * folder) for the Settings "Launcher logs" section - the widget-free
 * replacement for MeshMCLogsDialog. A game's logs live on each instance's
 * Log tab; these are the launcher's.
 *
 * Reuses OtherLogsModel (list, view, delete one, delete all, gzip-aware
 * loading) pointed at the data folder, but flat: the data folder also holds
 * every instance, which must not be scanned and watched for log files.
 * LogUploader is the upload for the selected file.
 *
 * MeshMC-0.log is the current run's. It grows while the launcher runs;
 * OtherLogsModel::reload() picks that up.
 */
class LauncherLogs : public QObject
{
	Q_OBJECT

	/// OtherLogsModel over the launcher's log files, newest (0) first.
	Q_PROPERTY(QObject* logs READ logs CONSTANT)
	Q_PROPERTY(QObject* uploader READ uploader CONSTANT)
	/// The folder the logs are in, for an "Open folder" action.
	Q_PROPERTY(QString folder READ folder CONSTANT)

  public:
	/// @p folder / @p namePrefix: where the launcher writes its logs and
	/// what they start with (BuildConfig.MESHMC_NAME).
	explicit LauncherLogs(const QString& folder, const QString& namePrefix,
						  QObject* parent = nullptr);
	~LauncherLogs() override;

	QObject* logs() const;
	QObject* uploader() const;
	QString folder() const
	{
		return m_folder;
	}

	/// The pattern the log files' names match: "<prefix>-<n>.log".
	static QString fileNamePattern(const QString& namePrefix);

  private:
	QString m_folder;
	OtherLogsModel* m_logs;
	LogUploader* m_uploader;
};
