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

#include <QStringList>

#include "JavaChecker.h"
#include "JavaInstallList.h"

#ifdef Q_OS_WIN
// Without it <windows.h> defines min() and max() macros, and every file that
// includes this header loses std::min and std::max.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

QProcessEnvironment CleanEnviroment();

class JavaUtils : public QObject
{
	Q_OBJECT
  public:
	JavaUtils();

	static QString managedJavaRoot();

	/// The java binary of a runtime folder: the known layouts first
	/// (<dir>/bin, <dir>/Contents/Home/bin, one folder down), the whole tree
	/// only as a last resort. Empty if there is none.
	static QString findJavaBinary(const QString& dir);

	JavaInstallPtr MakeJavaPtr(QString path, QString id = "unknown",
							   QString arch = "unknown");
	QList<QString> FindJavaPaths();
	JavaInstallPtr GetDefaultJava();

#ifdef Q_OS_WIN
	QList<JavaInstallPtr> FindJavaFromRegistryKey(DWORD keyType,
												  QString keyName,
												  QString keyJavaDir,
												  QString subkeySuffix = "");
#endif
};
