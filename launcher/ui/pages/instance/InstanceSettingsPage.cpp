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

#include "InstanceSettingsPage.h"
#include "ui_InstanceSettingsPage.h"

#include <QFileDialog>
#include <QDialog>
#include <QMessageBox>

#include "Sys.h"

#include "ui/dialogs/VersionSelectDialog.h"
#include "ui/widgets/CustomCommands.h"

#include "ui/JavaCommon.h"
#include "Application.h"

#include "java/JavaInstallList.h"
#include "java/download/JavaRuntime.h"
#include "FileSystem.h"

InstanceSettingsPage::InstanceSettingsPage(BaseInstance* inst, QWidget* parent)
	: QWidget(parent), ui(new Ui::InstanceSettingsPage), m_instance(inst)
{
	m_settings = inst->settings();
	ui->setupUi(this);
	auto sysMB = Sys::getSystemRam() / Sys::mebibyte;
	ui->maxMemSpinBox->setMaximum(sysMB);

	// Populate vendor combobox
	auto providers = JavaDownload::JavaProviderInfo::availableProviders();
	for (const auto& provider : providers) {
		ui->javaAutoDownloadVendorBox->addItem(provider.name, provider.uid);
	}

	connect(ui->openGlobalJavaSettingsButton, &QCommandLinkButton::clicked,
			this, &InstanceSettingsPage::globalSettingsButtonClicked);
	connect(APPLICATION, &Application::globalSettingsAboutToOpen, this,
			&InstanceSettingsPage::applySettings);
	connect(APPLICATION, &Application::globalSettingsClosed, this,
			&InstanceSettingsPage::loadSettings);
	emit APPLICATION->instanceSettingsPageCreated(this, m_instance);
	loadSettings();
}

bool InstanceSettingsPage::shouldDisplay() const
{
	return !m_instance->isRunning();
}

InstanceSettingsPage::~InstanceSettingsPage()
{
	delete ui;
}

void InstanceSettingsPage::globalSettingsButtonClicked(bool)
{
	switch (ui->settingsTabs->currentIndex()) {
		case 0:
			APPLICATION->ShowGlobalSettings(this, "java-settings");
			return;
		case 1:
			APPLICATION->ShowGlobalSettings(this, "minecraft-settings");
			return;
		case 2:
			APPLICATION->ShowGlobalSettings(this, "custom-commands");
			return;
	}
}

bool InstanceSettingsPage::apply()
{
	applySettings();
	return true;
}

