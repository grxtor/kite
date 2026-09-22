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

#include "ui_MainWindow.h"

#include "Application.h"
#include "BuildConfig.h"
#include "plugin/PluginManager.h"
#include "plugin/PluginHooks.h"

#include "MainWindow.h"
#include "QtCompat.h"
#include "ui/MacMenuBar.h"
#include "ui/themes/ThemeManager.h"

#include <QtCore/QVariant>
#include <QtCore/QUrl>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <QtGui/QKeyEvent>

#include <QAction>
#include <QActionGroup>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>
#include <QtWidgets/QWidget>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QProxyStyle>
#include <QtWidgets/QProgressDialog>
#include <QShortcut>

#include <BaseInstance.h>
#include <InstanceList.h>
#include <MMCZip.h>
#include <icons/IconList.h>
#include <java/JavaUtils.h>
#include <java/JavaInstallList.h>
#include <launch/LaunchTask.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/PackProfile.h>
#include <minecraft/VersionFile.h>
#include <minecraft/auth/AccountList.h>
#include <SkinUtils.h>
#include <BuildConfig.h>
#include <net/NetJob.h>
#include <net/Download.h>
#include <news/NewsChecker.h>
#include <notifications/NotificationChecker.h>
#include <tools/BaseProfiler.h>

#include <updater/ExternalUpdater.h>
#include <DesktopServices.h>
#include <FileSystem.h>
#include "InstanceWindow.h"
#include "InstancePageProvider.h"
#include "ui/JavaCommon.h"
#include "LaunchController.h"

#include "ui/instanceview/InstanceProxyModel.h"
#include "ui/instanceview/InstanceView.h"
#include "ui/instanceview/InstanceDelegate.h"
#include "ui/widgets/LabeledToolButton.h"
#include "ui/dialogs/NewInstanceDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/NewsViewerDialog.h"
#include "ui/dialogs/MeshMCLogsDialog.h"
#include "ui/dialogs/PluginsDialog.h"
#include "ui/dialogs/VersionSelectDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/IconPickerDialog.h"
#include "ui/dialogs/CopyInstanceDialog.h"
#include "ui/dialogs/EditAccountDialog.h"
#include "ui/dialogs/NotificationDialog.h"
#include "ui/dialogs/ExportInstanceDialog.h"
#include "ui/dialogs/ExportPackDialog.h"
#include "ui/dialogs/CreateShortcutDialog.h"
#include "ui/dialogs/skins/SkinManageDialog.h"

#include "KonamiCode.h"

#include "InstanceImportTask.h"
#include "InstanceCopyTask.h"

#include "MMCTime.h"

namespace
{
	QString profileInUseFilter(const QString& profile, bool used)
	{
		if (used) {
			return QObject::tr("%1 (in use)").arg(profile);
		} else {
			return profile;
		}
	}

	/**
	 * Pins a tool button's label to the left of whatever space the style
	 * handed it.
	 *
	 * Needed because the Windows 11 style centres the label of a
	 * text-beside-icon tool button while still drawing the icon hard left,
	 * so the sidebar ends up with a tidy column of icons and a ragged
	 * column of text. Every other style Qt ships here already puts the
	 * label immediately after the icon, and for those this override changes
	 * nothing.
	 *
	 * Catching the single text call keeps all the native painting --
	 * background, hover, focus, disabled icons -- exactly as the style drew
	 * it before. Setting a stylesheet cannot do this: text-align has no
	 * effect on QToolButton.
	 *
	 * Deliberately has no base style. QProxyStyle then defers to whatever
	 * QApplication::style() happens to be, so this survives a theme change
	 * rather than pinning a style object that is about to be deleted.
	 */
	class LeftAlignedLabelStyle : public QProxyStyle
	{
	  public:
		using QProxyStyle::QProxyStyle;

		void drawItemText(QPainter* painter, const QRect& rect, int flags,
						  const QPalette& pal, bool enabled,
						  const QString& text,
						  QPalette::ColorRole textRole = QPalette::NoRole)
			const override
		{
			flags &= ~(Qt::AlignHCenter | Qt::AlignRight);
			flags |= Qt::AlignLeft;
			QProxyStyle::drawItemText(painter, rect, flags, pal, enabled, text,
									  textRole);
		}
	};

	/* Holds no per-button state, so one shared instance does. It outlives
	 * every window because setStyle() does not take ownership. */
	QStyle* sidebarLabelStyle()
	{
		static QStyle* style = [] {
			auto* created = new LeftAlignedLabelStyle();
			created->setParent(qApp);
			return static_cast<QStyle*>(created);
		}();
		return style;
	}

	/* Gives one button of a vertical toolbar the sidebar look: it spans the
	 * full width of the bar and reads left to right from its icon, rather
	 * than sitting centred.
	 *
	 * The size policy stops the button shrinking to fit its own text, and
	 * the property is the hint Breeze and its forks look for, since they
	 * centre tool button contents by default. */
	void makeSidebarButton(QToolButton* button)
	{
		button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		button->setProperty("_kde_toolButton_alignment", Qt::AlignLeft);
		button->setStyle(sidebarLabelStyle());
	}
} // namespace

void MainWindow::applyThemedIcons()
{
	for (QAction* action : findChildren<QAction*>()) {
		const QString iconName = action->property("meshmcIcon").toString();
		if (iconName.isEmpty()) {
			continue;
		}

		const QIcon icon = APPLICATION->getThemedIcon(iconName);
		if (icon.isNull()) {
			qWarning() << "No icon named" << iconName
					   << "in the current icon theme, so"
					   << action->objectName() << "has none";
		}
		action->setIcon(icon);
	}
}

void MainWindow::refreshThemedIcons()
{
	applyThemedIcons();

	if (newsLabel) {
		newsLabel->setIcon(APPLICATION->getThemedIcon("news"));
	}

	if (changeIconButton) {
		if (m_currentInstIcon.isEmpty()) {
			// Nothing selected: createInstanceToolbar()'s placeholder.
			changeIconButton->setIcon(APPLICATION->getThemedIcon("news"));
		} else {
			updateInstanceToolIcon(m_currentInstIcon);
		}
	}

	defaultAccountChanged();

	if (view && view->viewport()) {
		view->viewport()->update();
	}

}

void MainWindow::createMainToolbar()
{
	ui->actionFoldersButton->setMenu(ui->foldersMenu);
	if (auto* button = qobject_cast<QToolButton*>(
			ui->mainToolBar->widgetForAction(ui->actionFoldersButton))) {
		button->setPopupMode(QToolButton::InstantPopup);
		button->setFocusPolicy(Qt::NoFocus);
	}

	ui->actionReportBug->setVisible(!BuildConfig.BUG_TRACKER_URL.isEmpty());

	ui->actionDISCORD->setVisible(!BuildConfig.DISCORD_URL.isEmpty());

	ui->actionREDDIT->setVisible(!BuildConfig.SUBREDDIT_URL.isEmpty());

	ui->actionPlugins->setVisible(
		APPLICATION->pluginManager()->moduleCount() >= 1);

	ui->actionHelpButton->setMenu(ui->helpMenu);
	if (auto* button = qobject_cast<QToolButton*>(
			ui->mainToolBar->widgetForAction(ui->actionHelpButton))) {
		button->setPopupMode(QToolButton::InstantPopup);
		button->setFocusPolicy(Qt::NoFocus);
	}

	ui->actionCheckUpdate->setVisible(APPLICATION->updaterEnabled());

	ui->actionPatreon->setVisible(!BuildConfig.PATREON_URL.isEmpty());
}

void MainWindow::createInstanceToolbar()
{
	// NOTE: not added to toolbar, but used for instance context menu (right
	// click)
	ui->actionChangeInstIcon->setIcon(QIcon(":/icons/instances/grass"));
	ui->actionChangeInstIcon->setIconVisibleInMenu(true);

	changeIconButton->setIcon(APPLICATION->getThemedIcon("news"));
	changeIconButton->setToolTip(ui->actionChangeInstIcon->toolTip());
	changeIconButton->setSizePolicy(QSizePolicy::Expanding,
									QSizePolicy::Preferred);

	renameButton->setToolTip(ui->actionRenameInstance->toolTip());
	renameButton->setSizePolicy(QSizePolicy::Expanding,
								QSizePolicy::Preferred);

	auto* exportInstanceMenu = new QMenu(this);
	exportInstanceMenu->setToolTipsVisible(true);
	exportInstanceMenu->addAction(ui->actionExportInstanceZip);
	exportInstanceMenu->addAction(ui->actionExportInstanceMrPack);
	exportInstanceMenu->addAction(ui->actionExportInstanceFlamePack);
	ui->actionExportInstance->setMenu(exportInstanceMenu);

	ui->instanceToolBar->insertWidget(ui->actionLaunchInstance,
									  changeIconButton);
	ui->instanceToolBar->insertWidget(ui->actionLaunchInstance, renameButton);
	ui->instanceToolBar->insertSeparator(ui->actionLaunchInstance);

	/* Qt's default for an action carrying a menu is DelayedPopup, which
	 * leaves a plain click on this button doing nothing at all. Not in
	 * makeSidebarButton(), which runs again on every re-measure and would
	 * undo the popup mode Launch picks per selection. */
	if (auto* exportButton = qobject_cast<QToolButton*>(
			ui->instanceToolBar->widgetForAction(ui->actionExportInstance))) {
		exportButton->setPopupMode(QToolButton::MenuButtonPopup);
	}

	syncSidebarWidths();
}

