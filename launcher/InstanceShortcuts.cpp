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

#include "InstanceShortcuts.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIcon>
#include <QPixmap>

#include "BuildConfig.h"
#include "DesktopServices.h"
#include "FileSystem.h"
#include "core/LauncherContext.h"
#include "icons/IconList.h"

namespace InstanceShortcuts
{
	namespace
	{
		Result failure(const QString& error)
		{
			Result result;
			result.error = error;
			return result;
		}

		/* Renders the instance's icon into its own folder in the one
		 * format this platform's shell will display, and hands back the
		 * path, or an empty string with @p error set. The launcher itself
		 * only ever holds the icon as a theme key, so without this step a
		 * shortcut has nothing to point at.
		 *
		 * The file name is fixed per instance rather than per shortcut:
		 * several shortcuts to the same instance should share one image,
		 * and deleting the instance takes the folder with it. */
		QString renderIcon(const Request& request, const char* format,
						   const QString& fileName, int size, QString* error)
		{
			const QString key = request.iconKey.isEmpty()
									? request.instance->iconKey()
									: request.iconKey;
			/* getIcon() already falls back to the default instance icon
			 * for a key that no longer resolves, so this is only null
			 * without a running launcher or with an empty icon list. */
			const QIcon icon =
				LAUNCHER ? LAUNCHER->icons()->getIcon(key) : QIcon();

			const QString path =
				FS::PathCombine(request.instance->instanceRoot(), fileName);
			const QString message =
				QObject::tr("Could not write the shortcut's icon to %1.")
					.arg(QDir::toNativeSeparators(path));
			/* A null icon leaves the file alone: it may be the image an
			 * earlier shortcut to this instance still points at. */
			if (icon.isNull()) {
				*error = message;
				return QString();
			}
			if (!icon.pixmap(size, size).save(path, format)) {
				QFile::remove(path);
				*error = message;
				return QString();
			}
			return path;
		}
	} // namespace

	QString folderFor(ShortcutTarget target)
	{
		switch (target) {
			case ShortcutTarget::Desktop:
				return FS::getDesktopDir();
			case ShortcutTarget::Applications: {
				const QString applications = FS::getApplicationsDir();
				if (applications.isEmpty()) {
					return QString();
				}
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
				/* Applications is a flat, shared list on these two, so
				 * keep our instances together instead of scattering them
				 * through it. On the free desktops the folder is a menu
				 * built from .desktop files and a subfolder would simply
				 * not be read. */
				return FS::PathCombine(applications,
									   BuildConfig.KITE_DISPLAYNAME +
										   " Instances");
#else
				return applications;
#endif
			}
			case ShortcutTarget::Other:
				break;
		}
		return QString();
	}

	QString fileSuffix()
	{
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
		return QStringLiteral(".desktop");
#elif defined(Q_OS_WIN)
		return QStringLiteral(".lnk");
#else
		return QString();
#endif
	}

	QString withoutSuffix(QString path)
	{
#if defined(Q_OS_MACOS)
		const QString suffix = QStringLiteral(".app");
#else
		const QString suffix = fileSuffix();
#endif
		if (!suffix.isEmpty() && path.endsWith(suffix, Qt::CaseInsensitive)) {
			path.chop(suffix.length());
		}
		return path;
	}

	Result create(const Request& request, const QString& filePath)
	{
		if (!request.instance) {
			return failure(QObject::tr("Could not create a shortcut to this %1.")
							   .arg(request.targetString));
		}

		QString appPath = QCoreApplication::applicationFilePath();
		QString iconPath;
		QString error;
		QStringList args;

#if defined(Q_OS_MACOS)
		/* A launcher still sitting in the read-only image it was opened
		 * from is unpacked somewhere under /private/var, and that path
		 * stops existing the moment the image is ejected. A shortcut to
		 * it would be dead on arrival. */
		if (appPath.startsWith(QLatin1String("/private/var/"))) {
			return failure(
				QObject::tr("Kite is running from the folder it was "
							"extracted into, so it cannot create a "
							"shortcut. Move it to your Applications "
							"folder first."));
		}

		iconPath = renderIcon(request, "ICNS", QStringLiteral("Icon.icns"),
							  1024, &error);

#elif defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
		/* An AppImage is mounted at a path that changes every run, so the
		 * shortcut has to name the image itself. The runtime tells us
		 * where that is. */
		if (appPath.startsWith(QLatin1String("/tmp/.mount_"))) {
			appPath = qEnvironmentVariable("APPIMAGE");
			if (appPath.isEmpty()) {
				return failure(
					QObject::tr("Kite looks like an AppImage, but "
								"$APPIMAGE is not set, so the path to "
								"write into the shortcut is unknown."));
			}
			while (appPath.endsWith(QLatin1Char('/'))) {
				appPath.chop(1);
			}
		}

		iconPath = renderIcon(request, "PNG", QStringLiteral("icon.png"), 64,
							  &error);

		if (DesktopServices::isFlatpak()) {
			/* Inside the sandbox our own path means nothing to the host
			 * session that will run the shortcut; it has to go through
			 * flatpak run instead. */
			QString appId = qEnvironmentVariable("FLATPAK_ID");
			if (appId.isEmpty()) {
				appId = QStringLiteral("io.github.grxtor.Kite");
			}
			appPath = QStringLiteral("flatpak");
			args << QStringLiteral("run") << appId;
		}

#elif defined(Q_OS_WIN)
		/* Writing the .ico is reported to change the launcher window's
		 * own icon; the widget caller puts it back (see ui/ShortcutUtils),
		 * since only it knows which window that is. */
		iconPath = renderIcon(request, "ICO", QStringLiteral("icon.ico"), 64,
							  &error);

#else
		return failure(
			QObject::tr("Shortcuts are not supported on this platform."));
#endif

		if (iconPath.isEmpty()) {
			// renderIcon() has already said what went wrong.
			return failure(error);
		}

		/* --launch takes the instance id, which is the folder name and
		 * survives renaming the instance; the display name does not. */
		args << QStringLiteral("--launch") << request.instance->id();
		args << request.extraArgs;

		const QString written = FS::createShortcut(filePath, appPath, args,
												   request.name, iconPath);
		if (written.isEmpty()) {
			QFile::remove(iconPath);
			return failure(QObject::tr("Could not create a shortcut to this %1.")
							   .arg(request.targetString));
		}

		/* Hand the path to the instance so that deleting the instance
		 * takes the shortcut with it instead of leaving a file that
		 * launches nothing. */
		request.instance->registerShortcut(
			{request.name, written, request.target});

		Result result;
		result.ok = true;
		result.path = written;
		return result;
	}

	Result createInFolder(const Request& request)
	{
		const QString folder = folderFor(request.target);
		if (folder.isEmpty()) {
			return failure(
				request.target == ShortcutTarget::Applications
					? QObject::tr("This system has no applications folder.")
					: QObject::tr("This system has no desktop folder."));
		}
		if (!QDir(folder).mkpath(QStringLiteral("."))) {
			return failure(QObject::tr("Could not create %1.")
							   .arg(QDir::toNativeSeparators(folder)));
		}

		return create(request,
					  FS::PathCombine(folder, FS::RemoveInvalidFilenameChars(
												  request.name)));
	}
} // namespace InstanceShortcuts
