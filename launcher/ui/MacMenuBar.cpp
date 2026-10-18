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

#include "MacMenuBar.h"

#include <QAction>
#include <QCoreApplication>
#include <QEvent>
#include <QKeySequence>
#include <QList>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QOperatingSystemVersion>
#include <QVariant>

#include "Application.h"
#include "settings/Setting.h"
#include "settings/SettingsObject.h"

namespace
{
	/// Translation context for the menu titles coined in this file.
	const char* const kContext = "MacMenuBar";

	/// Name of the profile menu the main window keeps for its toolbar.
	const char* const kAccountMenuName = "accountMenu";

	/* The menus are described rather than assembled. Every entry is the
	 * object name of an action the main window already owns; kDivider asks
	 * for a rule between two groups.
	 *
	 * Entries that resolve to nothing just fall out. Several actions are
	 * optional -- the bug tracker, Discord and Reddit links depend on the
	 * build config, Plugins on what got loaded, Update on whether this
	 * build can update itself -- and the divider tidy-up in appendGroup()
	 * means a table entry never has to know that. */
	const char* const kDivider = "|";

	inline bool isDivider(const char* entry)
	{
		return entry && entry[0] == '|';
	}

	struct MenuSpec {
		const char* title;          // untranslated, context kContext
		const char* const* entries; // nullptr-terminated
	};

	/* actionSettings and actionAbout carry PreferencesRole and AboutRole,
	 * so macOS lifts them out into the application menu. Both are listed
	 * last and without a divider in front of them: a divider would stay
	 * behind once its only neighbour had been hoisted away. */

	const char* const kFileEntries[] = {
		"actionAddInstance",
		kDivider,
		"actionLaunchInstance",
		"actionLaunchInstanceOffline",
		kDivider,
		"actionCopyInstance",
		"actionExportInstance",
		kDivider,
		"actionDeleteInstance",
		"actionSettings",
		nullptr,
	};

	const char* const kInstanceEntries[] = {
		"actionEditInstance",
		"actionInstanceSettings",
		"actionEditInstNotes",
		kDivider,
		"actionMods",
		"actionWorlds",
		"actionScreenshots",
		kDivider,
		"actionRenameInstance",
		"actionChangeInstIcon",
		"actionChangeInstGroup",
		nullptr,
	};

	const char* const kFolderEntries[] = {
		"actionViewInstanceFolder",
		"actionViewCentralModsFolder",
		kDivider,
		"actionViewSelectedInstFolder",
		"actionViewSelectedMCFolder",
		"actionViewSelectedModsFolder",
		"actionConfig_Folder",
		nullptr,
	};

	/* actionLockToolbars is otherwise reachable only through the toolbar
	 * context menu, which is awkward to hit once the toolbars are locked. */
	const char* const kViewEntries[] = {
		"actionCAT",
		kDivider,
		"actionLockToolbars",
		nullptr,
	};

	const char* const kHelpEntries[] = {
		"actionReportBug",
		"actionDISCORD",
		"actionREDDIT",
		kDivider,
		"actionMoreNews",
		"actionKiteLogs",
		"actionPlugins",
		kDivider,
		"actionCheckUpdate",
		"actionAbout",
		nullptr,
	};

	/// Everything left of the Window menu, in the order macOS expects.
	const MenuSpec kLeadingMenus[] = {
		{QT_TRANSLATE_NOOP("MacMenuBar", "&File"), kFileEntries},
		{QT_TRANSLATE_NOOP("MacMenuBar", "&Instance"), kInstanceEntries},
		{QT_TRANSLATE_NOOP("MacMenuBar", "F&olders"), kFolderEntries},
		{QT_TRANSLATE_NOOP("MacMenuBar", "&View"), kViewEntries},
	};

	/// Help sits last, after Window and Accounts.
	const MenuSpec kHelpMenu = {QT_TRANSLATE_NOOP("MacMenuBar", "&Help"),
								kHelpEntries};

	/**
	 * Fill @p menu, turning null entries and separators into rules.
	 *
	 * A rule is only committed once something follows it, which is what
	 * drops rules that would otherwise lead, trail, or double up after the
	 * absent actions around them have been skipped.
	 *
	 * Returns whether anything at all was added.
	 */
	bool appendGroup(QMenu* menu, const QList<QAction*>& actions)
	{
		bool dividerPending = false;
		bool anyAdded = false;

		for (QAction* action : actions) {
			if (!action || action->isSeparator() || !action->isVisible()) {
				dividerPending = anyAdded;
				continue;
			}
			if (dividerPending) {
				menu->addSeparator();
				dividerPending = false;
			}
			menu->addAction(action);
			anyAdded = true;
		}

		return anyAdded;
	}

