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

#include "UpdateAvailableDialog.h"
#include "ui_UpdateAvailableDialog.h"

#include <QIcon>
#include <QPushButton>

#include "BuildConfig.h"
#include "HoeDown.h"
#include "MMCStrings.h"

UpdateAvailableDialog::UpdateAvailableDialog(const QString& currentVersion,
											 const QString& availableVersion,
											 const QString& releaseNotes,
											 QWidget* parent)
	: QDialog(parent), ui(new Ui::UpdateAvailableDialog)
{
	ui->setupUi(this);

	ui->headerLabel->setText(tr("A new version of %1 is available!")
								 .arg(BuildConfig.KITE_DISPLAYNAME));
	ui->versionAvailableLabel->setText(
		tr("Version %1 is now available - you have %2 . Would you like to "
		   "download it now?")
			.arg(availableVersion, currentVersion));
	ui->icon->setPixmap(QIcon::fromTheme("checkupdate").pixmap(64));

	HoeDown markdown;
	ui->releaseNotes->setHtml(
		Strings::htmlListPatch(markdown.process(releaseNotes.toUtf8())));
	ui->releaseNotes->setOpenExternalLinks(true);

	connect(ui->skipButton, &QPushButton::clicked, this,
			[this]() { done(ResultCode::Skip); });
	connect(ui->delayButton, &QPushButton::clicked, this,
			[this]() { done(ResultCode::DontInstall); });
	connect(ui->installButton, &QPushButton::clicked, this,
			[this]() { done(ResultCode::Install); });
}

UpdateAvailableDialog::~UpdateAvailableDialog()
{
	delete ui;
}
