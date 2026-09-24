/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include <QAbstractListModel>

#include <QString>
#include <QList>
#include <memory>

#include "Library.h"
#include "LaunchProfile.h"
#include "Component.h"
#include "ProfileUtils.h"
#include "BaseVersion.h"
#include "MojangDownloadInfo.h"
#include "net/Mode.h"

class MinecraftInstance;
struct PackProfileData;
class ComponentUpdateTask;
class TaskWatcher;

class PackProfile : public QAbstractListModel
{
	Q_OBJECT
	friend ComponentUpdateTask;

	/// The ComponentUpdateTask behind the last reload()/resolve() call
	/// (see those and changeComponentVersion()/setComponentEnabled()
	/// below, which route through resolve()), wrapped for QML - see
	/// TaskWatcher's own class comment. Null until the first one runs;
	/// past that, always the most recent one, whether it is still
	/// running or has already finished. Parented to `this`, like
	/// ContentBrowser::install()'s own watcher, so QML need not manage
	/// its lifetime.
	Q_PROPERTY(QObject* task READ task NOTIFY taskChanged)
	/// Whether a ComponentUpdateTask is currently running - QML's cue to
	/// disable the Version tab's actions the same way VersionPage's
	/// `controlsEnabled` did while running.
	Q_PROPERTY(bool busy READ busy NOTIFY taskChanged)
	/// The reason the last mutating call below failed, or empty. Cleared
	/// on the next attempt, successful or not.
	Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

  public:
	enum Columns { NameColumn = 0, VersionColumn, NUM_COLUMNS };

	// Column-independent roles for QML consumers. Kept separate from
	// Columns above, which the QtWidgets tree view still relies on.
	//
	// Appended after ProblemSeverityRole, not interleaved, so the three
	// original values never change - see VersionTab.qml for the QML
	// Version tab these back.
	enum ModelRoles {
		NameRole = Qt::UserRole,
		VersionRole,
		ProblemSeverityRole,
		UidRole,
		IsCustomRole,
		IsEnabledRole,
		CanDisableRole,
		IsRemovableRole,
		IsMoveableRole,
		IsCustomizableRole,
		IsRevertibleRole,
		/// Whether the metadata index knows a version list for this
		/// component's uid at all - the QML "Change version" action is
		/// gated on this rather than on Component::isVersionChangeable(),
		/// which would have to call Meta::VersionList::load() to answer
		/// precisely (list loaded and non-empty) and
		/// Meta::BaseEntity::load() starts a fresh download on every call
		/// that is not already in flight (see LoaderVersionPage::reload()'s
		/// own comment on this) - doing that from data(), read once per
		/// visible row on every relayout, would hammer the metadata
		/// server. A uid with a list that turns out empty once the picker
		/// actually loads it shows that picker's own empty state instead
		/// (see VersionTab.qml).
		HasVersionListRole
	};

	explicit PackProfile(MinecraftInstance* instance);
	virtual ~PackProfile();

	virtual QVariant data(const QModelIndex& index,
						  int role = Qt::DisplayRole) const override;
	virtual bool setData(const QModelIndex& index, const QVariant& value,
						 int role = Qt::EditRole) override;
	virtual QVariant headerData(int section, Qt::Orientation orientation,
								int role) const override;
	virtual int
	rowCount(const QModelIndex& parent = QModelIndex()) const override;
	virtual int columnCount(const QModelIndex& parent) const override;
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
	QHash<int, QByteArray> roleNames() const override;

	/// call this to explicitly mark the component list as loaded - this is used
	/// to build a new component list from scratch.
	void buildingFromScratch();

	/// install more jar mods
	void installJarMods(QStringList selectedFiles);

	/// install a jar/zip as a replacement for the main jar
	void installCustomJar(QString selectedFile);

	enum MoveDirection { MoveUp, MoveDown };
	/// move component file # up or down the list
	void move(const int index, const MoveDirection direction);

	/// remove component file # - including files/records
	bool remove(const int index);

	/// remove component file by id - including files/records
	bool remove(const QString id);

	bool customize(int index);

	bool revertToBase(int index);

	/// reload the list, reload all components, resolve dependencies
	void reload(Net::Mode netmode);

	// reload all components, resolve dependencies
	void resolve(Net::Mode netmode);

	/// get current running task...
	Task::Ptr getCurrentTask();

	std::shared_ptr<LaunchProfile> getProfile() const;

	// NOTE: used ONLY by MinecraftInstance to provide legacy version mappings
	// from instance config
	void setOldConfigVersion(const QString& uid, const QString& version);

	Q_INVOKABLE QString getComponentVersion(const QString& uid) const;

	bool setComponentVersion(const QString& uid, const QString& version,
							 bool important = false);

	/// QML entry point for VersionPage's "change version"/"install loader"
	/// actions: setComponentVersion() (uid == "net.minecraft" is always
	/// `important`, matching VersionPage::on_actionChange_version_triggered())
	/// followed by resolve(Net::Mode::Online), the same two steps the
	/// widget dialogs perform on accept. Returns false, and sets
	/// lastError(), when @p uid or @p version is empty; resolve() itself
	/// reports its own failure asynchronously through task()/lastError().
	Q_INVOKABLE bool changeComponentVersion(const QString& uid,
											const QString& version);

