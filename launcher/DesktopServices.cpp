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

#include "DesktopServices.h"
#include <QDir>
#include <QDesktopServices>
#include <QFile>
#include <QProcess>
#include <QDebug>

/** Needed until QTBUG-9328 and other unreported bugs are fixed. */
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

template <typename T>
bool IndirectOpen(T callable, qint64* pid_forked = nullptr)
{
	auto pid = fork();
	if (pid_forked) {
		if (pid > 0)
			*pid_forked = pid;
		else
			*pid_forked = 0;
	}
	if (pid == -1) {
		qWarning() << "IndirectOpen failed to fork: " << errno;
		return false;
	}
	if (pid == 0) {
		// Unset these so they are not passed to the child process.
		qunsetenv("LD_PRELOAD");
		qunsetenv("LD_LIBRARY_PATH");
		qunsetenv("LD_DEBUG");
		qunsetenv("QT_PLUGIN_PATH");
		qunsetenv("QT_FONTPATH");

		auto status = callable();

		setsid();

		// Exit at once without cleanup, which would hang.
		_exit(status ? 0 : 1);
	} else {
		int status;
		while (waitpid(pid, &status, 0)) {
			if (WIFEXITED(status)) {
				return WEXITSTATUS(status) == 0;
			}
			if (WIFSIGNALED(status)) {
				return false;
			}
		}
		return true;
	}
}
#endif

namespace DesktopServices
{
	bool openDirectory(const QString& path, bool ensureExists)
	{
		qDebug() << "Opening directory" << path;
		QDir parentPath;
		QDir dir(path);
		if (!dir.exists()) {
			parentPath.mkpath(dir.absolutePath());
		}
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
		return QProcess::startDetached("xdg-open", QStringList()
													   << dir.absolutePath());
#else
		return QDesktopServices::openUrl(
			QUrl::fromLocalFile(dir.absolutePath()));
#endif
	}

	bool run(const QString& application, const QStringList& args,
			 const QString& workingDirectory, qint64* pid)
	{
		qDebug() << "Running" << application << "with args" << args.join(' ');
		return QProcess::startDetached(application, args, workingDirectory,
									   pid);
	}

	bool openUrl(const QUrl& url)
	{
		qDebug() << "Opening URL" << url.toString();
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD)
		QStringList args;
		args << url.toString();
		return QProcess::startDetached("xdg-open", args);
#else
		return QDesktopServices::openUrl(url);
#endif
	}

	bool isFlatpak()
	{
#ifdef Q_OS_LINUX
		/* Both are set by the runtime for every Flatpak app (the marker file
		 * is what flatpak-spawn looks for); either one alone suffices. */
		static const bool sandboxed =
			QFile::exists(QStringLiteral("/.flatpak-info")) ||
			qEnvironmentVariableIsSet("FLATPAK_ID");
		return sandboxed;
#else
		return false;
#endif
	}

} // namespace DesktopServices
