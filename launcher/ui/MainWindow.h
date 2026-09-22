/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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

#include <QMainWindow>
#include <QPointer>
#include <QProcess>
#include <QTimer>

#include "BaseInstance.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "net/NetJob.h"

namespace Ui
{
class MainWindow;
}

class LaunchController;
class NewsChecker;
class NewsViewerDialog;
class NotificationChecker;
class QToolButton;
class QAction;
class QActionGroup;
class InstanceProxyModel;
class LabeledToolButton;
class QLabel;
class MinecraftLauncher;
class BaseProfilerFactory;
class InstanceView;
class KonamiCode;
class InstanceTask;

class MainWindow : public QMainWindow
{
	Q_OBJECT

  public:
	explicit MainWindow(QWidget* parent = 0);
	~MainWindow();

	bool eventFilter(QObject* obj, QEvent* ev) override;
	void closeEvent(QCloseEvent* event) override;
	void changeEvent(QEvent* event) override;
	/// Re-applies the menu bar / main toolbar choice, which
	/// QMainWindow::restoreState() would otherwise overrule -- it runs
	/// after the constructor. See the implementation.
	void showEvent(QShowEvent* event) override;
#ifndef Q_OS_MACOS
	/// Tapping Alt shows the menu bar for as long as it is wanted, for
	/// windows that keep the main toolbar instead. Not on macOS, where
	/// the menu bar is native and always there.
	void keyReleaseEvent(QKeyEvent* event) override;
#endif

	void checkInstancePathForProblems();

	void updatesAllowedChanged(bool allowed);

	void droppedURLs(QList<QUrl> urls);

	/**
	 * Runs the "create a new instance" dialog and, if accepted, the task
	 * that installs it - the same flow addInstance() runs for the widget
	 * menu, extracted so the QML shell can start it without a MainWindow
	 * to call it on. @p parent may be nullptr.
	 */
	static void createInstanceFromDialog(QWidget* parent,
										 const QString& groupName,
										 const QString& url = QString());

	NewsChecker* newsChecker() const
	{
		return m_newsChecker.get();
	}
  signals:
	void isClosing();

  protected:
	QMenu* createPopupMenu() override;

  private slots:
	void onCatToggled(bool);

	void on_actionAbout_triggered();

	void on_actionPlugins_triggered();

	void on_actionMeshMCLogs_triggered();

	void on_actionAddInstance_triggered();

	void on_actionREDDIT_triggered();

	void on_actionDISCORD_triggered();

	void on_actionPatreon_triggered();

	void on_actionCopyInstance_triggered();

	void on_actionChangeInstGroup_triggered();

	void on_actionChangeInstIcon_triggered();
	void on_changeIconButton_clicked(bool)
	{
		on_actionChangeInstIcon_triggered();
	}

	void on_actionViewInstanceFolder_triggered();

	void on_actionConfig_Folder_triggered();

	void on_actionViewSelectedInstFolder_triggered();

	void on_actionViewSelectedMCFolder_triggered();

	void on_actionViewSelectedModsFolder_triggered();

	void refreshInstances();

	void on_actionViewCentralModsFolder_triggered();

	void on_actionViewLauncherRootFolder_triggered();

	void on_actionViewIconThemeFolder_triggered();

	void on_actionViewWidgetThemeFolder_triggered();

	void on_actionViewCatPackFolder_triggered();

	void on_actionViewIconsFolder_triggered();
	
	void on_actionViewLogsFolder_triggered();

	void on_actionViewJavaFolder_triggered();

    void on_actionViewSkinsFolder_triggered();

	void on_actionManageSkins_triggered();

	void checkForUpdates();

	void on_actionSettings_triggered();

	void on_actionInstanceSettings_triggered();

	void on_actionManageAccounts_triggered();

	void on_actionReportBug_triggered();

	void on_actionMoreNews_triggered();

	void newsButtonClicked();

	void on_actionLaunchInstance_triggered();

	void on_actionKillInstance_triggered();

	void on_actionLaunchInstanceOffline_triggered();

	void on_actionDeleteInstance_triggered();

	/// Put the most recently trashed instance back. Offered in the Edit
	/// menu and in the instance list's context menu, not on a toolbar --
	/// see createMainToolbar() for why.
	void restoreTrashedInstance();

	/**
	 * Keep the menu bar and give the main toolbar up, or the other way
	 * round. Persisted as the "MenuBarInsteadOfToolBar" setting.
	 */
	void setMenuBarInsteadOfToolBar(bool state);

	void deleteGroup();