void MainWindow::syncSidebarWidths()
{
	QList<QToolButton*> buttons;
	int widest = 0;

	for (QAction* action : ui->instanceToolBar->actions()) {
		QWidget* widget = ui->instanceToolBar->widgetForAction(action);
		if (!widget) {
			continue;
		}
		widest = qMax(widest, widget->sizeHint().width());

		/* The two LabeledToolButtons place their own label and already
		 * stretch, so they get a say in the width but are not resized. */
		auto* button = qobject_cast<QToolButton*>(widget);
		if (button && button != changeIconButton && button != renameButton) {
			buttons.append(button);
		}
	}

	for (QToolButton* button : buttons) {
		makeSidebarButton(button);
		button->setMinimumWidth(widest);
	}
}

void MainWindow::setupUi()
{
	/* Before ui->setupUi(), which ends in connectSlotsByName(): these two
	 * have to exist under their final object names by then, or
	 * on_renameButton_clicked and on_changeIconButton_clicked never get
	 * connected. */
	changeIconButton = new LabeledToolButton(this);
	changeIconButton->setObjectName(QStringLiteral("changeIconButton"));

	renameButton = new LabeledToolButton(this);
	renameButton->setObjectName(QStringLiteral("renameButton"));

	ui->setupUi(this);

#ifdef Q_OS_MACOS
	ui->menuBar = nullptr;
#endif

	setWindowIcon(APPLICATION->getThemedIcon("logo"));
	setWindowTitle(BuildConfig.MESHMC_DISPLAYNAME);
#ifndef QT_NO_ACCESSIBILITY
	setAccessibleName(BuildConfig.MESHMC_NAME);
#endif
	applyThemedIcons();

	createMainToolbar();
	createInstanceToolbar();

	/* One list, written out once, so that it can be audited against
	 * the menus above instead of being collected in three places. */
	instance_actions = {ui->actionLaunchInstance,
						ui->actionLaunchInstanceOffline,
						ui->actionKillInstance,
						ui->actionEditInstance,
						ui->actionInstanceSettings,
						ui->actionEditInstNotes,
						ui->actionMods,
						ui->actionWorlds,
						ui->actionScreenshots,
						ui->actionChangeInstGroup,
						ui->actionChangeInstIcon,
						ui->actionRenameInstance,
						ui->actionViewSelectedInstFolder,
						ui->actionViewSelectedMCFolder,
						ui->actionViewSelectedModsFolder,
						ui->actionConfig_Folder,
						ui->actionExportInstance,
						ui->actionCopyInstance,
						ui->actionDeleteInstance,
						ui->actionCreateInstanceShortcut};

	instance_buttons = {changeIconButton,
						renameButton};

	// Nothing is selected yet, and the menus are reachable before
	// anything is.
	for (QAction* action : instance_actions) {
		action->setEnabled(false);
	}

	for (QToolButton* button : instance_buttons) {
		button->setEnabled(false);
	}
}

MainWindow::MainWindow(QWidget* parent)
	: QMainWindow(parent), ui(new Ui::MainWindow)
{
	setupUi();

	// OSX magic.
	setUnifiedTitleAndToolBarOnMac(true);

	// Global shortcuts
	{
		// FIXME: This is kinda weird. and bad. We need some kind of managed
		// shutdown.
		auto q = new QShortcut(QKeySequence::Quit, this);
		connect(q, &QShortcut::activated, qApp, &QApplication::quit);
	}

	// Konami Code
	{
		secretEventFilter = new KonamiCode(this);
		connect(secretEventFilter, &KonamiCode::triggered, this,
				&MainWindow::konamiTriggered);
	}

	// Add the news label to the news toolbar.
	{
		// Feed 0 is ours; anything after it comes from
		// -DMeshMC_NEWS_EXTRA_FEEDS at build time. The news bar only
		// ever shows the newest headline across the lot; the rest is
		// for the news dialog.
		QStringList feeds{BuildConfig.NEWS_RSS_URL};
		feeds.append(BuildConfig.NEWS_EXTRA_FEEDS.split(QLatin1Char(';'),
														Qt::SkipEmptyParts));
		m_newsChecker.reset(new NewsChecker(APPLICATION->network(), feeds));
		newsLabel = new QToolButton();
		newsLabel->setIcon(APPLICATION->getThemedIcon("news"));
		newsLabel->setSizePolicy(QSizePolicy::Expanding,
								 QSizePolicy::Preferred);
		newsLabel->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		newsLabel->setFocusPolicy(Qt::NoFocus);
		ui->newsToolBar->insertWidget(ui->actionMoreNews, newsLabel);
		QObject::connect(newsLabel, &QAbstractButton::clicked, this,
						 &MainWindow::newsButtonClicked);
		QObject::connect(m_newsChecker.get(), &NewsChecker::newsLoaded, this,
						 &MainWindow::updateNewsLabel);

		/* Re-publish a finished load as MMCO_HOOK_NEWS_UPDATED. The
		 * checker is ours, so this is the only place that knows when
		 * every feed has landed — and by now they all have, which is
		 * what the hook promises its listeners. */
		QObject::connect(m_newsChecker.get(), &NewsChecker::newsLoaded, this,
						 []() {
							 if (APPLICATION->pluginManager()) {
								 APPLICATION->pluginManager()->dispatchHook(
									 MMCO_HOOK_NEWS_UPDATED);
							 }
						 });

		updateNewsLabel();
	}

	// Create the instance list widget
	{
		view = new InstanceView(ui->centralWidget);

		view->setSelectionMode(QAbstractItemView::SingleSelection);
		// FIXME: leaks ListViewDelegate
		view->setItemDelegate(new ListViewDelegate(this));
		view->setFrameShape(QFrame::NoFrame);
		// do not show ugly blue border on the mac
		view->setAttribute(Qt::WA_MacShowFocusRect, false);

		view->installEventFilter(this);
		view->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(view, &QWidget::customContextMenuRequested, this,
				&MainWindow::showInstanceContextMenu);
		connect(view, &InstanceView::droppedURLs, this,
				&MainWindow::droppedURLs, Qt::QueuedConnection);

		proxymodel = new InstanceProxyModel(this);
		proxymodel->setSourceModel(APPLICATION->instances().get());
		proxymodel->sort(0);
		connect(proxymodel, &InstanceProxyModel::dataChanged, this,
				&MainWindow::instanceDataChanged);

		view->setModel(proxymodel);
		view->setSourceOfGroupCollapseStatus(
			[](const QString& groupName) -> bool {
				return APPLICATION->instances()->isGroupCollapsed(groupName);
			});
		connect(view, &InstanceView::groupStateChanged,
				APPLICATION->instances().get(),
				&InstanceList::on_GroupStateChanged);
		ui->horizontalLayout->addWidget(view);
	}
	// The cat background
	{
		bool cat_enable = APPLICATION->settings()->get("TheCat").toBool();
		ui->actionCAT->setChecked(cat_enable);
		// NOTE: calling the operator like that is an ugly hack to appease
		// ancient gcc...
		connect(ui->actionCAT, &QAction::toggled, this,
				&MainWindow::onCatToggled);
		setCatBackground(cat_enable);
	}
	// start instance when double-clicked
	connect(view, &InstanceView::activated, this,
			&MainWindow::instanceActivated);

	// track the selection -- update the instance toolbar
	connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this,
			&MainWindow::instanceChanged);

	// track icon changes and update the toolbar!
	connect(APPLICATION->icons().get(), &IconList::iconUpdated, this,
			&MainWindow::iconUpdated);

	// model reset -> selection is invalid. All the instance pointers are wrong.
	connect(APPLICATION->instances().get(), &InstanceList::dataIsInvalid, this,
			&MainWindow::selectionBad);

	// handle newly added instances
	connect(APPLICATION->instances().get(),
			&InstanceList::instanceSelectRequest, this,
			&MainWindow::instanceSelectRequest);

	// When the global settings page closes, we want to know about it and update
	// our state
	connect(APPLICATION, &Application::globalSettingsClosed, this,
			&MainWindow::globalSettingsClosed);

	/* Picking an icon theme in the appearance page takes effect here, live.
	 * Without this the icons are already correct as objects and still drawn
	 * from the old theme, because nothing repaints */
	connect(APPLICATION->themeManager(), &ThemeManager::iconThemeChanged,
			this, &MainWindow::refreshThemedIcons);

	m_statusLeft = new QLabel(tr("No instance selected"), this);
	m_statusCenter = new QLabel(tr("Total playtime: 0s"), this);
	statusBar()->addPermanentWidget(m_statusLeft, 1);
	statusBar()->addPermanentWidget(m_statusCenter, 0);

	QWidget* spacer = new QWidget(this);
	spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	ui->mainToolBar->insertWidget(ui->actionAccountsButton, spacer);

	ui->actionAccountsButton->setIcon(APPLICATION->getThemedIcon("noaccount"));
	ui->actionAccountsButton->setMenu(ui->accountMenu);
	if (auto* button = qobject_cast<QToolButton*>(
		ui->mainToolBar->widgetForAction(ui->actionAccountsButton))) {
		button->setPopupMode(QToolButton::InstantPopup);
	}

	repopulateAccountsMenu();

	/* The accounts menu exists only now, so it takes its place in the menu
	 * bar here, in front of Help -- the same QMenu the tool button above
	 * pops up, so it keeps being rebuilt by repopulateAccountsMenu()
	 * either way. */
	/* Outside the menu bar check on purpose: the action is built on every
	 * platform, so it is wired on every platform. On macOS it is waiting
	 * for a field in MacOSMenuBar::Actions rather than being dead. */
	connect(ui->actionUndoTrashInstance, &QAction::triggered,
			this, &MainWindow::restoreTrashedInstance);

	if (ui->menuBar) {
		// Set before connecting, so restoring the setting is not mistaken
		// for the user asking for the swap.
		ui->actionMenuBarInsteadOfToolBar->setChecked(
			APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool());
		connect(ui->actionMenuBarInsteadOfToolBar,
				&QAction::toggled, this,
				&MainWindow::setMenuBarInsteadOfToolBar);
		updateMenuBarVisibility();
	}

	// Update the menu when the active account changes.
	// Shouldn't have to use lambdas here like this, but if I don't, the
	// compiler throws a fit. Template hell sucks...
	connect(APPLICATION->accounts().get(), &AccountList::defaultAccountChanged,
			[this] { defaultAccountChanged(); });
	connect(APPLICATION->accounts().get(), &AccountList::listChanged,
			[this] { repopulateAccountsMenu(); });

	// Show initial account
	defaultAccountChanged();

	/* Give macOS its screen-top menu. The bar finds the toolbar actions by
	 * object name and reuses them, so nothing has to be listed here, and it
	 * keeps itself in step with the account list and the interface
	 * language on its own. Dormant everywhere else. */
	MacMenuBar::attachTo(this);

	// TODO: refresh accounts here?
	// auto accounts = APPLICATION->accounts();

	// load the news
	{
		m_newsChecker->reloadNews();
		updateNewsLabel();
	}

	if (APPLICATION->updaterEnabled()) {
		updatesAllowedChanged(APPLICATION->updatesAreAllowed());

		// NOTE: calling the operator like that is an ugly hack to appease
		// ancient gcc...
		connect(ui->actionCheckUpdate, &QAction::triggered, this,
				&MainWindow::checkForUpdates);

		// Automatic checks are the updater's own business now: it keeps the
		// schedule, the interval and the last-checked time in its config, so
		// that they survive a restart and apply even when this window is not
		// the thing that started the launcher.
		if (auto* updater = APPLICATION->updater()) {
			connect(updater, &ExternalUpdater::canCheckForUpdatesChanged, this,
					&MainWindow::updatesAllowedChanged);
		}
	}

	{
		auto checker = new NotificationChecker();
		checker->setNotificationsUrl(QUrl(BuildConfig.NOTIFICATION_URL));
		checker->setApplicationChannel(BuildConfig.VERSION_CHANNEL);
		checker->setApplicationPlatform(BuildConfig.BUILD_PLATFORM);
		checker->setApplicationFullVersion(BuildConfig.FULL_VERSION_STR);
		m_notificationChecker.reset(checker);
		connect(m_notificationChecker.get(),
				&NotificationChecker::notificationCheckFinished, this,
				&MainWindow::notificationsChanged);
		checker->checkForNotifications();
	}

	// Toolbar movability, restored from settings. The layout the user drags the
	// toolbars into is persisted separately by saveState()/restoreState().
	{
		bool toolbarsLocked =
			APPLICATION->settings()->get("ToolbarsLocked").toBool();
		ui->actionLockToolbars->setChecked(toolbarsLocked);
		connect(ui->actionLockToolbars, &QAction::toggled, this,
				&MainWindow::lockToolbars);
		lockToolbars(toolbarsLocked);
	}

	setSelectedInstanceById(
		APPLICATION->settings()->get("SelectedInstance").toString());

	// removing this looks stupid
	view->setFocus();

	retranslateUi();

	// Notify plugins that the main UI is ready
	if (APPLICATION->pluginManager()) {
		/* NOTE: plugins used to be able to push their own entries onto the
		 * instance sidebar here, through ui_register_instance_action(). That
		 * API is deprecated and now does nothing, so there is nothing left
		 * to insert -- see PluginManager for why. */

		// Hand plugins direct handles to the long-lived widgets they
		// most commonly want to hook (saves them from scanning
		// qApp->allWidgets() on every load).
		MMCOUiMainReadyPayload mainReady{};
		mainReady.main_window = static_cast<void*>(this);
		mainReady.news_toolbar = static_cast<void*>(ui->newsToolBar);
		mainReady.more_news_action =
			static_cast<void*>(ui->actionMoreNews);
		mainReady.news_label_button = static_cast<void*>(newsLabel);
		APPLICATION->pluginManager()->dispatchHook(MMCO_HOOK_UI_MAIN_READY,
												   &mainReady);
	}
}

