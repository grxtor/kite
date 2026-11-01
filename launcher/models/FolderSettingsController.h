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

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

/*
 * Where Kite keeps its folders. setFolder() moves the five single folders
 * (instances, mods, icons, skins, Java); addAdditionalDir()/removeAdditionalDir()
 * manage extra instance folders. Checks ask through LauncherContext::uiHost():
 *  - an instance folder with a '!' in its path (Java mis-reads it) or granted
 *    through a Flatpak portal (gone after a restart) is only used if the user
 *    insists;
 *  - an extra instance folder already in use is refused: two entries for one
 *    folder make every instance in it read as a duplicate.
 * InstanceList re-reads its folders when InstanceDir or AdditionalInstanceDirs
 * changes. Existing instances are not moved.
 */
class FolderSettingsController : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QStringList additionalDirs READ additionalDirs NOTIFY
				   additionalDirsChanged)

  public:
	explicit FolderSettingsController(QObject* parent = nullptr);
	~FolderSettingsController() override;

	QStringList additionalDirs() const;

	/// Points the setting @p key (InstanceDir, CentralModsDir, IconsDir, SkinsDir or
	/// JavaDir) at @p url after the checks. False for an unknown key, a missing
	/// folder or a user who said no.
	Q_INVOKABLE bool setFolder(const QString& key, const QUrl& url);

	Q_INVOKABLE bool addAdditionalDir(const QUrl& url);

	Q_INVOKABLE void removeAdditionalDir(const QString& dir);

	static bool isFolderKey(const QString& key);

	/// Why @p cookedDir is a poor instance folder, or empty. @p rawDir is what the
	/// file dialog returned: the Flatpak check needs it, the cooked path no longer
	/// says where it came from.
	static QString instanceDirWarning(const QString& rawDir,
									  const QString& cookedDir, bool flatpak);

	/// Why @p cookedDir is already in use as an instance folder, or empty. Compared
	/// case-insensitively: on such a filesystem two spellings are one folder.
	static QString duplicateDirMessage(const QString& cookedDir,
									   const QString& primaryDir,
									   const QStringList& additionalDirs);

  signals:
	void additionalDirsChanged();
};
