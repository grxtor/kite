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

#include "ui/ShortcutUtils.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QIcon>
#include <QMessageBox>
#include <QUrl>
#include <QWidget>

#include "Application.h"
#include "BaseInstance.h"
#include "BuildConfig.h"
#include "DesktopServices.h"
#include "FileSystem.h"
#include "icons/IconList.h"

namespace ShortcutUtils
{
	namespace
	{
		QString title()
		{
			return QObject::tr("Create Shortcut");
		}

		void complain(QWidget* parent, const QString& what)
		{
			QMessageBox::critical(parent, title(), what);
		}

		void report(QWidget* parent, const QString& what)
		{
			QMessageBox::information(parent, title(), what);
		}

		/* Renders the instance's icon into its own folder in the one
		 * format this platform's shell will display, and hands back the
		 * path. The launcher itself only ever holds the icon as a theme
		 * key, so without this step a shortcut has nothing to point at.
		 *
		 * The file name is fixed per instance rather than per shortcut:
		 * several shortcuts to the same instance should share one image,
		 * and deleting the instance takes the folder with it. */
		QString renderIcon(const Shortcut& shortcut, const char* format,
						   const QString& fileName, int size)
		{
			const QString key = shortcut.iconKey.isEmpty()
									? shortcut.instance->iconKey()
									: shortcut.iconKey;
			/* getIcon() already falls back to the default instance icon
			 * for a key that no longer resolves, so this cannot be null
			 * unless the icon list itself is empty. */
			const QIcon icon = APPLICATION->icons()->getIcon(key);

			const QString path =
				FS::PathCombine(shortcut.instance->instanceRoot(), fileName);
			if (!icon.pixmap(size, size).save(path, format)) {
				QFile::remove(path);
				complain(shortcut.parent,
						 QObject::tr("Could not write the shortcut's icon to "
									 "%1.")
							 .arg(QDir::toNativeSeparators(path)));
				return QString();
			}
			return path;
		}
	} // namespace

	bool createInstanceShortcut(const Shortcut& shortcut,
								const QString& filePath)
	{
		if (!shortcut.instance) {
			return false;
		}

		QString appPath = QApplication::applicationFilePath();
		QString iconPath;
		QStringList args;

#if defined(Q_OS_MACOS)
		/* A launcher still sitting in the read-only image it was opened
		 * from is unpacked somewhere under /private/var, and that path
		 * stops existing the moment the image is ejected. A shortcut to
		 * it would be dead on arrival. */
		if (appPath.startsWith(QLatin1String("/private/var/"))) {
			complain(shortcut.parent,
					 QObject::tr("MeshMC is running from the folder it was "
								 "extracted into, so it cannot create a "
								 "shortcut. Move it to your Applications "
								 "folder first."));
			return false;
		}

		iconPath = renderIcon(shortcut, "ICNS", QStringLiteral("Icon.icns"),
							  1024);

#elif defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
		/* An AppImage is mounted at a path that changes every run, so the
		 * shortcut has to name the image itself. The runtime tells us
		 * where that is. */
		if (appPath.startsWith(QLatin1String("/tmp/.mount_"))) {
			appPath = qEnvironmentVariable("APPIMAGE");
			if (appPath.isEmpty()) {
				complain(shortcut.parent,
						 QObject::tr("MeshMC looks like an AppImage, but "
									 "$APPIMAGE is not set, so the path to "
									 "write into the shortcut is unknown."));
				return false;
			}
			while (appPath.endsWith(QLatin1Char('/'))) {
				appPath.chop(1);
			}
		}

		iconPath = renderIcon(shortcut, "PNG", QStringLiteral("icon.png"), 64);

		if (DesktopServices::isFlatpak()) {
			/* Inside the sandbox our own path means nothing to the host
			 * session that will run the shortcut; it has to go through
			 * flatpak run instead. */
			QString appId = qEnvironmentVariable("FLATPAK_ID");
			if (appId.isEmpty()) {
				appId = QStringLiteral("org.projecttick.MeshMC");
			}
			appPath = QStringLiteral("flatpak");
			args << QStringLiteral("run") << appId;
		}

#elif defined(Q_OS_WIN)
		/* Writing an .ico here is reported to leave the launcher wearing
		 * the icon that was just written. It could not be reproduced with
		 * Qt 6.11 on the offscreen platform, so rather than trust either
		 * answer the icon is simply put back: restoring one that never
		 * changed costs nothing.
		 *
		 * It is the window's icon and not the application's, because
		 * MeshMC only ever sets the former. */
		QWidget* window = shortcut.parent ? shortcut.parent->window() : nullptr;
		const QIcon windowIcon = window ? window->windowIcon() : QIcon();

		iconPath = renderIcon(shortcut, "ICO", QStringLiteral("icon.ico"), 64);

		if (window) {
			window->setWindowIcon(windowIcon);
		}

#else
		complain(shortcut.parent,
				 QObject::tr("Shortcuts are not supported on this platform."));
		return false;
#endif

		if (iconPath.isEmpty()) {
			// renderIcon() has already said what went wrong.
			return false;
		}

		/* --launch takes the instance id, which is the folder name and
		 * survives renaming the instance; the display name does not. */
		args << QStringLiteral("--launch") << shortcut.instance->id();
		args << shortcut.extraArgs;

		const QString written = FS::createShortcut(filePath, appPath, args,
												   shortcut.name, iconPath);
		if (written.isEmpty()) {
			QFile::remove(iconPath);
			complain(shortcut.parent,
					 QObject::tr("Could not create a shortcut to this %1.")
						 .arg(shortcut.targetString));
			return false;
		}

		/* Hand the path to the instance so that deleting the instance
		 * takes the shortcut with it instead of leaving a file that
		 * launches nothing. */
		shortcut.instance->registerShortcut(
			{shortcut.name, written, shortcut.target});
		return true;
	}

