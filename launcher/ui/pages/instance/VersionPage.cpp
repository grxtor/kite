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

#include "Application.h"

#include <QMessageBox>
#include <QLabel>
#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QAbstractItemModel>
#include <QMessageBox>
#include <QListView>
#include <QString>
#include <QUrl>

#include "VersionPage.h"
#include "ui_VersionPage.h"

#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/InstallLoaderDialog.h"
#include "ui/dialogs/VersionSelectDialog.h"
#include "ui/dialogs/NewComponentDialog.h"
#include "ui/dialogs/ProgressDialog.h"

#include "ui/GuiUtil.h"

#include "minecraft/PackProfile.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/mod/Mod.h"
#include "icons/IconList.h"
#include "Exception.h"
#include "Version.h"
#include "DesktopServices.h"

#include "meta/Index.h"
#include "meta/VersionList.h"

class IconProxy : public QIdentityProxyModel
{
	Q_OBJECT
  public:
	IconProxy(QWidget* parentWidget) : QIdentityProxyModel(parentWidget)
	{
		connect(parentWidget, &QObject::destroyed, this,
				&IconProxy::widgetGone);
		m_parentWidget = parentWidget;
	}

	virtual QVariant data(const QModelIndex& proxyIndex,
						  int role = Qt::DisplayRole) const override
	{
		QVariant var = QIdentityProxyModel::data(proxyIndex, role);
		int column = proxyIndex.column();
		if (column == 0 && role == Qt::DecorationRole && m_parentWidget) {
			if (!var.isNull()) {
				auto string = var.toString();
				if (string == "warning") {
					return APPLICATION->getThemedIcon("status-yellow");
				} else if (string == "error") {
					return APPLICATION->getThemedIcon("status-bad");
				}
			}
			return APPLICATION->getThemedIcon("status-good");
		}
		return var;
	}
  private slots:
	void widgetGone()
	{
		m_parentWidget = nullptr;
	}

  private:
	QWidget* m_parentWidget = nullptr;
};

QIcon VersionPage::icon() const
{
	return APPLICATION->icons()->getIcon(m_inst->iconKey());
}
bool VersionPage::shouldDisplay() const
{
	return true;
}

QMenu* VersionPage::createPopupMenu()
{
	QMenu* filteredMenu = QMainWindow::createPopupMenu();
	filteredMenu->removeAction(ui->toolBar->toggleViewAction());
	return filteredMenu;
}

VersionPage::VersionPage(MinecraftInstance* inst, QWidget* parent)
	: QMainWindow(parent), ui(new Ui::VersionPage), m_inst(inst)
{
	ui->setupUi(this);

	ui->toolBar->insertSpacer(ui->actionReload);

	m_profile = m_inst->getPackProfile();

	reloadPackProfile();

	auto proxy = new IconProxy(ui->packageView);
	proxy->setSourceModel(m_profile.get());

	m_filterModel = new QSortFilterProxyModel();
	m_filterModel->setDynamicSortFilter(true);
	m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	m_filterModel->setSortCaseSensitivity(Qt::CaseInsensitive);
	m_filterModel->setSourceModel(proxy);
	m_filterModel->setFilterKeyColumn(-1);

	ui->packageView->setModel(m_filterModel);
	ui->packageView->installEventFilter(this);
	ui->packageView->setSelectionMode(QAbstractItemView::SingleSelection);
	ui->packageView->setContextMenuPolicy(Qt::CustomContextMenu);

	connect(ui->packageView->selectionModel(),
			&QItemSelectionModel::currentChanged, this,
			&VersionPage::versionCurrent);
	auto smodel = ui->packageView->selectionModel();
	connect(smodel, &QItemSelectionModel::currentChanged, this,
			&VersionPage::packageCurrent);

	connect(m_profile.get(), &PackProfile::minecraftChanged, this,
			&VersionPage::updateVersionControls);
	controlsEnabled = !m_inst->isRunning();
	updateVersionControls();
	preselect(0);
	connect(m_inst, &BaseInstance::runningStatusChanged, this,
			&VersionPage::updateRunningStatus);
	connect(ui->packageView, &ModListView::customContextMenuRequested, this,
			&VersionPage::showContextMenu);
	connect(ui->filterEdit, &QLineEdit::textChanged, this,
			&VersionPage::onFilterTextChanged);
}

VersionPage::~VersionPage()
{
	delete ui;
}

