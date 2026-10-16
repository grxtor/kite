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

#include "WorldTools.h"

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QProcess>

#include "minecraft/World.h"

namespace WorldTools
{
	bool isWorld(const QString& source)
	{
		const QFileInfo info(source);
		if (!info.exists()) {
			return false;
		}
		return World(info).isValid();
	}

	bool installWorld(const QString& source, const QString& worldsDir)
	{
		World world{QFileInfo(source)};
		if (!world.isValid()) {
			return false;
		}
		return world.install(worldsDir);
	}

	QString mceditProgram(const QString& mceditPath)
	{
		if (mceditPath.trimmed().isEmpty()) {
			return QString();
		}
		const QDir dir(mceditPath);
		if (!dir.exists()) {
			return QString();
		}
#if defined(Q_OS_MACOS)
		// An application bundle, started through `open`.
		return mceditPath;
#elif defined(Q_OS_WIN32)
		for (const char* name : {"mcedit.exe", "mcedit2.exe"}) {
			if (dir.exists(QLatin1String(name))) {
				return dir.absoluteFilePath(QLatin1String(name));
			}
		}
		return QString();
#else
		for (const char* name : {"mcedit.sh", "mcedit.py"}) {
			if (dir.exists(QLatin1String(name))) {
				return dir.absoluteFilePath(QLatin1String(name));
			}
		}
		return QString();
#endif
	}

	QString openInMCEdit(const QString& mceditPath, const QString& worldFolder)
	{
		const QString program = mceditProgram(mceditPath);
		if (program.isEmpty()) {
			return QObject::tr("MCEdit was not found. Set its folder in "
							   "Settings, under External tools.");
		}
		if (!QFileInfo(worldFolder).isDir()) {
			return QObject::tr("This world's folder could not be found.");
		}
#ifdef Q_OS_MACOS
		const bool started = QProcess::startDetached(
			QStringLiteral("/usr/bin/open"),
			{QStringLiteral("-a"), program, QStringLiteral("--args"), worldFolder});
#else
		const bool started =
			QProcess::startDetached(program, {worldFolder}, mceditPath);
#endif
		return started ? QString()
					   : QObject::tr("MCEdit could not be started. It may need "
									 "to be installed again.");
	}
} // namespace WorldTools
