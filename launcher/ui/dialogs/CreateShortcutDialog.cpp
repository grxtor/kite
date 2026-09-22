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

#include "CreateShortcutDialog.h"
#include "ui_CreateShortcutDialog.h"

#include <QPushButton>

#include "Application.h"
#include "DesktopServices.h"
#include "FileSystem.h"
#include "icons/IconList.h"
#include "minecraft/MinecraftInstance.h"
#include "ui/ShortcutUtils.h"
#include "minecraft/World.h"
#include "minecraft/WorldList.h"
#include "minecraft/auth/AccountList.h"
#include "ui/dialogs/IconPickerDialog.h"

namespace
{
	/* The combo box carries its answer as a plain int rather than the
	 * enum itself, so no metatype has to be registered for something only
	 * read back a line later. */
	int asData(ShortcutTarget target)
	{
		return static_cast<int>(target);
	}
} // namespace

CreateShortcutDialog::CreateShortcutDialog(MinecraftInstance* instance,
										   QWidget* parent)
	: QDialog(parent), ui(new Ui::CreateShortcutDialog), m_instance(instance)
{
	ui->setupUi(this);

	m_iconKey = m_instance->iconKey();
	ui->iconButton->setIcon(APPLICATION->icons()->getIcon(m_iconKey));
	ui->instNameTextBox->setPlaceholderText(m_instance->name());

	/* Joining a world from the command line needs a Minecraft new enough
	 * to understand quick play, which the version's traits announce. No
	 * MeshMC instance reports it yet, so in practice the target section
	 * below is server-only and says so instead of offering a radio
	 * button with nothing on the other side of it. */
	m_canJoinWorld =
		m_instance->traits().contains(QStringLiteral(
			"feature:is_quick_play_singleplayer"));

	auto worlds = m_instance->worldList();
	worlds->update();
	if (!m_canJoinWorld || worlds->empty()) {
		m_canJoinWorld = false;
		ui->worldTarget->hide();
		ui->worldSelectionBox->hide();
		ui->serverTarget->setChecked(true);
		ui->serverTarget->hide();
		ui->serverLabel->show();
	} else {
		for (const World& world : worlds->allWorlds()) {
			ui->worldSelectionBox->addItem(
				tr("%1 [%2] - Last Played: %3")
					.arg(world.name(), world.gameType().toTranslatedString(),
						 world.lastPlayed().toString(Qt::ISODate)),
				world.folderName());
		}
	}

	/* Inside a Flatpak sandbox neither the desktop nor the applications
	 * folder is ours to write to; the file dialog behind "Other..." goes
	 * through the portal and is the only route out. */
	if (!DesktopServices::isFlatpak()) {
		if (!FS::getDesktopDir().isEmpty()) {
			ui->saveTargetSelectionBox->addItem(
				tr("Desktop"), asData(ShortcutTarget::Desktop));
		}
		if (!FS::getApplicationsDir().isEmpty()) {
			ui->saveTargetSelectionBox->addItem(
				tr("Applications"), asData(ShortcutTarget::Applications));
		}
	}
	ui->saveTargetSelectionBox->addItem(tr("Other..."),
										asData(ShortcutTarget::Other));

	auto accounts = APPLICATION->accounts();
	if (accounts->count() <= 0) {
		ui->overrideAccountCheckbox->setEnabled(false);
	} else {
		MinecraftAccountPtr defaultAccount = accounts->defaultAccount();
		for (int i = 0; i < accounts->count(); i++) {
			MinecraftAccountPtr account = accounts->at(i);

			QString label = account->profileName();
			if (account->isInUse()) {
				label = tr("%1 (in use)").arg(label);
			}

			const QPixmap face = account->getFace();
			ui->accountSelectionBox->addItem(
				face.isNull() ? APPLICATION->getThemedIcon("noaccount")
							  : QIcon(face),
				label, account->profileName());

			if (defaultAccount == account) {
				ui->accountSelectionBox->setCurrentIndex(i);
			}
		}
	}

	refresh();
}

