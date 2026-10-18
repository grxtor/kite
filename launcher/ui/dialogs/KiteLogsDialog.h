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

#include <QDialog>
#include <QFileSystemWatcher>

namespace Ui
{
	class KiteLogsDialog;
}

class KiteLogsDialog : public QDialog
{
	Q_OBJECT

  public:
	explicit KiteLogsDialog(QWidget* parent = nullptr);
	~KiteLogsDialog();

  private slots:
	void on_selectLogBox_currentIndexChanged(int index);
	void on_btnReload_clicked();
	void on_btnCopy_clicked();
	void on_btnUpload_clicked();
	void on_btnDelete_clicked();
	void on_btnClean_clicked();
	void on_findButton_clicked();
	void onLogFileChanged(const QString& path);

  private:
	void populateLogList();
	void loadSelectedLog();
	void setControlsEnabled(bool enabled);
	QString logFilePath(const QString& name) const;
	QString logDirectory() const;

	Ui::KiteLogsDialog* ui;
	QFileSystemWatcher* m_liveWatcher;
	QString m_currentFile;
	bool m_watching0Log = false;
};