void MainWindow::retranslateUi()
{
	ui->retranslateUi(this);

	const QString appName = BuildConfig.MESHMC_NAME;
	for (QAction* action : findChildren<QAction*>()) {
		if (action->text().contains(QLatin1String("%1")))
			action->setText(action->text().arg(appName));
		if (action->toolTip().contains(QLatin1String("%1")))
			action->setToolTip(action->toolTip().arg(appName));
	}

	QString winTitle = tr("%1 - Version %2", "MeshMC - Version X")
						   .arg(BuildConfig.MESHMC_DISPLAYNAME,
								BuildConfig.printableVersionString());
	if (!BuildConfig.BUILD_PLATFORM.isEmpty()) {
		winTitle += tr(" on %1", "on platform, as in operating system")
						.arg(BuildConfig.BUILD_PLATFORM);
	}
	setWindowTitle(APPLICATION->applicationDisplayName());

	// New labels mean new widths for the sidebar to line up against.
	syncSidebarWidths();

	auto accounts = APPLICATION->accounts();
	MinecraftAccountPtr defaultAccount = accounts->defaultAccount();
	if (defaultAccount) {
		auto profileLabel = profileInUseFilter(defaultAccount->profileName(),
											   defaultAccount->isInUse());
		ui->actionAccountsButton->setText(profileLabel);
	} else {
		ui->actionAccountsButton->setText(tr("Profiles"));
	}

	if (m_selectedInstance) {
		m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
	} else {
		m_statusLeft->setText(tr("No instance selected"));
	}
}

MainWindow::~MainWindow() {}

QMenu* MainWindow::createPopupMenu()
{
	QMenu* filteredMenu = QMainWindow::createPopupMenu();
	filteredMenu->removeAction(ui->mainToolBar->toggleViewAction());
	filteredMenu->addAction(ui->actionLockToolbars);
	return filteredMenu;
}

void MainWindow::lockToolbars(bool state)
{
	ui->mainToolBar->setMovable(!state);
	ui->instanceToolBar->setMovable(!state);
	ui->newsToolBar->setMovable(!state);
	APPLICATION->settings()->set("ToolbarsLocked", state);
}

void MainWindow::setMenuBarInsteadOfToolBar(bool state)
{
	APPLICATION->settings()->set("MenuBarInsteadOfToolBar", state);
	updateMenuBarVisibility();
}

void MainWindow::showEvent(QShowEvent* event)
{
	/* The setting is applied again here because QMainWindow::restoreState()
	 * runs after the constructor -- Application::showMainWindow() calls it
	 * on the finished window -- and it restores the main toolbar's saved
	 * visibility along with the layout. Without this, turning the menu bar
	 * mode off would leave the next start with the toolbar still hidden
	 * (saved that way) and the menu bar hidden too: a window with neither.
	 *
	 * Safe to repeat: nothing else owns the main toolbar's visibility.
	 * createPopupMenu() takes its toggle out of the toolbar context menu,
	 * so the user cannot hide it by hand and this cannot be overruling
	 * them. */
	updateMenuBarVisibility();
	QMainWindow::showEvent(event);
}

void MainWindow::updateMenuBarVisibility()
{
	if (!ui->menuBar) {
		// macOS: the native bar is MacOSMenuBar's, and the toolbar stays.
		return;
	}
	const bool instead =
		APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool();
	ui->menuBar->setVisible(instead);
	ui->mainToolBar->setVisible(!instead);
}

