/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include <memory>
#include <QDialog>

#include "java/JavaChecker.h"
#include "ui/pages/BasePage.h"
#include <Application.h>
#include "ui/ColorCache.h"
#include <translations/TranslationsModel.h>

class QTextCharFormat;
class SettingsObject;

namespace Ui
{
	class KitePage;
}

class KitePage : public QWidget, public BasePage
{
	Q_OBJECT

  public:
	explicit KitePage(QWidget* parent = 0);
	~KitePage();

	QString displayName() const override
	{
		return "Kite";
	}
	QIcon icon() const override
	{
		return APPLICATION->getThemedIcon("launcher");
	}
	QString id() const override
	{
		return "launcher-settings";
	}
	QString helpPage() const override
	{
		return "Kite-settings";
	}
	bool apply() override;

  private:
	void applySettings();
	void loadSettings();

	/*!
	 * Vet a folder the user picked for holding instances, and answer whether
	 * to go ahead with it.
	 *
	 * Shared by the primary instance folder and the additional ones, because
	 * the hazards belong to the path and not to the box it was typed into:
	 * an instance under a '!' fails the same way whichever folder it was
	 * discovered in. Returns true when there is nothing wrong, or when the
	 * user has seen the warning and chosen to continue anyway.
	 *
	 * Both spellings of the path are needed. The '!' check wants the
	 * normalised one, while the Flatpak check has to look at what the file
	 * dialog actually handed back - normalising resolves the sandbox path
	 * away and the check would never fire.
	 */
	bool confirmInstanceDirPath(const QString& rawDir,
								const QString& cookedDir);

	/// The additional folders currently listed, in order.
	QStringList additionalInstanceDirs() const;

  private slots:
	void on_instDirBrowseBtn_clicked();
	void on_addInstDirBtn_clicked();
	void on_removeInstDirBtn_clicked();
	void on_modsDirBrowseBtn_clicked();
	void on_iconsDirBrowseBtn_clicked();
	void on_skinsDirBrowseBtn_clicked();
	void on_javaDirBrowseBtn_clicked();
	void on_migrateDataFolderMacBtn_clicked();

	/*!
	 * Updates the list of update channels in the combo box.
	 */
	void refreshUpdateChannelList();

	/*!
	 * Updates the font preview
	 */
	void refreshFontPreview();


  private:
	Ui::KitePage* ui;

	// default format for the font preview...
	QTextCharFormat* defaultFormat;

	std::unique_ptr<LogColorCache> m_colors;

	std::shared_ptr<TranslationsModel> m_languageModel;
};