	/* The entry that carries the submenu, and one handler per format
	 * behind it.
	 *
	 * On the sidebar the parent is a split button: the arrow opens the
	 * list of formats, and the body needs something to do, or clicking
	 * the thing labelled "Export Instance" does nothing at all. It does
	 * the launcher's own zip - the format that needs no account, no
	 * catalogue and no network, and the one an export is most often
	 * wanted for. In a menu the parent only unfolds the submenu, so this
	 * never fires from there. */
	void on_actionExportInstance_triggered();
	void on_actionExportInstanceZip_triggered();
	void on_actionExportInstanceMrPack_triggered();
	void on_actionExportInstanceFlamePack_triggered();

	void on_actionCreateInstanceShortcut_triggered();

	void on_actionRenameInstance_triggered();
	void on_renameButton_clicked(bool)
	{
		on_actionRenameInstance_triggered();
	}

	void on_actionEditInstance_triggered();

	void on_actionEditInstNotes_triggered();

	void on_actionMods_triggered();

	void on_actionWorlds_triggered();

	void on_actionScreenshots_triggered();

	void taskEnd();

	/**
	 * called when an icon is changed in the icon model.
	 */
	void iconUpdated(QString);

	void showInstanceContextMenu(const QPoint&);

	void updateToolsMenu();

	void instanceActivated(QModelIndex);

	void instanceChanged(const QModelIndex& current,
						 const QModelIndex& previous);

	void instanceSelectRequest(QString id);

	/// Re-reads the selected instance, e.g. after it started or stopped.
	void refreshCurrentInstance();

	void instanceDataChanged(const QModelIndex& topLeft,
							 const QModelIndex& bottomRight);

	void selectionBad();

	void startTask(Task* task);

	void notificationsChanged();

	void defaultAccountChanged();

	void changeActiveAccount();

	void repopulateAccountsMenu();

	void updateNewsLabel();

	void konamiTriggered();

	void globalSettingsClosed();

	/*!
	 * Toggles whether the toolbars can be dragged between dock areas.
	 * Persisted via the "ToolbarsLocked" setting; the resulting layout itself
	 * is persisted by QMainWindow::saveState() in closeEvent().
	 */
	void lockToolbars(bool state);

  private:
	void setupUi();
	void applyThemedIcons();
	void refreshThemedIcons();
	void createMainToolbar();
	void createInstanceToolbar();

	// Widens every instance sidebar button to the widest one. Idempotent:
	// the target comes from the sizeHints, not the bar's current size.
	void syncSidebarWidths();

	void retranslateUi();

	/**
	 * Show the menu bar or the main toolbar according to the setting.
	 *
	 * Does nothing where there is no QMenuBar of ours -- on macOS the
	 * menu bar belongs to MacOSMenuBar and the toolbar always stays.
	 */
	void updateMenuBarVisibility();

	void addInstance(QString url = QString());
	void activateInstance(InstancePtr instance);
	void setCatBackground(bool enabled);
	void updateInstanceToolIcon(QString new_icon);
	void setSelectedInstanceById(const QString& id);
	void updateStatusCenter();

	void runModalTask(Task* task);
	void finalizeInstance(InstancePtr inst);

	/* Opens (or raises) the news dialog. withSidebar picks between
	 * browsing every entry and going straight to the latest one. */
	void showNews(bool withSidebar);

  private:
	std::unique_ptr<Ui::MainWindow> ui;

	// these are managed by Qt's memory management model!
	InstanceView* view = nullptr;
	InstanceProxyModel* proxymodel = nullptr;
	QToolButton* newsLabel = nullptr;
	LabeledToolButton* renameButton = nullptr;
	LabeledToolButton* changeIconButton = nullptr;

	/* Every action that acts on the selected instance, so that they can be
	 * enabled and disabled together. */
	QList<QAction*> instance_actions;
	QList<QToolButton*> instance_buttons;
	QLabel* m_statusLeft = nullptr;
	QLabel* m_statusCenter = nullptr;
	/* Exclusive group behind the profiler entries of the launch menu. It
	 * has to outlive updateToolsMenu(), and the menu's own clear() takes
	 * the actions but not the group, so the group is tracked here and
	 * replaced on every rebuild. */
	QActionGroup* m_profilerActions = nullptr;
	KonamiCode* secretEventFilter = nullptr;

	unique_qobject_ptr<NewsChecker> m_newsChecker;
	unique_qobject_ptr<NotificationChecker> m_notificationChecker;

	// Deletes itself on close (WA_DeleteOnClose), so this only ever
	// holds a live dialog.
	QPointer<NewsViewerDialog> m_newsDialog;

	InstancePtr m_selectedInstance;
	QString m_currentInstIcon;

	// managed by the application object
	Task* m_versionLoadTask = nullptr;
};