#ifndef Q_OS_MACOS
void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
	/* Alt on its own shows the menu bar and hides it again, the way it
	 * works in every other window that keeps its menus out of the way.
	 * Pointless while the menu bar is already the permanent one, so the
	 * key is left to the base class then -- and to the mnemonics it
	 * carries. */
	if (ui->menuBar && event->key() == Qt::Key_Alt &&
		!APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool()) {
		ui->menuBar->setVisible(!ui->menuBar->isVisible());
		return;
	}
	QMainWindow::keyReleaseEvent(event);
}
#endif

void MainWindow::konamiTriggered()
{
	qDebug() << "Super Secret Mode ACTIVATED!";
}

void MainWindow::showInstanceContextMenu(const QPoint& pos)
{
	QMenu myMenu;
	myMenu.setToolTipsVisible(true);

	QList<QAction*> actions;

	QAction* actionSep = new QAction("", this);
	actionSep->setSeparator(true);

	bool onInstance = view->indexAt(pos).isValid();
	if (onInstance) {
		actions = ui->instanceToolBar->actions();

		// replace the change icon widget with an actual action
		actions.replace(0, ui->actionChangeInstIcon);

		// replace the rename widget with an actual action
		actions.replace(1, ui->actionRenameInstance);

		// add header
		actions.prepend(actionSep);
		QAction* actionVoid = new QAction(m_selectedInstance->name(), this);
		actionVoid->setEnabled(false);
		actions.prepend(actionVoid);
	} else {
		auto group = view->groupNameAt(pos);

		QAction* actionVoid = new QAction(BuildConfig.MESHMC_NAME, this);
		actionVoid->setEnabled(false);

		QAction* actionCreateInstance =
			new QAction(tr("Create instance"), this);
		actionCreateInstance->setToolTip(ui->actionAddInstance->toolTip());
		if (!group.isNull()) {
			QVariantMap data;
			data["group"] = group;
			actionCreateInstance->setData(data);
		}

		connect(actionCreateInstance, &QAction::triggered, this,
				&MainWindow::on_actionAddInstance_triggered);

		actions.prepend(actionSep);
		actions.prepend(actionVoid);
		actions.append(actionCreateInstance);
		if (!group.isNull()) {
			QAction* actionDeleteGroup =
				new QAction(tr("Delete group '%1'").arg(group), this);
			QVariantMap data;
			data["group"] = group;
			actionDeleteGroup->setData(data);
			connect(actionDeleteGroup, &QAction::triggered, this,
					&MainWindow::deleteGroup);
			actions.append(actionDeleteGroup);
		}
	}

	/* Restoring what was just deleted belongs next to the Delete that did
	 * it, and only while there is something to restore -- an entry that is
	 * absent rather than greyed out says the same thing without taking up
	 * room. Named after the instance, because "Restore Instance" on its own
	 * does not say which one is coming back.
	 *
	 * Parented to the menu, so both entries die with it. The throwaway
	 * actions above are parented to the window and pile up one set per
	 * right-click; that is older than this block, but no reason to add to
	 * it. */
	auto instances = APPLICATION->instances();
	if (instances->trashedSomething()) {
		/* A separator of its own: actionSep is already in the list above,
		 * and adding one QAction to a menu twice moves it rather than
		 * repeating it, which would take the header's separator away. */
		QAction* trailingSep = new QAction("", &myMenu);
		trailingSep->setSeparator(true);
		actions.append(trailingSep);

		/* Same verb as the Edit menu entry, so the two do not read like
		 * two different features; the name is here because a context menu
		 * can afford it and "which one?" is the first question otherwise. */
		QAction* actionRestore = new QAction(
			tr("Undo Deletion of \"%1\"").arg(instances->lastTrashedName()),
			&myMenu);
		actionRestore->setToolTip(
			tr("Bring the instance you last sent to the trash back, along "
			   "with its shortcuts."));
		connect(actionRestore, &QAction::triggered, this,
				&MainWindow::restoreTrashedInstance);
		actions.append(actionRestore);
	}

	myMenu.addActions(actions);
	/*
	if (onInstance)
		myMenu.setEnabled(m_selectedInstance->canLaunch());
	*/

	// Let plugins add items to the context menu
	if (APPLICATION->pluginManager()) {
		QByteArray ctxName = onInstance ? "instance" : "main";
		MMCOMenuEvent menuEvt{};
		menuEvt.context = ctxName.constData();
		menuEvt.menu_handle = &myMenu;
		APPLICATION->pluginManager()->dispatchHook(MMCO_HOOK_UI_CONTEXT_MENU,
												   &menuEvt);
	}

	myMenu.exec(view->mapToGlobal(pos));
}

void MainWindow::updateToolsMenu()
{
	/* One drop-down, hanging off the Launch button: the launch modes
	 * first, then the profiler the instance runs under. Launch Offline used
	 * to be a toolbar button of its own with a duplicate profiler list
	 * underneath it; the action still exists for the macOS menu bar, it
	 * just lives in here now.
	 *
	 * The profiler entries do not launch anything -- they write an instance
	 * setting, so that Launch, Launch Offline and every other way into the
	 * game profile alike. */
	QToolButton* launchButton = dynamic_cast<QToolButton*>(
		ui->instanceToolBar->widgetForAction(ui->actionLaunchInstance));

	if (!m_selectedInstance || m_selectedInstance->isRunning()) {
		/* setMenu(nullptr) only detaches the menu -- it belongs to the
		 * window, so it would otherwise pile up one menu per start/stop. */
		QMenu* stale = ui->actionLaunchInstance->menu();
		ui->actionLaunchInstance->setMenu(nullptr);
		delete m_profilerActions;
		m_profilerActions = nullptr;
		if (stale) {
			stale->deleteLater();
		}
		if (launchButton) {
			launchButton->setPopupMode(QToolButton::InstantPopup);
		}
		return;
	}

	QMenu* launchMenu = ui->actionLaunchInstance->menu();
	if (launchMenu) {
		launchMenu->clear();
	} else {
		launchMenu = new QMenu(this);
		// The disabled entries below explain themselves through tooltips,
		// which a menu does not show unless asked to.
		launchMenu->setToolTipsVisible(true);
	}
	/* clear() destroyed the actions but not the group that held them. */
	delete m_profilerActions;
	m_profilerActions = nullptr;

	if (launchButton) {
		launchButton->setPopupMode(QToolButton::MenuButtonPopup);
	}

	QAction* normalLaunch = launchMenu->addAction(tr("Launch"));
	connect(normalLaunch, &QAction::triggered, this, [this]() {
		APPLICATION->launch(m_selectedInstance, LaunchMode::Normal);
	});

	QAction* offlineLaunch = launchMenu->addAction(tr("Launch Offline"));
	connect(offlineLaunch, &QAction::triggered, this, [this]() {
		APPLICATION->launch(m_selectedInstance, LaunchMode::Offline);
	});

	auto mcInstance =
		std::dynamic_pointer_cast<MinecraftInstance>(m_selectedInstance);
	if (mcInstance) {
		QAction* demoLaunch = launchMenu->addAction(tr("Launch &Demo"));
		if (mcInstance->supportsDemo()) {
			connect(demoLaunch, &QAction::triggered, this, [this]() {
				APPLICATION->launch(m_selectedInstance, LaunchMode::Demo);
			});
		} else {
			demoLaunch->setDisabled(true);
			demoLaunch->setToolTip(
				tr("This Minecraft version has no demo mode. It was added "
				   "in 1.3.1."));
		}
	}

	/* The profiler is a property of the instance, so these are a radio
	 * group reflecting a setting, not four different ways to launch. */
	launchMenu->addSeparator()->setText(tr("Profilers"));

	m_profilerActions = new QActionGroup(launchMenu);
	m_profilerActions->setExclusive(true);

	const QString currentProfiler = m_selectedInstance->profilerKey();

	QAction* noProfiler = launchMenu->addAction(tr("No Profiler"));
	noProfiler->setCheckable(true);
	noProfiler->setData(QString());
	noProfiler->setChecked(currentProfiler.isEmpty());
	m_profilerActions->addAction(noProfiler);

	const auto& profilers = APPLICATION->profilers();
	for (auto it = profilers.constBegin(); it != profilers.constEnd(); ++it) {
		const auto& profiler = it.value();
		QAction* profilerAction = launchMenu->addAction(profiler->name());
		profilerAction->setCheckable(true);
		profilerAction->setData(it.key());
		profilerAction->setChecked(it.key() == currentProfiler);
		m_profilerActions->addAction(profilerAction);

		QString error;
		if (!profiler->check(&error)) {
			profilerAction->setDisabled(true);
			profilerAction->setToolTip(
				tr("Profiler not setup correctly. Go into settings, "
				   "\"External Tools\"."));
		}
	}
	/* A setting naming a profiler this build does not have leaves nothing
	 * checked, which is the truth: no entry here stands for it. The
	 * setting itself is left alone, and Application::launch() says so when
	 * the instance starts. */

	connect(m_profilerActions, &QActionGroup::triggered, this,
			[this](QAction* action) {
				if (m_selectedInstance) {
					m_selectedInstance->setProfilerKey(
						action->data().toString());
				}
			});

	ui->actionLaunchInstance->setMenu(launchMenu);
}