void InstanceSettingsPage::applySettings()
{
	SettingsObject::Lock lock(m_settings);

	// Console
	bool console = ui->consoleSettingsBox->isChecked();
	m_settings->set("OverrideConsole", console);
	if (console) {
		m_settings->set("ShowConsole", ui->showConsoleCheck->isChecked());
		m_settings->set("AutoCloseConsole",
						ui->autoCloseConsoleCheck->isChecked());
		m_settings->set("ShowConsoleOnError",
						ui->showConsoleErrorCheck->isChecked());
	} else {
		m_settings->reset("ShowConsole");
		m_settings->reset("AutoCloseConsole");
		m_settings->reset("ShowConsoleOnError");
	}

	// Window Size
	bool window = ui->windowSizeGroupBox->isChecked();
	m_settings->set("OverrideWindow", window);
	if (window) {
		m_settings->set("LaunchMaximized", ui->maximizedCheckBox->isChecked());
		m_settings->set("MinecraftWinWidth", ui->windowWidthSpinBox->value());
		m_settings->set("MinecraftWinHeight", ui->windowHeightSpinBox->value());
	} else {
		m_settings->reset("LaunchMaximized");
		m_settings->reset("MinecraftWinWidth");
		m_settings->reset("MinecraftWinHeight");
	}

	// Memory
	bool memory = ui->memoryGroupBox->isChecked();
	m_settings->set("OverrideMemory", memory);
	if (memory) {
		int min = ui->minMemSpinBox->value();
		int max = ui->maxMemSpinBox->value();
		if (min < max) {
			m_settings->set("MinMemAlloc", min);
			m_settings->set("MaxMemAlloc", max);
		} else {
			m_settings->set("MinMemAlloc", max);
			m_settings->set("MaxMemAlloc", min);
		}
		m_settings->set("PermGen", ui->permGenSpinBox->value());
	} else {
		m_settings->reset("MinMemAlloc");
		m_settings->reset("MaxMemAlloc");
		m_settings->reset("PermGen");
	}

	// Java Install Settings
	bool javaInstall = ui->javaSettingsGroupBox->isChecked();
	m_settings->set("OverrideJavaLocation", javaInstall);
	if (javaInstall) {
		m_settings->set("JavaPath", ui->javaPathTextBox->text());
	} else {
		m_settings->reset("JavaPath");
	}

	// Java arguments
	bool javaArgs = ui->javaArgumentsGroupBox->isChecked();
	m_settings->set("OverrideJavaArgs", javaArgs);
	if (javaArgs) {
		m_settings->set("JvmArgs",
						ui->jvmArgsTextBox->toPlainText().replace("\n", " "));
		JavaCommon::checkJVMArgs(m_settings->get("JvmArgs").toString(),
								 this->parentWidget());
	} else {
		m_settings->reset("JvmArgs");
	}

	// old generic 'override both' is removed.
	m_settings->reset("OverrideJava");

	// Java auto-download vendor
	bool vendorOverride = ui->javaVendorGroupBox->isChecked();
	m_settings->set("OverrideJavaAutoDownloadVendor", vendorOverride);
	if (vendorOverride) {
		m_settings->set(
			"JavaAutoDownloadVendor",
			ui->javaAutoDownloadVendorBox->currentData().toString());
	} else {
		m_settings->reset("JavaAutoDownloadVendor");
	}

	// Custom Commands
	bool custcmd = ui->customCommands->checked();
	m_settings->set("OverrideCommands", custcmd);
	if (custcmd) {
		m_settings->set("PreLaunchCommand",
						ui->customCommands->prelaunchCommand());
		m_settings->set("WrapperCommand", ui->customCommands->wrapperCommand());
		m_settings->set("PostExitCommand",
						ui->customCommands->postexitCommand());
	} else {
		m_settings->reset("PreLaunchCommand");
		m_settings->reset("WrapperCommand");
		m_settings->reset("PostExitCommand");
	}

	// Workarounds
	bool workarounds = ui->nativeWorkaroundsGroupBox->isChecked();
	m_settings->set("OverrideNativeWorkarounds", workarounds);
	if (workarounds) {
		m_settings->set("UseNativeOpenAL",
						ui->useNativeOpenALCheck->isChecked());
		m_settings->set("UseNativeGLFW", ui->useNativeGLFWCheck->isChecked());
	} else {
		m_settings->reset("UseNativeOpenAL");
		m_settings->reset("UseNativeGLFW");
	}

	// Game time
	bool gameTime = ui->gameTimeGroupBox->isChecked();
	m_settings->set("OverrideGameTime", gameTime);
	if (gameTime) {
		m_settings->set("ShowGameTime", ui->showGameTime->isChecked());
		m_settings->set("RecordGameTime", ui->recordGameTime->isChecked());
	} else {
		m_settings->reset("ShowGameTime");
		m_settings->reset("RecordGameTime");
	}

	// Join server on launch
	bool joinServerOnLaunch = ui->serverJoinGroupBox->isChecked();
	m_settings->set("JoinServerOnLaunch", joinServerOnLaunch);
	if (joinServerOnLaunch) {
		m_settings->set("JoinServerOnLaunchAddress",
						ui->serverJoinAddress->text());
	} else {
		m_settings->reset("JoinServerOnLaunchAddress");
	}

	// Global data packs folder
	bool globalDataPacks = ui->globalDataPacksGroupBox->isChecked();
	m_settings->set("GlobalDataPacksEnabled", globalDataPacks);
	if (globalDataPacks) {
		m_settings->set("GlobalDataPacksPath",
						ui->globalDataPacksPath->text().trimmed());
	} else {
		m_settings->reset("GlobalDataPacksPath");
	}

	emit settingsAboutToApply();
}

