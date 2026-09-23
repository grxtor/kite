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
 *
 * MeshMC Plugin SDK - C++ single-include header for .mmco module development.
 *
 * This is the header C++ plugins include. It pulls in the canonical,
 * language-neutral C ABI from mmco_c_sdk.h (the full data-plane API,
 * module-info struct, hook ids, payload structs and the
 * MMCO_DEFINE_MODULE family) and layers the C++-only facilities on top:
 *
 *   - The Qt includes a plugin commonly needs (widgets, dialogs, layout,
 *     JSON, etc.), so a plugin source only needs this one #include.
 *   - The BasePage interface, used to subclass UI pages handed back to
 *     the host through MMCO_HOOK_UI_INSTANCE_PAGES /
 *     MMCO_HOOK_UI_GLOBAL_SETTINGS_PAGES.
 *
 * USAGE (C++):
 *   1. #include "mmco_cxx_sdk.h" in your plugin source (this is the ONLY
 *      include you need - Qt and the MMCO ABI come in automatically).
 *   2. Define the module via MMCO_DEFINE_MODULE(...), and implement
 *      mmco_init() and mmco_unload().
 *   3. Compile as a shared library with the .mmco extension.
 *
 * Plugins MUST NOT:
 *   - Directly #include Qt or MeshMC headers (use this SDK header instead)
 *   - Fork or exec processes
 *
 * Plugins CAN:
 *   - Use Qt types and widgets (provided through this header)
 *   - Subclass BasePage to build UI pages
 *   - Register for hooks to observe/modify launcher behaviour
 *   - Read/write their own settings (namespaced automatically)
 *   - Query and manage instances fully (launch, stop, mods, worlds, etc.)
 *   - Query accounts and Java installations
 *   - Make HTTP requests through the provided API
 *   - Show dialogs (file chooser, input, confirm, message)
 *   - Create/extract zip archives (via the host)
 *   - Perform filesystem operations
 */

#pragma once

#ifndef __cplusplus
#error "mmco_cxx_sdk.hpp is the C++ SDK header. C plugins must include mmco_c_sdk.h instead."
#endif

/* The canonical, language-neutral C ABI: all structs, enums, the
 * MMCOContext function table, and the MMCO_DEFINE_MODULE family. */
#include "mmco_c_sdk.h"

/* ── Qt facilities available to C++ plugins ─────────────────────────── */
/* ABI 5 dropped the Qt::Widgets link from MeshMC::SDK (see
 * plugin-abi5-spec.md §4 step 3 and sdk/CMakeLists.txt): every ui_ and
 * tray_menu_ call a plugin used to build QWidget/QMenu/QAction trees
 * with by hand is gone, replaced by ui_surface_create's "mmco-ui/1"
 * JSON documents (rendered host-side, MeshMC_logic-only, by
 * PluginUiRenderer) and tray_set_menu's JSON menu doc. Only QtCore and
 * QtGui headers are pulled in below; a plugin that still needs a
 * genuine QWidget (e.g. to parent a native dialog some other way) must
 * link Qt::Widgets itself and include the specific header it needs. */
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QIcon>
#include <QPointer>
#include <QTimer>
#include <QJsonDocument>
#include <QLocale>

/*
 * --- BasePage / BasePageContainer ---
 *
 * BasePage is a pure header-only interface (every method is inline);
 * we copy its declaration here verbatim so plugins can subclass it
 * for MMCO_HOOK_UI_INSTANCE_PAGES / MMCO_HOOK_UI_GLOBAL_SETTINGS_PAGES
 * payloads without including launcher/ui/pages/BasePage.h. The vtable
 * layout MUST stay byte-identical to launcher/ui/pages/BasePage.h -
 * the host iterates the QList<BasePage*> handed back by plugins and
 * calls these virtuals through that layout. If you change BasePage on
 * the launcher side you must mirror the change here in lock-step.
 */
class BasePageContainer; // forward-declared, plugin never deref's it

class BasePage
{
  public:
	virtual ~BasePage() {}
	virtual QString id() const = 0;
	virtual QString displayName() const = 0;
	virtual QIcon icon() const = 0;
	virtual bool apply()
	{
		return true;
	}
	virtual bool shouldDisplay() const
	{
		return true;
	}
	virtual QString helpPage() const
	{
		return QString();
	}
	void opened()
	{
		isOpened = true;
		openedImpl();
	}
	void closed()
	{
		isOpened = false;
		closedImpl();
	}
	virtual void openedImpl() {}
	virtual void closedImpl() {}
	virtual void setParentContainer(BasePageContainer* container)
	{
		m_container = container;
	}

  public:
	int stackIndex = -1;
	int listIndex = -1;

  protected:
	BasePageContainer* m_container = nullptr;
	bool isOpened = false;
};