void VersionPage::showContextMenu(const QPoint& pos)
{
	auto menu = ui->toolBar->createContextMenu(this, tr("Context menu"));
	menu->exec(ui->packageView->mapToGlobal(pos));
	delete menu;
}

void VersionPage::packageCurrent(const QModelIndex& current,
								 const QModelIndex& previous)
{
	if (!current.isValid()) {
		ui->frame->clear();
		return;
	}
	int row = current.row();
	auto patch = m_profile->getComponent(row);
	auto severity = patch->getProblemSeverity();
	switch (severity) {
		case ProblemSeverity::Warning:
			ui->frame->setModText(
				tr("%1 possibly has issues.").arg(patch->getName()));
			break;
		case ProblemSeverity::Error:
			ui->frame->setModText(tr("%1 has issues!").arg(patch->getName()));
			break;
		default:
		case ProblemSeverity::None:
			ui->frame->clear();
			return;
	}

	auto& problems = patch->getProblems();
	QString problemOut;
	for (auto& problem : problems) {
		if (problem.m_severity == ProblemSeverity::Error) {
			problemOut += tr("Error: ");
		} else if (problem.m_severity == ProblemSeverity::Warning) {
			problemOut += tr("Warning: ");
		}
		problemOut += problem.m_description;
		problemOut += "\n";
	}
	ui->frame->setModDescription(problemOut);
}

void VersionPage::updateRunningStatus(bool running)
{
	if (controlsEnabled == running) {
		controlsEnabled = !running;
		updateVersionControls();
	}
}

/* There is nothing version-specific left to decide here.
 *
 * This used to enable or disable one toolbar button per loader against
 * hardcoded Minecraft version arithmetic - and admitted as much with a
 * "dirty hack" comment. The install dialog now answers the same question
 * from the metadata, per page, so a loader that has nothing to offer for
 * this Minecraft version shows an empty list with a reason instead of a
 * greyed-out button with none. The function stays because
 * PackProfile::minecraftChanged is wired to it. */
void VersionPage::updateVersionControls()
{
	updateButtons();
}

void VersionPage::updateButtons(int row)
{
	if (row == -1)
		row = currentRow();
	auto patch = m_profile->getComponent(row);
	ui->actionRemove->setEnabled(controlsEnabled && patch &&
								 patch->isRemovable());
	ui->actionMove_down->setEnabled(controlsEnabled && patch &&
									patch->isMoveable());
	ui->actionMove_up->setEnabled(controlsEnabled && patch &&
								  patch->isMoveable());
	ui->actionChange_version->setEnabled(controlsEnabled && patch &&
										 patch->isVersionChangeable());
	ui->actionEdit->setEnabled(controlsEnabled && patch && patch->isCustom());
	ui->actionCustomize->setEnabled(controlsEnabled && patch &&
									patch->isCustomizable());
	ui->actionRevert->setEnabled(controlsEnabled && patch &&
								 patch->isRevertible());
	ui->actionDownload_All->setEnabled(controlsEnabled);
	ui->actionAdd_Empty->setEnabled(controlsEnabled);
	ui->actionReload->setEnabled(controlsEnabled);
	/* Listed here with the rest so it follows the running state. The old
	 * per-loader buttons were set from updateVersionControls() instead,
	 * and Forge's was missed entirely - it stayed clickable while the
	 * instance was running, unlike every other control on this page. */
	ui->actionInstall_Loader->setEnabled(controlsEnabled);
	ui->actionInstall_mods->setEnabled(controlsEnabled);
	ui->actionReplace_Minecraft_jar->setEnabled(controlsEnabled);
	ui->actionAdd_to_Minecraft_jar->setEnabled(controlsEnabled);
}

bool VersionPage::reloadPackProfile()
{
	try {
		m_profile->reload(Net::Mode::Online);
		return true;
	} catch (const Exception& e) {
		QMessageBox::critical(this, tr("Error"), e.cause());
		return false;
	} catch (...) {
		QMessageBox::critical(this, tr("Error"),
							  tr("Couldn't load the instance profile."));
		return false;
	}
}

void VersionPage::on_actionReload_triggered()
{
	reloadPackProfile();
	m_container->refreshContainer();
}

void VersionPage::on_actionRemove_triggered()
{
	if (ui->packageView->currentIndex().isValid()) {
		// FIXME: use actual model, not reloading.
		if (!m_profile->remove(ui->packageView->currentIndex().row())) {
			QMessageBox::critical(this, tr("Error"),
								  tr("Couldn't remove file"));
		}
	}
	updateButtons();
	reloadPackProfile();
	m_container->refreshContainer();
}