void MainWindow::repopulateAccountsMenu()
{
	ui->accountMenu->clear();

	auto accounts = APPLICATION->accounts();
	MinecraftAccountPtr defaultAccount = accounts->defaultAccount();

	QString active_profileId = "";
	if (defaultAccount) {
		auto profileLabel = profileInUseFilter(
			defaultAccount->profileName(), defaultAccount->isInUse());
		ui->actionAccountsButton->setText(profileLabel);
	}

	if (accounts->count() <= 0) {
		QAction* action = new QAction(tr("No accounts added!"), this);
		action->setEnabled(false);
		ui->accountMenu->addAction(action);
	} else {
		// TODO: Nicer way to iterate?
		for (int i = 0; i < accounts->count(); i++) {
			MinecraftAccountPtr account = accounts->at(i);
			auto profileLabel =
				profileInUseFilter(account->profileName(), account->isInUse());
			QAction* action = new QAction(profileLabel, this);
			action->setData(i);
			action->setCheckable(true);
			if (defaultAccount == account) {
				action->setChecked(true);
			}

			auto face = account->getFace();
			if (!face.isNull()) {
				action->setIcon(face);
			} else {
				action->setIcon(APPLICATION->getThemedIcon("noaccount"));
			}
			ui->accountMenu->addAction(action);
			connect(action, &QAction::triggered, this,
					&MainWindow::changeActiveAccount);
		}
	}

	ui->accountMenu->addSeparator();

	QAction* action = new QAction(tr("No Default Account"), this);
	action->setCheckable(true);
	action->setIcon(APPLICATION->getThemedIcon("noaccount"));
	action->setData(-1);
	if (!defaultAccount) {
		action->setChecked(true);
	}

	ui->accountMenu->addAction(action);
	connect(action, &QAction::triggered, this,
			&MainWindow::changeActiveAccount);

	ui->accountMenu->addSeparator();
	ui->actionManageSkins->setEnabled(defaultAccount &&
									  defaultAccount->isMSA() &&
									  !defaultAccount->isActive());
	ui->accountMenu->addAction(ui->actionManageSkins);
	ui->accountMenu->addAction(ui->actionManageAccounts);
}

void MainWindow::updatesAllowedChanged(bool allowed)
{
	if (!APPLICATION->updaterEnabled()) {
		return;
	}
	ui->actionCheckUpdate->setEnabled(allowed);
}

/*
 * Assumes the sender is a QAction
 */
void MainWindow::changeActiveAccount()
{
	QAction* sAction = (QAction*)sender();

	// Profile's associated Mojang username
	if (sAction->data().type() != QVariant::Type::Int)
		return;

	QVariant data = sAction->data();
	bool valid = false;
	int index = data.toInt(&valid);
	if (!valid) {
		index = -1;
	}
	auto accounts = APPLICATION->accounts();
	accounts->setDefaultAccount(index == -1 ? nullptr : accounts->at(index));
	defaultAccountChanged();
}

void MainWindow::defaultAccountChanged()
{
	repopulateAccountsMenu();

	MinecraftAccountPtr account = APPLICATION->accounts()->defaultAccount();

	// FIXME: this needs adjustment for MSA
	if (account && account->profileName() != "") {
		auto profileLabel =
			profileInUseFilter(account->profileName(), account->isInUse());
		ui->actionAccountsButton->setText(profileLabel);
		auto face = account->getFace();
		if (face.isNull()) {
			ui->actionAccountsButton->setIcon(APPLICATION->getThemedIcon("noaccount"));
		} else {
			ui->actionAccountsButton->setIcon(face);
		}
		return;
	}

	// Set the icon to the "no account" icon.
	ui->actionAccountsButton->setIcon(APPLICATION->getThemedIcon("noaccount"));
	ui->actionAccountsButton->setText(tr("Profiles"));
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
	if (obj == view) {
		if (ev->type() == QEvent::KeyPress) {
			secretEventFilter->input(ev);
			QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);
			switch (keyEvent->key()) {
					/*
				case Qt::Key_Enter:
				case Qt::Key_Return:
					activateInstance(m_selectedInstance);
					return true;
					*/
				/* Backspace as well: the key macOS keyboards label
				 * "delete" arrives as Key_Backspace, so Key_Delete alone
				 * means the list cannot be cleared by keyboard there at
				 * all. */
				case Qt::Key_Delete:
				case Qt::Key_Backspace:
					on_actionDeleteInstance_triggered();
					return true;
				case Qt::Key_F5:
					refreshInstances();
					return true;
				case Qt::Key_F2:
					on_actionRenameInstance_triggered();
					return true;
				default:
					break;
			}
		}
	}
	return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::updateNewsLabel()
{
	if (m_newsChecker->isLoadingNews()) {
		newsLabel->setText(tr("Loading news..."));
		newsLabel->setEnabled(false);
	} else {
		QList<NewsEntryPtr> entries = m_newsChecker->getNewsEntries();
		if (entries.length() > 0) {
			newsLabel->setText(entries[0]->title);
			newsLabel->setEnabled(true);
		} else {
			newsLabel->setText(tr("No news available."));
			newsLabel->setEnabled(false);
		}
	}
}

QList<int> stringToIntList(const QString& string)
{
	QStringList split = string.split(',', Qt::SkipEmptyParts);
	QList<int> out;
	for (int i = 0; i < split.size(); ++i) {
		out.append(split.at(i).toInt());
	}
	return out;
}
QString intListToString(const QList<int>& list)
{
	QStringList slist;
	for (int i = 0; i < list.size(); ++i) {
		slist.append(QString::number(list.at(i)));
	}
	return slist.join(',');
}
void MainWindow::notificationsChanged()
{
	QList<NotificationChecker::NotificationEntry> entries =
		m_notificationChecker->notificationEntries();
	QList<int> shownNotifications = stringToIntList(
		APPLICATION->settings()->get("ShownNotifications").toString());
	for (auto it = entries.begin(); it != entries.end(); ++it) {
		NotificationChecker::NotificationEntry entry = *it;
		if (!shownNotifications.contains(entry.id)) {
			NotificationDialog dialog(entry, this);
			if (dialog.exec() == NotificationDialog::DontShowAgain) {
				shownNotifications.append(entry.id);
			}
		}
	}
	APPLICATION->settings()->set("ShownNotifications",
								 intListToString(shownNotifications));
}

void MainWindow::onCatToggled(bool state)
{
	setCatBackground(state);
	APPLICATION->settings()->set("TheCat", state);
}

void MainWindow::setCatBackground(bool enabled)
{
	if (enabled) {
		QString catPath = APPLICATION->themeManager()->getCatPack();
		view->setStyleSheet(QString(R"(
InstanceView
{
    background-image: url(%1);
    background-attachment: fixed;
    background-clip: padding;
    background-position: top right;
    background-repeat: none;
    background-color:palette(base);
})")
								.arg(catPath));
	} else {
		view->setStyleSheet(QString());
	}
}

void MainWindow::runModalTask(Task* task)
{
	connect(task, &Task::failed, [this](QString reason) {
		CustomMessageBox::selectable(this, tr("Error"), reason,
									 QMessageBox::Critical)
			->show();
	});
	connect(task, &Task::succeeded, [this, task]() {
		QStringList warnings = task->warnings();
		if (warnings.count()) {
			CustomMessageBox::selectable(
				this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)
				->show();
		}
	});
	ProgressDialog loadDialog(this);
	loadDialog.setSkipButton(true, tr("Abort"));
	loadDialog.execWithTask(task);
}

void MainWindow::instanceFromInstanceTask(InstanceTask* rawTask)
{
	unique_qobject_ptr<Task> task(
		APPLICATION->instances()->wrapInstanceTask(rawTask));
	runModalTask(task.get());
}

void MainWindow::on_actionCopyInstance_triggered()
{
	if (!m_selectedInstance)
		return;

	CopyInstanceDialog copyInstDlg(m_selectedInstance, this);
	if (!copyInstDlg.exec())
		return;

	auto copyTask =
		new InstanceCopyTask(m_selectedInstance, copyInstDlg.shouldCopySaves(),
							 copyInstDlg.shouldKeepPlaytime());
	copyTask->setName(copyInstDlg.instName());
	copyTask->setGroup(copyInstDlg.instGroup());
	copyTask->setIcon(copyInstDlg.iconKey());
	unique_qobject_ptr<Task> task(
		APPLICATION->instances()->wrapInstanceTask(copyTask));
	runModalTask(task.get());
}

void MainWindow::finalizeInstance(InstancePtr inst)
{
	view->updateGeometries();
	setSelectedInstanceById(inst->id());
	if (APPLICATION->accounts()->anyAccountIsValid()) {
		ProgressDialog loadDialog(this);
		auto update = inst->createUpdateTask(Net::Mode::Online);
		connect(update.get(), &Task::failed, [this](QString reason) {
			QString error = QString("Instance load failed: %1").arg(reason);
			CustomMessageBox::selectable(this, tr("Error"), error,
										 QMessageBox::Warning)
				->show();
		});
		if (update) {
			loadDialog.setSkipButton(true, tr("Abort"));
			loadDialog.execWithTask(update.get());
		}
	} else {
		CustomMessageBox::selectable(
			this, tr("Error"),
			tr("MeshMC cannot download Minecraft or update instances unless "
			   "you have at least "
			   "one account added.\nPlease add your Mojang or Minecraft "
			   "account."),
			QMessageBox::Warning)
			->show();
	}
}