	/// QML entry point for a component's on/off switch, and for
	/// LoaderInstaller's conflict handling (turning an existing, clashing
	/// loader off before installing a new one) - Component::setEnabled()
	/// itself is not QML-reachable (Component is not exposed to QML on
	/// its own). Returns false if @p uid names no component or the
	/// component refuses (canBeDisabled() is false).
	Q_INVOKABLE bool setComponentEnabled(const QString& uid, bool enabled);

	/// QML entry point for VersionPage's remove/move/customize/revert
	/// toolbar actions, by row rather than by the model index QML would
	/// otherwise have to build. Each mirrors the matching VersionPage
	/// `on_action..._triggered()` handler, including the
	/// invalidateLaunchProfile()/scheduleSave() those already do
	/// internally; unlike the widget page, reloadPackProfile() is not
	/// re-run afterwards - remove()/move()/customize()/revertToBase()
	/// already update this model's rows in place. Each returns false
	/// (and sets lastError()) when @p row is out of range or the
	/// component refuses the operation (not removable/moveable/etc).
	Q_INVOKABLE bool removeComponent(int row);
	Q_INVOKABLE bool moveComponentUp(int row);
	Q_INVOKABLE bool moveComponentDown(int row);
	Q_INVOKABLE bool customizeComponent(int row);
	Q_INVOKABLE bool revertComponent(int row);

	/// Re-reads mmc-pack.json/patches from disk and resolves the result,
	/// the same as VersionPage's "Reload" action - see reload() above,
	/// which this simply calls with Net::Mode::Online and reports
	/// failure from through lastError().
	Q_INVOKABLE bool reloadProfile();

	/// Defined in the .cpp, where TaskWatcher (forward-declared above) is
	/// a complete type.
	QObject* task() const;
	bool busy() const;
	QString lastError() const
	{
		return m_lastError;
	}

	bool installEmpty(const QString& uid, const QString& name);

	QString patchFilePathForUid(const QString& uid) const;

	/* The mod loaders this instance actually has, named the way the mod
	 * platforms name them ("forge", "fabric", ...), in the order the
	 * components appear in the instance.
	 *
	 * Only enabled components count. A component that has been switched
	 * off contributes nothing to the launch, so counting it as an
	 * installed loader would have the content browser search for mods
	 * the instance cannot run. Components with no platform name - which
	 * today means LiteLoader - are left out; see ModLoaderInfo. */
	QStringList getModLoaders();

	/* The single loader to search content with, or empty if there is
	 * none. With conflicts resolved at install time there is at most one
	 * anyway; taking the first in instance order rather than in some
	 * hardcoded preference means the answer matches what the user sees
	 * in the version list should an older instance still carry two. */
	QString primaryModLoader();

	/* Whether anything the content browser can search with is installed.
	 * Not the same question as "is this instance modded": a
	 * LiteLoader-only instance is modded but answers false here, because
	 * there is no loader facet to search either platform with. */
	bool hasModLoader();

	/// if there is a save scheduled, do it now.
	void saveNow();

  signals:
	void minecraftChanged();
	void taskChanged();
	void lastErrorChanged();

  public:
	/// get the profile component by id
	Component* getComponent(const QString& id);

	/// get the profile component by index
	Component* getComponent(int index);

	/// Add the component to the internal list of patches
	// todo(merged): is this the best approach
	void appendComponent(ComponentPtr component);

  private:
	void scheduleSave();
	bool saveIsScheduled() const;

	/// apply the component patches. Catches all the errors and returns
	/// true/false for success/failure
	void invalidateLaunchProfile();

	/// insert component so that its index is ideally the specified one (returns
	/// real index)
	void insertComponent(size_t index, ComponentPtr component);

	QString componentsFilePath() const;
	QString patchesPattern() const;

  private slots:
	void save_internal();
	void updateSucceeded();
	void updateFailed(const QString& error);
	void componentDataChanged();
	void disableInteraction(bool disable);

  private:
	bool load();
	bool installJarMods_internal(QStringList filepaths);
	bool installCustomJar_internal(QString filepath);
	bool removeComponent_internal(ComponentPtr patch);

	bool migratePreComponentConfig();

	/// Sets m_lastError and emits lastErrorChanged() - the one place every
	/// QML-facing wrapper above reports a failure through, so lastError()
	/// is never left holding a stale reason from an unrelated call.
	void setLastError(const QString& error);

  private: /* data */
	std::unique_ptr<PackProfileData> d;

	/// See the `task` Q_PROPERTY above. Parented to `this`; not deleted
	/// on replacement (ContentBrowser::install()'s watchers are kept the
	/// same way) - only PackProfile's own destruction cleans it up.
	TaskWatcher* m_taskWatcher = nullptr;
	QString m_lastError;
};
