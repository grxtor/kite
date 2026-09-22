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

#include <QDialog>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include "modplatform/BlockedMod.h"

class BlockedModsDialog : public QDialog
{
	Q_OBJECT

  public:
	explicit BlockedModsDialog(QWidget* parent, const QString& title,
							   const QString& text, QList<BlockedMod>& mods);

	/// Returns the list of mods with updated `found` status
	QList<BlockedMod>& resultMods()
	{
		return m_mods;
	}

  private slots:
	void onDownloadDirChanged(const QString& path);
	void openModDownload(int index);

  private:
	void scanDownloadsFolder();
	void updateModStatus();
	void setupWatch();

	QList<BlockedMod>& m_mods;
	QString m_downloadDir;
	QFileSystemWatcher m_watcher;

	struct ModRow {
		QLabel* nameLabel;
		QLabel* statusLabel;
		QPushButton* downloadButton;
	};
	QList<ModRow> m_rows;

	QDialogButtonBox* m_buttons;
};