void MainWindow::addInstance(QString url)
{
	QString groupName;
	do {
		QObject* obj = sender();
		if (!obj)
			break;
		QAction* action = qobject_cast<QAction*>(obj);
		if (!action)
			break;
		auto map = action->data().toMap();
		if (!map.contains("group"))
			break;
		groupName = map["group"].toString();
	} while (0);

	if (groupName.isEmpty()) {
		groupName = APPLICATION->settings()
						->get("LastUsedGroupForNewInstance")
						.toString();
	}

	NewInstanceDialog newInstDlg(groupName, url, this);
	if (!newInstDlg.exec())
		return;

	APPLICATION->settings()->set("LastUsedGroupForNewInstance",
								 newInstDlg.instGroup());
	/* Remembered alongside the group, and for the same reason: somebody
	 * installing several packs onto a second disk should not have to
	 * re-pick the folder each time. Recorded only once the dialog was
	 * accepted, so browsing and cancelling moves nothing. */
	APPLICATION->settings()->set("LastUsedInstDirForNewInstance",
								 newInstDlg.instDir());

	InstanceTask* creationTask = newInstDlg.extractTask();
	if (creationTask) {
		instanceFromInstanceTask(creationTask);
	}
}

void MainWindow::on_actionAddInstance_triggered()
{
	addInstance();
}

void MainWindow::droppedURLs(QList<QUrl> urls)
{
	for (auto& url : urls) {
		if (url.isLocalFile()) {
			addInstance(url.toLocalFile());
		} else {
			addInstance(url.toString());
		}
		// Only process one dropped file...
		break;
	}
}

void MainWindow::on_actionREDDIT_triggered()
{
	DesktopServices::openUrl(QUrl(BuildConfig.SUBREDDIT_URL));
}

void MainWindow::on_actionDISCORD_triggered()
{
	DesktopServices::openUrl(QUrl(BuildConfig.DISCORD_URL));
}

void MainWindow::on_actionChangeInstIcon_triggered()
{
	if (!m_selectedInstance)
		return;

	IconPickerDialog dlg(this);
	dlg.execWithSelection(m_selectedInstance->iconKey());
	if (dlg.result() == QDialog::Accepted) {
		m_selectedInstance->setIconKey(dlg.selectedIconKey);
		auto icon = APPLICATION->icons()->getIcon(dlg.selectedIconKey);
		ui->actionChangeInstIcon->setIcon(icon);
		changeIconButton->setIcon(icon);
	}
}

void MainWindow::iconUpdated(QString icon)
{
	if (icon == m_currentInstIcon) {
		auto icon = APPLICATION->icons()->getIcon(m_currentInstIcon);
		ui->actionChangeInstIcon->setIcon(icon);
		changeIconButton->setIcon(icon);
	}
}

void MainWindow::updateInstanceToolIcon(QString new_icon)
{
	m_currentInstIcon = new_icon;
	auto icon = APPLICATION->icons()->getIcon(m_currentInstIcon);
	ui->actionChangeInstIcon->setIcon(icon);
	changeIconButton->setIcon(icon);
}

void MainWindow::setSelectedInstanceById(const QString& id)
{
	if (id.isNull())
		return;
	const QModelIndex index =
		APPLICATION->instances()->getInstanceIndexById(id);
	if (index.isValid()) {
		QModelIndex selectionIndex = proxymodel->mapFromSource(index);
		view->selectionModel()->setCurrentIndex(
			selectionIndex, QItemSelectionModel::ClearAndSelect);
		updateStatusCenter();
	}
}

void MainWindow::on_actionChangeInstGroup_triggered()
{
	if (!m_selectedInstance)
		return;

	bool ok = false;
	InstanceId instId = m_selectedInstance->id();
	QString name(APPLICATION->instances()->getInstanceGroup(instId));
	auto groups = APPLICATION->instances()->getGroups();
	groups.insert(0, "");
	groups.sort(Qt::CaseInsensitive);
	int foo = groups.indexOf(name);

	name = QInputDialog::getItem(this, tr("Group name"),
								 tr("Enter a new group name."), groups, foo,
								 true, &ok);
	name = name.simplified();
	if (ok) {
		APPLICATION->instances()->setInstanceGroup(instId, name);
	}
}

void MainWindow::deleteGroup()
{
	QObject* obj = sender();
	if (!obj)
		return;
	QAction* action = qobject_cast<QAction*>(obj);
	if (!action)
		return;
	auto map = action->data().toMap();
	if (!map.contains("group"))
		return;
	QString groupName = map["group"].toString();
	if (!groupName.isEmpty()) {
		auto reply = QMessageBox::question(
			this, tr("Delete group"),
			tr("Are you sure you want to delete the group %1").arg(groupName),
			QMessageBox::Yes | QMessageBox::No);
		if (reply == QMessageBox::Yes) {
			APPLICATION->instances()->deleteGroup(groupName);
		}
	}
}

void MainWindow::on_actionViewLauncherRootFolder_triggered()
{
	DesktopServices::openDirectory(".");
}

void MainWindow::on_actionViewInstanceFolder_triggered()
{
	/* The primary folder, asked of the list rather than read back out of
	 * the setting: the setting may be a relative path, while the list has
	 * already resolved it against the working directory and confirmed it
	 * exists. With several folders configured this opens the one new
	 * instances go to, which is the one this action has always meant. */
	DesktopServices::openDirectory(APPLICATION->instances()->primaryDir());
}

void MainWindow::refreshInstances()
{
	APPLICATION->instances()->loadList();
}

void MainWindow::on_actionViewCentralModsFolder_triggered()
{
	DesktopServices::openDirectory(
		APPLICATION->settings()->get("CentralModsDir").toString(), true);
}

void MainWindow::on_actionViewSkinsFolder_triggered()
{
	DesktopServices::openDirectory(APPLICATION->settings()->get("SkinsDir").toString(), true);
}

void MainWindow::on_actionViewIconThemeFolder_triggered()
{
	DesktopServices::openDirectory(APPLICATION->themeManager()->getIconThemesFolder().path(), true);
}

void MainWindow::on_actionViewWidgetThemeFolder_triggered()
{
	DesktopServices::openDirectory(APPLICATION->themeManager()->getApplicationThemesFolder().path(), true);
}

void MainWindow::on_actionViewCatPackFolder_triggered()
{
	DesktopServices::openDirectory(APPLICATION->themeManager()->getCatPacksFolder().path(), true);
}

void MainWindow::on_actionViewIconsFolder_triggered()
{
	DesktopServices::openDirectory(APPLICATION->icons()->getDirectory(), true);
}

void MainWindow::on_actionViewLogsFolder_triggered()
{
	DesktopServices::openDirectory("logs", true);
}

void MainWindow::on_actionViewJavaFolder_triggered()
{
	DesktopServices::openDirectory(APPLICATION->javaPath(), true);
}

void MainWindow::on_actionConfig_Folder_triggered()
{
	if (m_selectedInstance) {
		QString str = m_selectedInstance->instanceConfigFolder();
		DesktopServices::openDirectory(QDir(str).absolutePath());
	}
}

void MainWindow::checkForUpdates()
{
	if (!APPLICATION->updaterEnabled()) {
		qWarning() << "The updater is not set up; cannot check for updates.";
		return;
	}

	APPLICATION->triggerUpdateCheck();
}

void MainWindow::on_actionSettings_triggered()
{
	APPLICATION->ShowGlobalSettings(this, "global-settings");
}

void MainWindow::globalSettingsClosed()
{
	// FIXME: quick HACK to make this work. improve, optimize.
	APPLICATION->instances()->loadList();
	proxymodel->invalidate();
	proxymodel->sort(0);
	updateToolsMenu();
	updateStatusCenter();
	update();
}

void MainWindow::on_actionInstanceSettings_triggered()
{
	APPLICATION->showInstanceWindow(m_selectedInstance, "settings");
}

void MainWindow::on_actionEditInstNotes_triggered()
{
	APPLICATION->showInstanceWindow(m_selectedInstance, "notes");
}

void MainWindow::on_actionWorlds_triggered()
{
	APPLICATION->showInstanceWindow(m_selectedInstance, "worlds");
}

void MainWindow::on_actionMods_triggered()
{
	APPLICATION->showInstanceWindow(m_selectedInstance, "mods");
}

void MainWindow::on_actionEditInstance_triggered()
{
	APPLICATION->showInstanceWindow(m_selectedInstance);
}

void MainWindow::on_actionScreenshots_triggered()
{
	APPLICATION->showInstanceWindow(m_selectedInstance, "screenshots");
}

void MainWindow::on_actionManageSkins_triggered()
{
	auto account = APPLICATION->accounts()->defaultAccount();
	if (account && account->isMSA() && !account->isActive()) {
		SkinManageDialog dialog(this, account);
		dialog.exec();
	}
}

