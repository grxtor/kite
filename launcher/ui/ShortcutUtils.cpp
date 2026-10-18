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

#include "ui/ShortcutUtils.h"

#include <QDir>
#include <QFileDialog>
#include <QIcon>
#include <QMessageBox>
#include <QUrl>
#include <QWidget>

#include <functional>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "InstanceShortcuts.h"

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

		InstanceShortcuts::Request requestFor(const Shortcut& shortcut,
											  ShortcutTarget target)
		{
			InstanceShortcuts::Request request;
			request.instance = shortcut.instance;
			request.name = shortcut.name;
			request.targetString = shortcut.targetString;
			request.extraArgs = shortcut.extraArgs;
			request.iconKey = shortcut.iconKey;
			request.target = target;
			return request;
		}

		/* Runs @p write and puts any failure in front of the user.
		 *
		 * On Windows, writing an .ico is reported to leave the launcher
		 * wearing the icon that was just written. It could not be
		 * reproduced with Qt 6.11 on the offscreen platform, so rather
		 * than trust either answer the icon is simply put back: restoring
		 * one that never changed costs nothing. It is the window's icon
		 * and not the application's, because Kite only ever sets the
		 * former. */
		bool run(const Shortcut& shortcut,
				 const std::function<InstanceShortcuts::Result()>& write)
		{
#if defined(Q_OS_WIN)
			QWidget* window =
				shortcut.parent ? shortcut.parent->window() : nullptr;
			const QIcon windowIcon = window ? window->windowIcon() : QIcon();
#endif
			const InstanceShortcuts::Result result = write();
#if defined(Q_OS_WIN)
			if (window) {
				window->setWindowIcon(windowIcon);
			}
#endif
			if (!result.ok) {
				complain(shortcut.parent, result.error);
			}
			return result.ok;
		}
	} // namespace

	bool createInstanceShortcut(const Shortcut& shortcut,
								const QString& filePath)
	{
		if (!shortcut.instance) {
			return false;
		}
		return run(shortcut, [&] {
			return InstanceShortcuts::create(
				requestFor(shortcut, shortcut.target), filePath);
		});
	}

	bool createInstanceShortcutOnDesktop(const Shortcut& shortcut)
	{
		if (!shortcut.instance) {
			return false;
		}
		if (!run(shortcut, [&] {
				return InstanceShortcuts::createInFolder(
					requestFor(shortcut, ShortcutTarget::Desktop));
			})) {
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
		if (!run(shortcut, [&] {
				return InstanceShortcuts::createInFolder(
					requestFor(shortcut, ShortcutTarget::Applications));
			})) {
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

		const QString suffix = InstanceShortcuts::fileSuffix();
#if defined(Q_OS_LINUX) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD)
		const QString filter = QObject::tr("Desktop entries") + " (*.desktop)";
#elif defined(Q_OS_WIN)
		const QString filter = QObject::tr("Shortcuts") + " (*.lnk)";
#else
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
