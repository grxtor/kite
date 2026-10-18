/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include "core/InstancePathProblems.h"

#include <QCoreApplication>
#include <QDir>

#include "BuildConfig.h"
#include "FileSystem.h"

QList<InstancePathProblem> instancePathProblems(const QStringList& folders,
												const QString& tempPath)
{
	/* Translated in MainWindow's context, where these strings have always
	 * lived, so existing translations keep applying. */
	const QString tempFolderText = QCoreApplication::translate(
		"MainWindow",
		"This is a problem: <br/>"
		" - Kite will likely be deleted without warning "
		"by the operating system <br/>"
		" - close Kite now and extract it to a real "
		"location, not a temporary folder");

	QList<InstancePathProblem> problems;
	for (const QString& folder : folders) {
		if (FS::checkProblemticPathJava(QDir(folder))) {
			problems.append(
				{ folder,
				  QCoreApplication::translate(
					  "MainWindow",
					  "Your instance folder \'%1\' contains \'!\' and this is "
					  "known to cause Java problems!")
					  .arg(folder),
				  QCoreApplication::translate(
					  "MainWindow",
					  "You have now two options: <br/>"
					  " - change the instance folder in the settings <br/>"
					  " - move this installation of %1 to a different folder")
					  .arg(BuildConfig.KITE_NAME) });
		}

		const QString absolute = QDir(folder).absolutePath();
		if (absolute.contains(QLatin1String("Rar$"), Qt::CaseInsensitive)) {
			problems.append(
				{ folder,
				  QCoreApplication::translate(
					  "MainWindow",
					  "Your instance folder \'%1\' contains \'Rar$\' - that "
					  "means you haven't extracted Kite archive!")
					  .arg(folder),
				  tempFolderText });
		} else if (absolute.startsWith(tempPath) ||
				   absolute.contains(QLatin1String("/TempState/"))) {
			problems.append(
				{ folder,
				  QCoreApplication::translate(
					  "MainWindow",
					  "Your instance folder \'%1\' is in a temporary folder: "
					  "\'%2\'!")
					  .arg(folder, tempPath),
				  tempFolderText });
		}
	}
	return problems;
}