void VersionPage::on_actionInstall_mods_triggered()
{
	if (m_container) {
		m_container->selectPage("mods");
	}
}

void VersionPage::on_actionAdd_to_Minecraft_jar_triggered()
{
	auto list = GuiUtil::BrowseForFiles(
		"jarmod", tr("Select jar mods"), tr("Minecraft.jar mods (*.zip *.jar)"),
		APPLICATION->settings()->get("CentralModsDir").toString(),
		this->parentWidget());
	if (!list.empty()) {
		m_profile->installJarMods(list);
	}
	updateButtons();
}

void VersionPage::on_actionReplace_Minecraft_jar_triggered()
{
	auto jarPath = GuiUtil::BrowseForFile(
		"jar", tr("Select jar"), tr("Minecraft.jar replacement (*.jar)"),
		APPLICATION->settings()->get("CentralModsDir").toString(),
		this->parentWidget());
	if (!jarPath.isEmpty()) {
		m_profile->installCustomJar(jarPath);
	}
	updateButtons();
}

void VersionPage::on_actionMove_up_triggered()
{
	try {
		m_profile->move(currentRow(), PackProfile::MoveUp);
	} catch (const Exception& e) {
		QMessageBox::critical(this, tr("Error"), e.cause());
	}
	updateButtons();
}

void VersionPage::on_actionMove_down_triggered()
{
	try {
		m_profile->move(currentRow(), PackProfile::MoveDown);
	} catch (const Exception& e) {
		QMessageBox::critical(this, tr("Error"), e.cause());
	}
	updateButtons();
}

void VersionPage::on_actionChange_version_triggered()
{
	auto versionRow = currentRow();
	if (versionRow == -1) {
		return;
	}
	auto patch = m_profile->getComponent(versionRow);
	auto name = patch->getName();
	auto list = patch->getVersionList();
	if (!list) {
		return;
	}
	auto uid = list->uid();

	/* Changing a loader's version is the same act as installing it, so
	 * it goes through the same dialog, opened on that loader's page.
	 * It also means the conflict handling is not something the user can
	 * step around by choosing "change version" instead of "install".
	 *
	 * What used to be here was a hardcoded redirect to the per-loader
	 * install handlers for three of the five uids, under a comment
	 * calling itself a horrible hack. The loader table answers the
	 * question now, so there is nothing left to hardcode. */
	if (modLoaderForUid(uid) != nullptr) {
		InstallLoaderDialog dialog(m_profile.get(), uid, this);
		dialog.exec();
		m_container->refreshContainer();
		return;
	}

	VersionSelectDialog vselect(list.get(), tr("Change %1 version").arg(name),
								this);
	if (uid == "net.fabricmc.intermediary" || uid == "org.quiltmc.hashed") {
		vselect.setEmptyString(
			tr("No intermediary mappings versions are currently available."));
		vselect.setEmptyErrorString(tr("Couldn't load or download the "
									   "intermediary mappings version lists!"));
	}

	/* Applied to every component, not just the mappings: anything whose
	 * metadata pins it to a Minecraft version should only offer builds
	 * for the one installed. "If present" leaves components that are not
	 * pinned - LWJGL, say - showing their full list rather than none. */
	vselect.setExactIfPresentFilter(
		BaseVersionList::ParentVersionRole,
		m_profile->getComponentVersion("net.minecraft"));

	auto currentVersion = patch->getVersion();
	if (!currentVersion.isEmpty()) {
		vselect.setCurrentVersion(currentVersion);
	}
	if (!vselect.exec() || !vselect.selectedVersion())
		return;

	qDebug() << "Change" << uid << "to"
			 << vselect.selectedVersion()->descriptor();
	bool important = false;
	if (uid == "net.minecraft") {
		important = true;
	}
	m_profile->setComponentVersion(uid, vselect.selectedVersion()->descriptor(),
								   important);
	m_profile->resolve(Net::Mode::Online);
	m_container->refreshContainer();
}

void VersionPage::on_actionDownload_All_triggered()
{
	if (!APPLICATION->accounts()->anyAccountIsValid()) {
		CustomMessageBox::selectable(
			this, tr("Error"),
			tr("Kite cannot download Minecraft or update instances unless "
			   "you have at least "
			   "one account added.\nPlease add your Mojang or Minecraft "
			   "account."),
			QMessageBox::Warning)
			->show();
		return;
	}

	auto updateTask = m_inst->createUpdateTask(Net::Mode::Online);
	if (!updateTask) {
		return;
	}
	ProgressDialog tDialog(this);
	connect(updateTask.get(), &Task::failed, this,
			&VersionPage::onGameUpdateError);
	// FIXME: unused return value
	tDialog.execWithTask(updateTask.get());
	updateButtons();
	m_container->refreshContainer();
}

