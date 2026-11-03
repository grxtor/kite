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

#include <QModelIndex>
#include <QObject>
#include <QString>
#include <memory>

#include "BaseInstance.h"
#include "QObjectPtr.h"

class MinecraftInstance;
class ModFolderModel;
class QSortFilterProxyModel;
class WorldList;
class SettingsAdapter;
class ScreenshotListModel;
class InstanceLogBridge;
class LaunchTask;
class LogModel;
class ContentBrowser;
class OtherLogsModel;
class LoaderInstaller;
class ComponentTools;
class GameOptions;
class KeyValueFilterModel;
class MinecraftVersionListProxy;
class ServersListModel;
class BackupController;
class WorldDataPacksController;
class ManagedPackController;
class InstanceContentController;
class ContentDrop;
class LogUploader;
class ScreenshotUploader;

/*
 * QML bridge for one instance's detail page. Holds the InstancePtr, so the
 * models borrowed from the instance never outlive it.
 */
class InstanceDetails : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString instanceId READ instanceId CONSTANT)
	Q_PROPERTY(QString name READ name NOTIFY nameChanged)
	Q_PROPERTY(QString gameRoot READ gameRoot CONSTANT)
	Q_PROPERTY(QString instanceRoot READ instanceRoot CONSTANT)
	Q_PROPERTY(QObject* settings READ settings CONSTANT)
	Q_PROPERTY(QString notes READ notes WRITE setNotes NOTIFY notesChanged)

	/// Null if this instance is not Minecraft-backed.
	Q_PROPERTY(QObject* mods READ mods CONSTANT)
	Q_PROPERTY(QString modsDir READ modsDir CONSTANT)
	/// False while the instance is running.
	Q_PROPERTY(bool contentChangesAllowed READ contentChangesAllowed NOTIFY
				   contentChangesAllowedChanged)

	/// Created lazily, so opening the page never touches the network.
	Q_PROPERTY(QObject* contentBrowser READ contentBrowser CONSTANT)

	Q_PROPERTY(QObject* worlds READ worlds CONSTANT)
	Q_PROPERTY(QString worldsDir READ worldsDir CONSTANT)
	Q_PROPERTY(QObject* worldDataPacks READ worldDataPacks CONSTANT)

	Q_PROPERTY(QObject* servers READ servers CONSTANT)
	Q_PROPERTY(QString serversDir READ serversDir CONSTANT)

	Q_PROPERTY(QObject* backups READ backups CONSTANT)

	/// Null unless the instance is a Modrinth/CurseForge pack.
	Q_PROPERTY(QObject* managedPack READ managedPack CONSTANT)

	Q_PROPERTY(QObject* screenshots READ screenshots CONSTANT)
	Q_PROPERTY(QString screenshotsDir READ screenshotsDir CONSTANT)

	Q_PROPERTY(QObject* log READ log CONSTANT)
	/// Null if the instance names no log filter.
	Q_PROPERTY(QObject* otherLogs READ otherLogs CONSTANT)
	Q_PROPERTY(QObject* components READ components CONSTANT)
	/// Created lazily.
	Q_PROPERTY(QObject* minecraftVersions READ minecraftVersions CONSTANT)
	/// Created lazily.
	Q_PROPERTY(QObject* loaderInstaller READ loaderInstaller CONSTANT)
	Q_PROPERTY(QObject* componentTools READ componentTools CONSTANT)
	/// options.txt as key/value rows; editable only while the game is not running.
	Q_PROPERTY(QObject* gameOptions READ gameOptions CONSTANT)
	Q_PROPERTY(bool isMinecraft READ isMinecraft CONSTANT)
	/// Old-format instance: needs upgradeLegacy() before it can be played.
	Q_PROPERTY(bool isLegacy READ isLegacy CONSTANT)
	/// Created lazily.
	Q_PROPERTY(QObject* content READ content CONSTANT)
	/// Created lazily: files, folders and links dropped on this instance.
	Q_PROPERTY(QObject* drop READ drop CONSTANT)
	Q_PROPERTY(QObject* logUploader READ logUploader CONSTANT)
	/// `available` is false in a build without an Imgur client id.
	Q_PROPERTY(QObject* screenshotUploader READ screenshotUploader CONSTANT)

  public:
	explicit InstanceDetails(InstancePtr instance, QObject* parent = nullptr);
	~InstanceDetails() override;

	QString instanceId() const;
	QString name() const;
	QString gameRoot() const;
	QString instanceRoot() const;
	QObject* settings() const;

	QString notes() const;
	void setNotes(const QString& notes);

	QObject* mods() const;
	QString modsDir() const;

	bool contentChangesAllowed() const;
	QObject* contentBrowser() const;

	QObject* worlds() const;
	QString worldsDir() const;
	Q_INVOKABLE void deleteWorld(int row);
	/// False (no change) for a name that trims to nothing.
	Q_INVOKABLE bool renameWorld(int row, const QString& name);
	/// Copies off the GUI thread; null for a bad row or a blank name.
	Q_INVOKABLE QObject* copyWorld(int row, const QString& name);
	Q_INVOKABLE bool resetWorldIcon(int row);
	QObject* worldDataPacks() const;

	QObject* servers() const;
	QString serversDir() const;

	QObject* backups() const;

	QObject* managedPack() const;

	QObject* screenshots() const;
	QString screenshotsDir() const;

	QObject* log() const;
	QObject* otherLogs() const;
	QObject* components() const;
	QObject* minecraftVersions() const;
	QObject* loaderInstaller() const;
	QObject* componentTools() const;
	QObject* gameOptions() const;
	bool isMinecraft() const;
	QObject* content() const;

	QObject* drop() const;
	QObject* logUploader() const;
	QObject* screenshotUploader() const;
	/// @p fileUrlOrPath: a file:// URL or a local path. Unpacks off the GUI
	/// thread; null at once if it is not a Minecraft world.
	Q_INVOKABLE QObject* installWorld(const QString& fileUrlOrPath);
	Q_INVOKABLE void refreshWorlds();
	bool isLegacy() const;
	/// Returns a TaskWatcher; null if not legacy, running, or no instance list.
	Q_INVOKABLE QObject* upgradeLegacy();
	/// "available", "noBuild", "loaderTooOld" or "unknown". Read again after the
	/// game has run: that is when the manifest is fetched.
	Q_INVOKABLE QString kiteModSupport() const;
	Q_INVOKABLE bool mceditConfigured() const;
	/// Empty on success, otherwise a message for the user.
	Q_INVOKABLE QString openWorldInMCEdit(int row);

  signals:
	void nameChanged();
	void notesChanged();
	void contentChangesAllowedChanged();

  private slots:
	void onRunningStatusChanged(bool running);

  private:
	InstancePtr m_instance;
	/// Non-owning; null when the instance is not Minecraft.
	MinecraftInstance* m_mc = nullptr;

	/// Borrowed from the instance.
	std::shared_ptr<ModFolderModel> m_mods;
	std::unique_ptr<QSortFilterProxyModel> m_sortedMods;
	/* Only watched here, for InstanceContentController, which serves them. */
	std::shared_ptr<ModFolderModel> m_resourcePacks;
	std::shared_ptr<ModFolderModel> m_shaderPacks;
	std::shared_ptr<ModFolderModel> m_texturePacks;
	std::shared_ptr<WorldList> m_worlds;
	mutable std::unique_ptr<WorldDataPacksController> m_worldDataPacks;
	/// Created on first access.
	mutable std::unique_ptr<ContentBrowser> m_contentBrowser;

	std::unique_ptr<SettingsAdapter> m_settings;
	std::unique_ptr<ScreenshotListModel> m_screenshots;
	/// Owned by QObject parentage.
	InstanceLogBridge* m_log = nullptr;
	OtherLogsModel* m_otherLogs = nullptr;
	/// Parented to `this` in the getter.
	mutable std::unique_ptr<MinecraftVersionListProxy> m_minecraftVersions;
	mutable std::unique_ptr<LoaderInstaller> m_loaderInstaller;
	mutable std::unique_ptr<ComponentTools> m_componentTools;
	/// Borrowed from the instance.
	std::shared_ptr<GameOptions> m_gameOptions;
	/// Created with m_gameOptions; both null for a non-Minecraft instance.
	KeyValueFilterModel* m_gameOptionsFilter = nullptr;

	std::unique_ptr<ServersListModel> m_servers;
	BackupController* m_backups = nullptr;
	mutable std::unique_ptr<ManagedPackController> m_managedPack;
	/// Set once isSupported() has run; the answer cannot change.
	mutable bool m_managedPackChecked = false;
	mutable InstanceContentController* m_content = nullptr;
	mutable ContentDrop* m_drop = nullptr;
	LogUploader* m_logUploader = nullptr;
	ScreenshotUploader* m_screenshotUploader = nullptr;
};

/* QML view of the current LaunchTask's LogModel; model() is null between launches. */
class InstanceLogBridge : public QObject
{
	Q_OBJECT

	/// Null when the instance is not launching/running.
	Q_PROPERTY(QObject* model READ model NOTIFY modelChanged)
	Q_PROPERTY(bool hasLog READ hasLog NOTIFY modelChanged)

  public:
	explicit InstanceLogBridge(InstancePtr instance, QObject* parent = nullptr);
	~InstanceLogBridge() override;

	QObject* model() const;
	bool hasLog() const;

	Q_INVOKABLE void clear();
	Q_INVOKABLE QString text();
	Q_INVOKABLE void setSuspended(bool suspended);
	/// Rows whose line contains @p needle (case-insensitive), ascending.
	Q_INVOKABLE QVariantList findLines(const QString& needle) const;

  signals:
	void modelChanged();

  private slots:
	void onLaunchTaskChanged(shared_qobject_ptr<LaunchTask> task);

  private:
	InstancePtr m_instance;
	shared_qobject_ptr<LaunchTask> m_task;
	shared_qobject_ptr<LogModel> m_model;
};