void InstanceSettingsPage::loadSettings()
{
	// Console
	ui->consoleSettingsBox->setChecked(
		m_settings->get("OverrideConsole").toBool());
	ui->showConsoleCheck->setChecked(m_settings->get("ShowConsole").toBool());
	ui->autoCloseConsoleCheck->setChecked(
		m_settings->get("AutoCloseConsole").toBool());
	ui->showConsoleErrorCheck->setChecked(
		m_settings->get("ShowConsoleOnError").toBool());

	// Window Size
	ui->windowSizeGroupBox->setChecked(
		m_settings->get("OverrideWindow").toBool());
	ui->maximizedCheckBox->setChecked(
		m_settings->get("LaunchMaximized").toBool());
	ui->windowWidthSpinBox->setValue(
		m_settings->get("MinecraftWinWidth").toInt());
	ui->windowHeightSpinBox->setValue(
		m_settings->get("MinecraftWinHeight").toInt());

	// Memory
	ui->memoryGroupBox->setChecked(m_settings->get("OverrideMemory").toBool());
	int min = m_settings->get("MinMemAlloc").toInt();
	int max = m_settings->get("MaxMemAlloc").toInt();
	if (min < max) {
		ui->minMemSpinBox->setValue(min);
		ui->maxMemSpinBox->setValue(max);
	} else {
		ui->minMemSpinBox->setValue(max);
		ui->maxMemSpinBox->setValue(min);
	}
	ui->permGenSpinBox->setValue(m_settings->get("PermGen").toInt());
	bool permGenVisible = m_settings->get("PermGenVisible").toBool();
	ui->permGenSpinBox->setVisible(permGenVisible);
	ui->labelPermGen->setVisible(permGenVisible);
	ui->labelPermgenNote->setVisible(permGenVisible);

	// Java Settings
	bool overrideJava = m_settings->get("OverrideJava").toBool();
	bool overrideLocation =
		m_settings->get("OverrideJavaLocation").toBool() || overrideJava;
	bool overrideArgs =
		m_settings->get("OverrideJavaArgs").toBool() || overrideJava;

	ui->javaSettingsGroupBox->setChecked(overrideLocation);
	ui->javaPathTextBox->setText(m_settings->get("JavaPath").toString());

	ui->javaArgumentsGroupBox->setChecked(overrideArgs);
	ui->jvmArgsTextBox->setPlainText(m_settings->get("JvmArgs").toString());

	// Java auto-download vendor
	ui->javaVendorGroupBox->setChecked(
		m_settings->get("OverrideJavaAutoDownloadVendor").toBool());
	auto currentVendor = m_settings->get("JavaAutoDownloadVendor").toString();
	int vendorIdx = ui->javaAutoDownloadVendorBox->findData(currentVendor);
	if (vendorIdx >= 0) {
		ui->javaAutoDownloadVendorBox->setCurrentIndex(vendorIdx);
	}

	// Custom commands
	ui->customCommands->initialize(
		true, m_settings->get("OverrideCommands").toBool(),
		m_settings->get("PreLaunchCommand").toString(),
		m_settings->get("WrapperCommand").toString(),
		m_settings->get("PostExitCommand").toString());

	// Workarounds
	ui->nativeWorkaroundsGroupBox->setChecked(
		m_settings->get("OverrideNativeWorkarounds").toBool());
	ui->useNativeGLFWCheck->setChecked(
		m_settings->get("UseNativeGLFW").toBool());
	ui->useNativeOpenALCheck->setChecked(
		m_settings->get("UseNativeOpenAL").toBool());

	// Miscellanous
	ui->gameTimeGroupBox->setChecked(
		m_settings->get("OverrideGameTime").toBool());
	ui->showGameTime->setChecked(m_settings->get("ShowGameTime").toBool());
	ui->recordGameTime->setChecked(m_settings->get("RecordGameTime").toBool());

	ui->serverJoinGroupBox->setChecked(
		m_settings->get("JoinServerOnLaunch").toBool());
	ui->serverJoinAddress->setText(
		m_settings->get("JoinServerOnLaunchAddress").toString());

	ui->globalDataPacksGroupBox->setChecked(
		m_settings->get("GlobalDataPacksEnabled").toBool());
	ui->globalDataPacksPath->setText(
		m_settings->get("GlobalDataPacksPath").toString());

	emit settingsLoaded();
}

void InstanceSettingsPage::on_javaDetectBtn_clicked()
{
	JavaInstallPtr java;

	VersionSelectDialog vselect(APPLICATION->javalist().get(),
								tr("Select a Java version"), this, true);
	vselect.setResizeOn(2);
	vselect.exec();

	if (vselect.result() == QDialog::Accepted && vselect.selectedVersion()) {
		java =
			std::dynamic_pointer_cast<JavaInstall>(vselect.selectedVersion());
		ui->javaPathTextBox->setText(java->path);
		bool visible = java->id.requiresPermGen() &&
					   m_settings->get("OverrideMemory").toBool();
		ui->permGenSpinBox->setVisible(visible);
		ui->labelPermGen->setVisible(visible);
		ui->labelPermgenNote->setVisible(visible);
		m_settings->set("PermGenVisible", visible);
	}
}

void InstanceSettingsPage::on_javaBrowseBtn_clicked()
{
	QString raw_path =
		QFileDialog::getOpenFileName(this, tr("Find Java executable"));

	// do not allow current dir - it's dirty. Do not allow dirs that don't exist
	if (raw_path.isEmpty()) {
		return;
	}
	QString cooked_path = FS::NormalizePath(raw_path);

	QFileInfo javaInfo(cooked_path);
	if (!javaInfo.exists() || !javaInfo.isExecutable()) {
		return;
	}
	ui->javaPathTextBox->setText(cooked_path);

	// custom Java could be anything... enable perm gen option
	ui->permGenSpinBox->setVisible(true);
	ui->labelPermGen->setVisible(true);
	ui->labelPermgenNote->setVisible(true);
	m_settings->set("PermGenVisible", true);
}

void InstanceSettingsPage::on_javaTestBtn_clicked()
{
	if (checker) {
		return;
	}
	checker.reset(new JavaCommon::TestCheck(
		this, ui->javaPathTextBox->text(),
		ui->jvmArgsTextBox->toPlainText().replace("\n", " "),
		ui->minMemSpinBox->value(), ui->maxMemSpinBox->value(),
		ui->permGenSpinBox->value()));
	connect(checker.get(), &JavaCommon::TestCheck::finished, this,
			&InstanceSettingsPage::checkerFinished);
	checker->run();
}

void InstanceSettingsPage::checkerFinished()
{
	checker.reset();
}
