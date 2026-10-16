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

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

/*
 * Where MeshMC keeps its folders: the widget-free half of MeshMCPage's
 * "Folders" box. The five single folders (instances, mods, icons, skins,
 * Java) are moved with setFolder(); extra instance folders are added and
 * removed with addAdditionalDir() / removeAdditionalDir().
 *
 * Both run the checks the widget page ran, and ask through
 * LauncherContext::uiHost(), so they work under either interface:
 *  - an instance folder with a '!' in its path (Java mis-reads it) or one
 *    granted through a Flatpak portal (gone after a restart) is only used
 *    if the user insists;
 *  - an extra instance folder that is already in use is refused, because
 *    two entries for one folder make every instance in it read as a
 *    duplicate.
 *
 * InstanceList already re-reads its folders when InstanceDir or
 * AdditionalInstanceDirs changes, so nothing here reloads the library.
 * Existing instances are not moved: only where MeshMC looks changes.
 */
class FolderSettingsController : public QObject
{
	Q_OBJECT

	/// The extra instance folders, in order.
	Q_PROPERTY(QStringList additionalDirs READ additionalDirs NOTIFY
				   additionalDirsChanged)

  public:
	explicit FolderSettingsController(QObject* parent = nullptr);
	~FolderSettingsController() override;

	QStringList additionalDirs() const;

	/*!
	 * Points the setting @p key (InstanceDir, CentralModsDir, IconsDir,
	 * SkinsDir or JavaDir) at the folder @p url, after the checks above.
	 * Returns whether the setting changed: false for an unknown key, a
	 * folder that does not exist, or a user who said no.
	 */
	Q_INVOKABLE bool setFolder(const QString& key, const QUrl& url);

	/// Adds @p url to the extra instance folders. Returns whether it was
	/// added.
	Q_INVOKABLE bool addAdditionalDir(const QUrl& url);

	/// Takes @p dir out of the extra instance folders.
	Q_INVOKABLE void removeAdditionalDir(const QString& dir);

	/// The folder settings setFolder() accepts.
	static bool isFolderKey(const QString& key);

	/*!
	 * Why @p cookedDir is a poor instance folder, or an empty string if it
	 * is fine. @p rawDir is what the file dialog returned, before
	 * normalising: the Flatpak check needs it, because the cooked path no
	 * longer says where it came from.
	 */
	static QString instanceDirWarning(const QString& rawDir,
									  const QString& cookedDir, bool flatpak);

	/// Why @p cookedDir cannot be added as an extra instance folder because
	/// it is already in use, or an empty string if it can. Compared without
	/// regard to case, since on a case-insensitive filesystem two spellings
	/// are one folder.
	static QString duplicateDirMessage(const QString& cookedDir,
									   const QString& primaryDir,
									   const QStringList& additionalDirs);

  signals:
	void additionalDirsChanged();
};