void MainWindow::on_actionManageAccounts_triggered()
{
	APPLICATION->ShowGlobalSettings(this, "accounts");
}

void MainWindow::on_actionReportBug_triggered()
{
	DesktopServices::openUrl(QUrl(BuildConfig.BUG_TRACKER_URL));
}

void MainWindow::on_actionPatreon_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.PATREON_URL));
}

/*
 * The two ways into the news dialog. They differ only in whether the
 * list of every entry is open: "More news" wants to browse, the
 * headline in the news bar wants that one post.
 */
void MainWindow::showNews(bool withSidebar)
{
	if (!m_newsDialog) {
		m_newsDialog = new NewsViewerDialog(m_newsChecker.get(), this);
		m_newsDialog->setAttribute(Qt::WA_DeleteOnClose);
	}
	m_newsDialog->loadEntries(withSidebar);
	m_newsDialog->show();
	m_newsDialog->raise();
	m_newsDialog->activateWindow();
}

void MainWindow::on_actionMoreNews_triggered()
{
	showNews(true);
}

void MainWindow::newsButtonClicked()
{
	showNews(false);
}

void MainWindow::on_actionAbout_triggered()
{
	AboutDialog dialog(this);
	dialog.exec();
}

void MainWindow::on_actionPlugins_triggered()
{
	PluginsDialog dialog(this);
	dialog.exec();
}

void MainWindow::on_actionMeshMCLogs_triggered()
{
	MeshMCLogsDialog dialog(this);
	dialog.exec();
}

void MainWindow::on_actionDeleteInstance_triggered()
{
	if (!m_selectedInstance) {
		return;
	}

	/* The Delete key reaches this through the list's event filter even
	 * when the toolbar entry is disabled, so the guard has to be here as
	 * well. Trashing a folder Java still has open fails on Windows, and
	 * the fallback below would then start deleting a running game's files
	 * one by one. */
	if (m_selectedInstance->isRunning()) {
		CustomMessageBox::selectable(
			this, tr("Instance is running"),
			tr("\"%1\" is running. Stop it first -- deleting an instance "
			   "while the game has its files open would leave both in a "
			   "mess.")
				.arg(m_selectedInstance->name()),
			QMessageBox::Warning, QMessageBox::Ok)
			->exec();
		return;
	}

	auto id = m_selectedInstance->id();

	/* Shortcuts go with the instance, so say so before it happens
	 * rather than leaving the user to notice their desktop is emptier. */
	const int shortcutCount = m_selectedInstance->shortcuts().size();
	QString shortcutNote;
	if (shortcutCount > 0) {
		shortcutNote = tr("\n\n%n shortcut(s) to it will go the same way.", "",
						  shortcutCount);
	}

	/* Say which of the two this is going to be instead of hedging: with a
	 * trash to move it into the instance can be brought back, and without
	 * one it cannot. */
	const bool recoverable = FS::canTrash();
	const QString fate =
		recoverable
			? tr("It will be moved to the trash. To bring it back, "
				 "right-click the instance list and pick Restore.")
			: tr("There is no usable trash on this system, so this is "
				 "permanent.");

	/* One title for both cases: the difference between recoverable and
	 * permanent is a whole sentence in the body, and shouting CAREFUL at
	 * one of them only makes the other look safe to click through. */
	auto response =
		CustomMessageBox::selectable(
			this, tr("Delete instance?"),
			tr("About to delete: %1\n%2%3\n\nAre you sure?")
				.arg(m_selectedInstance->name(), fate, shortcutNote),
			QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No,
			QMessageBox::No)
			->exec();
	if (response != QMessageBox::Yes) {
		return;
	}

	auto instances = APPLICATION->instances();
	if (!instances->trashInstance(id)) {
		/* canTrash() said yes but the move failed, or it said no in the
		 * first place. Either way the user asked for the instance to go. */
		if (recoverable) {
			qWarning() << "Trashing" << id
					   << "failed after all; deleting it instead.";
		}
		instances->deleteInstance(id);
	}
	ui->actionUndoTrashInstance->setEnabled(instances->trashedSomething());

	/* The instance is gone either way, so stop pointing at it. The rescan
	 * that follows would get here eventually, but until it does the
	 * sidebar is still armed with commands for something that no longer
	 * exists. */
	APPLICATION->settings()->set("SelectedInstance", QString());
	selectionBad();
}

void MainWindow::restoreTrashedInstance()
{
	auto instances = APPLICATION->instances();
	const bool complete = instances->undoTrashInstance();
	ui->actionUndoTrashInstance->setEnabled(instances->trashedSomething());

	if (!complete) {
		CustomMessageBox::selectable(
			this, tr("Restore Instance"),
			tr("Not everything could be put back. Check your trash to "
			   "recover the rest by hand."),
			QMessageBox::Warning, QMessageBox::Ok)
			->exec();
	}
}

void MainWindow::on_actionExportInstance_triggered()
{
	/* The body of the split button. Deliberately the same errand as the
	 * Zip entry rather than a fourth thing: the arrow beside it lists
	 * the formats, so what is left for the button itself is the default
	 * one, not a choice of its own. */
	on_actionExportInstanceZip_triggered();
}

void MainWindow::on_actionExportInstanceZip_triggered()
{
	if (m_selectedInstance) {
		ExportInstanceDialog dlg(m_selectedInstance, this);
		dlg.exec();
	}
}

void MainWindow::on_actionExportInstanceMrPack_triggered()
{
	/* The pack exporters read the component list and the game directory,
	 * so they need more than a BaseInstance. Nothing else can currently
	 * be selected, but a cast that fails is a reason to do nothing
	 * rather than to crash. */
	auto instance =
		std::dynamic_pointer_cast<MinecraftInstance>(m_selectedInstance);
	if (!instance) {
		return;
	}

	ExportPackDialog dlg(instance.get(), this,
						 ExportPackDialog::Format::ModrinthPack);
	dlg.exec();
}

void MainWindow::on_actionExportInstanceFlamePack_triggered()
{
	auto instance =
		std::dynamic_pointer_cast<MinecraftInstance>(m_selectedInstance);
	if (!instance) {
		return;
	}

	/* CurseForge modpacks cannot name a snapshot: its manifest carries a
	 * Minecraft version the platform has to recognise, and it does not
	 * recognise snapshots. Said before the dialog rather than after it,
	 * so nobody fills in a description and picks a filename for a pack
	 * that was never going to be accepted. */
	auto component = instance->getPackProfile()->getComponent("net.minecraft");
	if (component && component->getVersionFile() &&
		component->getVersionFile()->type == QStringLiteral("snapshot")) {
		CustomMessageBox::selectable(
			this, tr("Cannot export"),
			tr("CurseForge modpacks cannot be made from a snapshot version "
			   "of Minecraft."),
			QMessageBox::Warning)
			->exec();
		return;
	}

	ExportPackDialog dlg(instance.get(), this,
						 ExportPackDialog::Format::CurseForgePack);
	dlg.exec();
}

void MainWindow::on_actionCreateInstanceShortcut_triggered()
{
	/* The dialog reads the instance's world folder, so it needs more
	 * than a BaseInstance. Nothing else can currently be selected, but a
	 * cast that fails is a reason to do nothing rather than to crash. */
	auto instance =
		std::dynamic_pointer_cast<MinecraftInstance>(m_selectedInstance);
	if (!instance) {
		return;
	}

	CreateShortcutDialog dlg(instance.get(), this);
	if (dlg.exec() == QDialog::Accepted) {
		dlg.createShortcut();
	}
}

void MainWindow::on_actionRenameInstance_triggered()
{
	if (m_selectedInstance) {
		view->edit(view->currentIndex());
	}
}

void MainWindow::on_actionViewSelectedInstFolder_triggered()
{
	if (m_selectedInstance) {
		QString str = m_selectedInstance->instanceRoot();
		DesktopServices::openDirectory(QDir(str).absolutePath());
	}
}

void MainWindow::on_actionViewSelectedMCFolder_triggered()
{
	if (m_selectedInstance) {
		QString str = m_selectedInstance->gameRoot();
		if (!FS::ensureFilePathExists(str)) {
			// TODO: report error
			return;
		}
		DesktopServices::openDirectory(QDir(str).absolutePath());
	}
}

void MainWindow::on_actionViewSelectedModsFolder_triggered()
{
	if (m_selectedInstance) {
		QString str = m_selectedInstance->modsRoot();
		if (!FS::ensureFilePathExists(str)) {
			// TODO: report error
			return;
		}
		DesktopServices::openDirectory(QDir(str).absolutePath());
	}
}

void MainWindow::closeEvent(QCloseEvent* event)
{
	// Save the window state and geometry.
	APPLICATION->settings()->set("MainWindowState", saveState().toBase64());
	APPLICATION->settings()->set("MainWindowGeometry",
								 saveGeometry().toBase64());
	event->accept();
	emit isClosing();
}