	/// Turn a table of object names into actions; dividers become nulls.
	QList<QAction*> resolveEntries(const QMainWindow* window,
								   const char* const* entries)
	{
		QList<QAction*> resolved;

		for (; entries && *entries; ++entries) {
			if (isDivider(*entries)) {
				resolved.append(nullptr);
				continue;
			}
			/* Direct children only. Every toolbar action hangs straight off
			 * the main window, and insisting on that keeps a same-named
			 * action buried in some child widget from being picked up. */
			resolved.append(window->findChild<QAction*>(
				QString::fromLatin1(*entries), Qt::FindDirectChildrenOnly));
		}

		return resolved;
	}

	/// Read the toggle, defaulting to on while settings are still coming up.
	bool settingEnabled()
	{
		if (!APPLICATION || !APPLICATION->settings()) {
			return true;
		}
		return APPLICATION->settings()->get(MacMenuBar::settingKey()).toBool();
	}
} // namespace

QString MacMenuBar::settingKey()
{
	return QStringLiteral("MacNativeMenuBar");
}

QString MacMenuBar::legacySettingKey()
{
	return QStringLiteral("UseMacNativeMenuBar");
}

bool MacMenuBar::platformSupported()
{
	return QOperatingSystemVersion::currentType() ==
		   QOperatingSystemVersion::MacOS;
}

MacMenuBar* MacMenuBar::attachTo(QMainWindow* window)
{
	if (!window) {
		return nullptr;
	}

	auto* existing =
		window->findChild<MacMenuBar*>(QString(), Qt::FindDirectChildrenOnly);
	if (existing) {
		return existing;
	}

	return new MacMenuBar(window);
}

MacMenuBar::MacMenuBar(QMainWindow* window) : QObject(window), m_window(window)
{
	if (!platformSupported()) {
		/* None of this means anything without a screen-top menu bar. The
		 * object is still handed back so callers need no platform test of
		 * their own, and the code below is still compiled everywhere --
		 * it simply never runs here. */
		return;
	}

	/* The actions relabel themselves on a language change -- they belong to
	 * the window -- but the menu titles are coined here, so the bar has to
	 * be built again. */
	m_window->installEventFilter(this);

	if (APPLICATION && APPLICATION->settings()) {
		connect(APPLICATION->settings().get(), &SettingsObject::SettingChanged,
				this, [this](const Setting& setting) {
					if (setting.id() == settingKey()) {
						queueReconcile();
					}
				});
	}

	reconcile();
}

void MacMenuBar::reconcile()
{
	detach();

	if (!platformSupported() || !settingEnabled()) {
		return;
	}
}

void MacMenuBar::detach()
{
	if (m_accountSource) {
		m_accountSource->removeEventFilter(this);
		m_accountSource.clear();
	}
	m_accounts.clear();

	if (m_bar) {
		/* Handing the window a null bar retires the old one for us. Its
		 * menus go with it; the actions inside them do not, because they
		 * are the window's children and always were. */
		m_window->setMenuBar(nullptr);
		m_bar.clear();
	}
}

void MacMenuBar::syncAccounts()
{
	if (!m_accounts) {
		return;
	}

	/* Only the rules were created here, so clearing takes them and leaves
	 * the window's account actions -- which are merely on loan -- alone. */
	m_accounts->clear();

	if (!m_accountSource) {
		return;
	}
	appendGroup(m_accounts.data(), m_accountSource->actions());
}

void MacMenuBar::queueReconcile()
{
	if (m_reconcileQueued) {
		return;
	}
	m_reconcileQueued = true;

	QMetaObject::invokeMethod(
		this,
		[this] {
			m_reconcileQueued = false;
			reconcile();
		},
		Qt::QueuedConnection);
}

void MacMenuBar::queueAccountSync()
{
	if (m_accountSyncQueued) {
		return;
	}
	m_accountSyncQueued = true;

	QMetaObject::invokeMethod(
		this,
		[this] {
			m_accountSyncQueued = false;
			syncAccounts();
		},
		Qt::QueuedConnection);
}

bool MacMenuBar::eventFilter(QObject* watched, QEvent* event)
{
	if (watched == m_window) {
		if (event->type() == QEvent::LanguageChange) {
			queueReconcile();
		}
	} else if (watched == m_accountSource.data()) {
		switch (event->type()) {
			case QEvent::ActionAdded:
			case QEvent::ActionChanged:
			case QEvent::ActionRemoved:
				queueAccountSync();
				break;
			default:
				break;
		}
	}

	return QObject::eventFilter(watched, event);
}