void VersionPage::on_actionInstall_Loader_triggered()
{
	/* No page preselected: arriving from the toolbar there is no loader
	 * in mind yet, so the dialog opens on its own first page. Arriving
	 * from "change version" on an installed loader is the other entry
	 * point, and that one does pass a uid. */
	InstallLoaderDialog dialog(m_profile.get(), QString(), this);
	dialog.exec();

	/* The dialog writes into the profile itself, so there is nothing to
	 * read back - but a component may have been added, removed or
	 * disabled, and both the row selection and the sidebar have to catch
	 * up with that. Reselecting the current row rather than the last one
	 * matters because "uninstall it" on a conflict can have shortened
	 * the list underneath us. */
	preselect(currentRow());
	m_container->refreshContainer();
}

void VersionPage::on_actionAdd_Empty_triggered()
{
	NewComponentDialog compdialog(QString(), QString(), this);
	QStringList blacklist;
	for (int i = 0; i < m_profile->rowCount(); i++) {
		auto comp = m_profile->getComponent(i);
		blacklist.push_back(comp->getID());
	}
	compdialog.setBlacklist(blacklist);
	if (compdialog.exec()) {
		qDebug() << "name:" << compdialog.name();
		qDebug() << "uid:" << compdialog.uid();
		m_profile->installEmpty(compdialog.uid(), compdialog.name());
	}
}

void VersionPage::on_actionLibrariesFolder_triggered()
{
	DesktopServices::openDirectory(m_inst->getLocalLibraryPath(), true);
}

void VersionPage::on_actionMinecraftFolder_triggered()
{
	DesktopServices::openDirectory(m_inst->gameRoot(), true);
}

void VersionPage::versionCurrent(const QModelIndex& current,
								 const QModelIndex& previous)
{
	currentIdx = current.row();
	updateButtons(currentIdx);
}

void VersionPage::preselect(int row)
{
	if (row < 0) {
		row = 0;
	}
	if (row >= m_profile->rowCount(QModelIndex())) {
		row = m_profile->rowCount(QModelIndex()) - 1;
	}
	if (row < 0) {
		return;
	}
	auto model_index = m_profile->index(row);
	ui->packageView->selectionModel()->select(
		model_index,
		QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
	updateButtons(row);
}

void VersionPage::onGameUpdateError(QString error)
{
	CustomMessageBox::selectable(this, tr("Error updating instance"), error,
								 QMessageBox::Warning)
		->show();
}

Component* VersionPage::current()
{
	auto row = currentRow();
	if (row < 0) {
		return nullptr;
	}
	return m_profile->getComponent(row);
}

int VersionPage::currentRow()
{
	if (ui->packageView->selectionModel()->selectedRows().isEmpty()) {
		return -1;
	}
	return ui->packageView->selectionModel()->selectedRows().first().row();
}

void VersionPage::on_actionCustomize_triggered()
{
	auto version = currentRow();
	if (version == -1) {
		return;
	}
	auto patch = m_profile->getComponent(version);
	if (!patch->getVersionFile()) {
		// TODO: wait for the update task to finish here...
		return;
	}
	if (!m_profile->customize(version)) {
		// TODO: some error box here
	}
	updateButtons();
	preselect(currentIdx);
}

void VersionPage::on_actionEdit_triggered()
{
	auto version = current();
	if (!version) {
		return;
	}
	auto filename = version->getFilename();
	if (!QFileInfo::exists(filename)) {
		qWarning() << "file" << filename
				   << "can't be opened for editing, doesn't exist!";
		return;
	}
	APPLICATION->openJsonEditor(filename);
}

void VersionPage::on_actionRevert_triggered()
{
	auto version = currentRow();
	if (version == -1) {
		return;
	}
	if (!m_profile->revertToBase(version)) {
		// TODO: some error box here
	}
	updateButtons();
	preselect(currentIdx);
	m_container->refreshContainer();
}

void VersionPage::onFilterTextChanged(const QString& newContents)
{
	m_filterModel->setFilterFixedString(newContents);
}

#include "VersionPage.moc"