	bool createInstanceShortcutOnDesktop(const Shortcut& shortcut)
	{
		if (!shortcut.instance) {
			return false;
		}

		const QString desktop = FS::getDesktopDir();
		if (desktop.isEmpty()) {
			complain(shortcut.parent,
					 QObject::tr("This system has no desktop folder."));
			return false;
		}

		const QString path = FS::PathCombine(
			desktop, FS::RemoveInvalidFilenameChars(shortcut.name));
		if (!createInstanceShortcut(shortcut, path)) {
			return false;
		}

		report(shortcut.parent,
			   QObject::tr("Created a shortcut to this %1 on your desktop.")
				   .arg(shortcut.targetString));
		return true;
	}

	bool createInstanceShortcutInApplications(const Shortcut& shortcut)
	{
		if (!shortcut.instance) {
			return false;
		}

		QString applications = FS::getApplicationsDir();
		if (applications.isEmpty()) {
			complain(shortcut.parent,
					 QObject::tr("This system has no applications folder."));
			return false;
		}

#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
		/* Applications is a flat, shared list on these two, so keep our
		 * instances together instead of scattering them through it. On
		 * the free desktops the folder is a menu built from .desktop
		 * files and a subfolder would simply not be read. */
		applications = FS::PathCombine(
			applications, BuildConfig.MESHMC_DISPLAYNAME + " Instances");
		if (!QDir(applications).mkpath(".")) {
			complain(shortcut.parent,
					 QObject::tr("Could not create %1.")
						 .arg(QDir::toNativeSeparators(applications)));
			return false;
		}
#endif

		const QString path = FS::PathCombine(
			applications, FS::RemoveInvalidFilenameChars(shortcut.name));
		if (!createInstanceShortcut(shortcut, path)) {
			return false;
		}

		report(shortcut.parent,
			   QObject::tr(
				   "Created a shortcut to this %1 in your applications folder.")
				   .arg(shortcut.targetString));
		return true;
	}

	bool createInstanceShortcutInOther(const Shortcut& shortcut)
	{
		if (!shortcut.instance) {
			return false;
		}

#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
		const QString suffix = QStringLiteral(".desktop");
		const QString filter = QObject::tr("Desktop entries") + " (*.desktop)";
#elif defined(Q_OS_WIN)
		const QString suffix = QStringLiteral(".lnk");
		const QString filter = QObject::tr("Shortcuts") + " (*.lnk)";
#else
		const QString suffix = QString();
		const QString filter = QObject::tr("Applications") + " (*)";
#endif

		const QString desktop = FS::getDesktopDir();
		const QString suggestion = FS::PathCombine(
			desktop, FS::RemoveInvalidFilenameChars(shortcut.name) + suffix);

		/* An instance rather than QFileDialog::getSaveFileName(): the
		 * static helper builds its own dialog, so there is nowhere to
		 * point the XDG portal at, and on a sandboxed desktop it opens
		 * wherever it last was instead of next to the suggested name. */
		QFileDialog picker(shortcut.parent, title(), desktop, filter);
		picker.setAcceptMode(QFileDialog::AcceptSave);
		picker.setDirectoryUrl(QUrl::fromLocalFile(desktop));
		if (!suffix.isEmpty()) {
			picker.setDefaultSuffix(suffix.mid(1));
		}
		picker.selectFile(suggestion);
		if (picker.exec() != QDialog::Accepted) {
			return false; // cancelled
		}

		QString chosen = picker.selectedFiles().value(0);
		if (chosen.isEmpty()) {
			return false;
		}

		// FS::createShortcut() appends the suffix itself.
		if (!suffix.isEmpty() && chosen.endsWith(suffix, Qt::CaseInsensitive)) {
			chosen.chop(suffix.length());
		}

		if (!createInstanceShortcut(shortcut, chosen)) {
			return false;
		}

		report(shortcut.parent,
			   QObject::tr("Created a shortcut to this %1.")
				   .arg(shortcut.targetString));
		return true;
	}
} // namespace ShortcutUtils
