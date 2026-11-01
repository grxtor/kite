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
 *
 * Kite Plugin SDK: C++ single-include header for .mmco module development.
 *
 * Includes the C ABI from mmco_c_sdk.h (API table, module info, hook ids, payload structs,
 * MMCO_DEFINE_MODULE) and adds the C++-only parts: the Qt includes a plugin commonly needs and the
 * BasePage interface, kept for ABI and source compatibility (the host no longer calls it; plugin UI
 * goes through ui_surface_create).
 *
 * Usage:
 *   1. #include "mmco_cxx_sdk.hpp" (the only include needed)
 *   2. Define the module with MMCO_DEFINE_MODULE(...) and implement mmco_init() and mmco_unload()
 *   3. Build a shared library with the .mmco extension
 *
 * Plugins must not include Qt or Kite headers directly, and must not fork or exec processes
 * (use process_run).
 */

#pragma once

#ifndef __cplusplus
#error "mmco_cxx_sdk.hpp is the C++ SDK header. C plugins must include mmco_c_sdk.h instead."
#endif

#include "mmco_c_sdk.h"

/* Only QtCore and QtGui headers: ABI 5 dropped the Qt::Widgets link from Kite::SDK. A plugin that
 * needs a real QWidget must link Qt::Widgets itself. */
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

/* BasePage is a frozen ABI shim: the host never calls it, and the page hooks are reserved, never fired. */
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
