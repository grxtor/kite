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

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

/*
 * Instance folders that are known to cause trouble -- the checks
 * MainWindow::checkInstancePathForProblems() used to run inline, here so
 * that the QML shell can run them too and report them through UiHost:
 *   - a '!' anywhere in the path, which Java mis-reads;
 *   - a folder inside an archive viewer's unpack area ("Rar$"), or inside
 *     a temporary folder, which the system may delete without warning.
 * Every configured folder is checked, and every problem names the folder
 * it is about.
 */
struct InstancePathProblem {
	QString folder;
	/// What is wrong; may contain simple HTML (<br/>).
	QString text;
	/// What to do about it; may contain simple HTML.
	QString advice;
};

/* @p tempPath is the system's temporary folder (QDir::tempPath() in
 * production), passed in so the rule can be tested. */
QList<InstancePathProblem> instancePathProblems(const QStringList& folders,
												const QString& tempPath);