void MainWindow::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange) {
		retranslateUi();
	}
	QMainWindow::changeEvent(event);
}

void MainWindow::instanceActivated(QModelIndex index)
{
	if (!index.isValid())
		return;
	QString id = index.data(InstanceList::InstanceIDRole).toString();
	InstancePtr inst = APPLICATION->instances()->getInstanceById(id);
	if (!inst)
		return;

	activateInstance(inst);
}

void MainWindow::on_actionLaunchInstance_triggered()
{
	if (m_selectedInstance && !m_selectedInstance->isRunning()) {
		APPLICATION->launch(m_selectedInstance);
	}
}

void MainWindow::on_actionKillInstance_triggered()
{
	if (m_selectedInstance && m_selectedInstance->isRunning()) {
		APPLICATION->kill(m_selectedInstance);
	}
}

void MainWindow::activateInstance(InstancePtr instance)
{
	APPLICATION->launch(instance);
}

void MainWindow::on_actionLaunchInstanceOffline_triggered()
{
	if (m_selectedInstance) {
		APPLICATION->launch(m_selectedInstance, LaunchMode::Offline);
	}
}

void MainWindow::taskEnd()
{
	QObject* sender = QObject::sender();
	if (sender == m_versionLoadTask)
		m_versionLoadTask = NULL;

	sender->deleteLater();
}

void MainWindow::startTask(Task* task)
{
	connect(task, &Task::succeeded, this, &MainWindow::taskEnd);
	connect(task, &Task::failed, this, &MainWindow::taskEnd);
	task->start();
}

void MainWindow::instanceChanged(const QModelIndex& current,
								 const QModelIndex& previous)
{
	if (!current.isValid()) {
		APPLICATION->settings()->set("SelectedInstance", QString());
		selectionBad();
		return;
	}
	/* Stop following the instance we are about to let go of. Nothing else
	 * tells the window that an instance started or stopped -- the list model
	 * only signals when a property is written -- so without this the split
	 * Launch/Kill pair would freeze in whatever state it was in when the
	 * instance was selected, leaving Kill greyed out for a running game. */
	if (m_selectedInstance) {
		disconnect(m_selectedInstance.get(), &BaseInstance::runningStatusChanged,
				   this, &MainWindow::refreshCurrentInstance);
		disconnect(m_selectedInstance.get(), &BaseInstance::profilerChanged,
				   this, &MainWindow::updateToolsMenu);
	}

	QString id = current.data(InstanceList::InstanceIDRole).toString();
	m_selectedInstance = APPLICATION->instances()->getInstanceById(id);
	if (m_selectedInstance) {
		connect(m_selectedInstance.get(), &BaseInstance::runningStatusChanged,
				this, &MainWindow::refreshCurrentInstance);
		/* Queued on purpose: the profiler is normally changed by picking an
		 * entry out of this very menu, and the rebuild destroys the action
		 * whose triggered() is still being delivered. Measured (offscreen,
		 * Qt 6.11.2, activation through the popup rather than
		 * QAction::trigger()): a direct connection survives that, so this
		 * is not a fix for a crash we have seen -- it just keeps the action
		 * alive to the end of its own signal instead of relying on Qt's
		 * internal guards. Do not "simplify" it away. */
		connect(m_selectedInstance.get(), &BaseInstance::profilerChanged, this,
				&MainWindow::updateToolsMenu, Qt::QueuedConnection);

		ui->instanceToolBar->setEnabled(true);
		/* Baseline: there is an instance to act on. The rules below then
		 * refine the few that need more than that. */
		for (QAction* action : instance_actions) {
			action->setEnabled(true);
		}
		for (QToolButton* button : instance_buttons) {
			button->setEnabled(true);
		}
		/* Launch and Kill are separate buttons now, so each one just
		 * reflects whether it can do anything right this moment. */
		ui->actionLaunchInstance->setEnabled(m_selectedInstance->canLaunch() &&
											 !m_selectedInstance->isRunning());
		ui->actionKillInstance->setEnabled(m_selectedInstance->isRunning());
		/* Nothing good comes of deleting an instance out from under a
		 * running game; the handler says so too, for the Delete key. */
		ui->actionDeleteInstance->setEnabled(!m_selectedInstance->isRunning());
		ui->actionLaunchInstanceOffline->setEnabled(
			m_selectedInstance->canLaunch());
		ui->actionExportInstance->setEnabled(m_selectedInstance->canExport());
		renameButton->setText(m_selectedInstance->name());
		// The name drives how wide the sidebar wants to be.
		syncSidebarWidths();
		m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
		updateStatusCenter();
		updateInstanceToolIcon(m_selectedInstance->iconKey());

		updateToolsMenu();

		APPLICATION->settings()->set("SelectedInstance",
									 m_selectedInstance->id());
	} else {
		ui->instanceToolBar->setEnabled(false);
		APPLICATION->settings()->set("SelectedInstance", QString());
		selectionBad();
		return;
	}
}

void MainWindow::refreshCurrentInstance()
{
	auto current = view->selectionModel()->currentIndex();
	instanceChanged(current, current);
}

void MainWindow::instanceSelectRequest(QString id)
{
	setSelectedInstanceById(id);
}

void MainWindow::instanceDataChanged(const QModelIndex& topLeft,
									 const QModelIndex& bottomRight)
{
	auto current = view->selectionModel()->currentIndex();
	QItemSelection test(topLeft, bottomRight);
	if (test.contains(current)) {
		instanceChanged(current, current);
	}
}

void MainWindow::selectionBad()
{
	// start by reseting everything...
	m_selectedInstance = nullptr;

	statusBar()->clearMessage();
	ui->instanceToolBar->setEnabled(false);
	/* Greying out the toolbar hides its buttons' state, but the same
	 * actions are in the menu bar, where nothing else would stop them
	 * being clicked with no instance to act on. */
	for (QAction* action : instance_actions) {
		action->setEnabled(false);
	}
	for (QToolButton* button : instance_buttons) {
		button->setEnabled(false);
	}
	renameButton->setText(tr("Rename Instance"));
	syncSidebarWidths();
	updateInstanceToolIcon("grass");

	// ...and then see if we can enable the previously selected instance
	setSelectedInstanceById(
		APPLICATION->settings()->get("SelectedInstance").toString());
}

void MainWindow::checkInstancePathForProblems()
{
	/* Every configured instance folder is checked, not just the primary
	 * one. None of these problems care which of our folders an instance
	 * came out of - Java chokes on '!' in a path wherever that path is,
	 * and an unextracted archive is no less temporary for being the second
	 * folder in the list. Checking one and staying quiet about the rest
	 * would leave the user with instances that fail for a reason we
	 * already knew how to name.
	 *
	 * Each message names the folder it is about, because with more than
	 * one configured "your instance folder" is no longer an answer.
	 */
	auto tempFolderText = tr("This is a problem: <br/>"
							 " - MeshMC will likely be deleted without warning "
							 "by the operating system <br/>"
							 " - close MeshMC now and extract it to a real "
							 "location, not a temporary folder");

	const QStringList instanceFolders =
		APPLICATION->instances()->instanceDirs();
	for (const QString& instanceFolder : instanceFolders) {
		if (FS::checkProblemticPathJava(QDir(instanceFolder))) {
			QMessageBox warning(this);
			warning.setText(
				tr("Your instance folder \'%1\' contains \'!\' and this is "
				   "known to cause Java problems!")
					.arg(instanceFolder));
			warning.setInformativeText(
				tr("You have now two options: <br/>"
				   " - change the instance folder in the settings <br/>"
				   " - move this installation of %1 to a different folder")
					.arg(BuildConfig.MESHMC_NAME));
			warning.setDefaultButton(QMessageBox::Ok);
			warning.exec();
		}

		QString pathfoldername = QDir(instanceFolder).absolutePath();
		if (pathfoldername.contains("Rar$", Qt::CaseInsensitive)) {
			QMessageBox warning(this);
			warning.setText(
				tr("Your instance folder \'%1\' contains \'Rar$\' - that "
				   "means you haven't extracted MeshMC archive!")
					.arg(instanceFolder));
			warning.setInformativeText(tempFolderText);
			warning.setDefaultButton(QMessageBox::Ok);
			warning.exec();
		} else if (pathfoldername.startsWith(QDir::tempPath()) ||
				   pathfoldername.contains("/TempState/")) {
			QMessageBox warning(this);
			warning.setText(
				tr("Your instance folder \'%1\' is in a temporary folder: "
				   "\'%2\'!")
					.arg(instanceFolder, QDir::tempPath()));
			warning.setInformativeText(tempFolderText);
			warning.setDefaultButton(QMessageBox::Ok);
			warning.exec();
		}
	}
}

void MainWindow::updateStatusCenter()
{
	m_statusCenter->setVisible(
		APPLICATION->settings()->get("ShowGlobalGameTime").toBool());

	int timePlayed = APPLICATION->instances()->getTotalPlayTime();
	if (timePlayed > 0) {
		m_statusCenter->setText(
			tr("Total playtime: %1").arg(Time::prettifyDuration(timePlayed)));
	}
}