CreateShortcutDialog::~CreateShortcutDialog()
{
	delete ui;
}

void CreateShortcutDialog::on_iconButton_clicked()
{
	IconPickerDialog picker(this);
	if (picker.execWithSelection(m_iconKey) != QDialog::Accepted) {
		return;
	}

	m_iconKey = picker.selectedIconKey;
	ui->iconButton->setIcon(APPLICATION->icons()->getIcon(m_iconKey));
}

void CreateShortcutDialog::on_overrideAccountCheckbox_toggled(bool checked)
{
	ui->accountOptionsGroup->setEnabled(checked);
}

void CreateShortcutDialog::on_targetCheckbox_toggled(bool checked)
{
	ui->targetOptionsGroup->setEnabled(checked);
	refresh();
}

void CreateShortcutDialog::on_worldTarget_toggled(bool checked)
{
	ui->worldSelectionBox->setEnabled(checked);
	refresh();
}

void CreateShortcutDialog::on_serverTarget_toggled(bool checked)
{
	ui->serverAddressBox->setEnabled(checked);
	refresh();
}

void CreateShortcutDialog::on_worldSelectionBox_currentIndexChanged(int)
{
	refresh();
}

void CreateShortcutDialog::on_serverAddressBox_textChanged(const QString&)
{
	refresh();
}

void CreateShortcutDialog::refresh()
{
	/* The name field is left empty and only shows what it would be
	 * called, so that a name the user has not touched keeps following
	 * the target they are still picking. */
	QString suggestion = m_instance->name();
	bool complete = true;

	if (ui->targetCheckbox->isChecked()) {
		if (ui->worldTarget->isChecked()) {
			suggestion = tr("%1 - %2").arg(
				suggestion, ui->worldSelectionBox->currentData().toString());
			complete = ui->worldSelectionBox->currentIndex() != -1;
		} else if (ui->serverTarget->isChecked()) {
			suggestion = tr("%1 - Server %2")
							 .arg(suggestion, ui->serverAddressBox->text());
			complete = !ui->serverAddressBox->text().isEmpty();
		} else {
			complete = false;
		}
	}

	ui->instNameTextBox->setPlaceholderText(suggestion);
	ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(complete);
}

void CreateShortcutDialog::createShortcut()
{
	ShortcutUtils::Shortcut shortcut;
	shortcut.instance = m_instance;
	shortcut.parent = this;
	shortcut.iconKey = m_iconKey;
	shortcut.targetString = tr("instance");
	shortcut.target = static_cast<ShortcutTarget>(
		ui->saveTargetSelectionBox->currentData().toInt());

	if (ui->targetCheckbox->isChecked()) {
		if (ui->worldTarget->isChecked()) {
			shortcut.targetString = tr("world");
			shortcut.extraArgs
				<< QStringLiteral("--world")
				<< ui->worldSelectionBox->currentData().toString();
		} else if (ui->serverTarget->isChecked()) {
			shortcut.targetString = tr("server");
			shortcut.extraArgs << QStringLiteral("--server")
							   << ui->serverAddressBox->text();
		}
	}

	if (ui->overrideAccountCheckbox->isChecked()) {
		shortcut.extraArgs
			<< QStringLiteral("--profile")
			<< ui->accountSelectionBox->currentData().toString();
	}

	shortcut.name = ui->instNameTextBox->text();
	if (shortcut.name.isEmpty()) {
		shortcut.name = ui->instNameTextBox->placeholderText();
	}

	switch (shortcut.target) {
		case ShortcutTarget::Desktop:
			ShortcutUtils::createInstanceShortcutOnDesktop(shortcut);
			break;
		case ShortcutTarget::Applications:
			ShortcutUtils::createInstanceShortcutInApplications(shortcut);
			break;
		case ShortcutTarget::Other:
			ShortcutUtils::createInstanceShortcutInOther(shortcut);
			break;
	}
}
