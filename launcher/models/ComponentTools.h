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
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <memory>

class MinecraftInstance;
class PackProfile;

/*
 * The Version tab's less common actions, for QML: what VersionPage's toolbar
 * did beyond changing versions. Created lazily by
 * InstanceDetails::componentTools() for a Minecraft instance.
 *
 *  - addEmpty(): an empty component with a uid and a name, which the user
 *    then fills in by editing its JSON (the old NewComponentDialog);
 *  - addJarMods() / replaceMainJar(): put files into Minecraft.jar or swap
 *    the jar itself;
 *  - downloadAll(): fetch every library and asset again;
 *  - editComponentJson(): open a component's patch file in the JSON editor
 *    set in Settings, or the system's default for .json files;
 *  - openLibrariesFolder() / openGameFolder().
 *
 * Whether the game is running is the caller's to check (the tab greys these
 * out through InstanceDetails::contentChangesAllowed); nothing here guards
 * against it beyond what PackProfile itself refuses.
 */
class ComponentTools : public QObject
{
	Q_OBJECT

	/// Why the last action failed, in a sentence; empty after a success.
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)

  public:
	explicit ComponentTools(std::shared_ptr<MinecraftInstance> instance,
							QObject* parent = nullptr);
	~ComponentTools() override;

	QString error() const
	{
		return m_error;
	}

	/// "My Mod!" -> "org.projecttick.custom.mymod": the uid offered when
	/// the user leaves the field empty. Empty if @p name has no letters.
	Q_INVOKABLE static QString suggestUid(const QString& name);

	/*!
	 * Adds an empty component named @p name. @p uid may be empty, in which
	 * case suggestUid(@p name) is used. Returns an empty string on success,
	 * or what is wrong (no name, no uid, a uid already in this instance).
	 */
	Q_INVOKABLE QString addEmpty(const QString& name, const QString& uid);

	/// The first problem with a new component, or an empty string. Public
	/// so the dialog can check as the user types: @p existingUids are the
	/// uids already in the instance.
	Q_INVOKABLE static QString validateNewComponent(
		const QString& name, const QString& uid, const QStringList& existingUids);

	/// The uids of the instance's components, for validateNewComponent().
	Q_INVOKABLE QStringList componentUids() const;

	/// Adds @p files (local files from a file dialog) to Minecraft.jar.
	/// Returns false if none of them is an existing file.
	Q_INVOKABLE bool addJarMods(const QList<QUrl>& files);

	/// Replaces Minecraft.jar with @p file. Returns false if it is not an
	/// existing file.
	Q_INVOKABLE bool replaceMainJar(const QUrl& file);

	/*!
	 * Downloads the game's libraries and assets again. Returns a
	 * TaskWatcher for the download, or null with error() set (no valid
	 * account, nothing to update).
	 */
	Q_INVOKABLE QObject* downloadAll();

	/// Whether component @p row has a patch file on disk to edit.
	Q_INVOKABLE bool hasJsonFile(int row) const;

	/// Opens component @p row's patch file in the JSON editor. Returns
	/// false, with error() set, if there is nothing to open.
	Q_INVOKABLE bool editComponentJson(int row);

	Q_INVOKABLE void openLibrariesFolder() const;
	Q_INVOKABLE void openGameFolder() const;

	/// The local paths of @p urls that are existing files.
	static QStringList existingFiles(const QList<QUrl>& urls);

  signals:
	void errorChanged();

  private:
	void setError(const QString& error);
	PackProfile* profile() const;

	std::shared_ptr<MinecraftInstance> m_instance;
	QString m_error;
};
