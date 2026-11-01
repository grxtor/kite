/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

	/// The ComponentUpdateTask behind the last reload()/resolve(), wrapped for
	/// QML. Null until one runs; parented to this.
	Q_PROPERTY(QObject* task READ task NOTIFY taskChanged)
	/// Whether a ComponentUpdateTask is running.
	Q_PROPERTY(bool busy READ busy NOTIFY taskChanged)
	/// The reason the last mutating call failed; cleared on the next attempt.
	Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

  public:
	enum Columns { NameColumn = 0, VersionColumn, NUM_COLUMNS };

	// Roles for QML consumers. New roles are appended so existing values never
	// change.
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
		/// Whether the metadata index has a version list for this uid. Cheap on
		/// purpose: data() runs per visible row and loading the list starts a
		/// download.
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
	/// A launch profile for @p context, made anew and not kept: what another
	/// Java would get. Null if the components cannot be applied.
	std::shared_ptr<LaunchProfile>
	makeProfile(const RuntimeContext& context) const;

	// NOTE: used ONLY by MinecraftInstance to provide legacy version mappings
	// from instance config
	void setOldConfigVersion(const QString& uid, const QString& version);

	Q_INVOKABLE QString getComponentVersion(const QString& uid) const;

	bool setComponentVersion(const QString& uid, const QString& version,
							 bool important = false);

	/// setComponentVersion() (net.minecraft is always important), then
	/// resolve(Online). Returns false and sets lastError() if uid or version is
	/// empty; resolve() reports failure later via task()/lastError().
	Q_INVOKABLE bool changeComponentVersion(const QString& uid,
											const QString& version);

	/// Turns a component on or off. Returns false if uid is unknown or
	/// canBeDisabled() is false.
	Q_INVOKABLE bool setComponentEnabled(const QString& uid, bool enabled);

	/// Row-based wrappers over remove/move/customize/revertToBase. Each returns
	/// false and sets lastError() when the row is out of range or the component
	/// refuses.
	Q_INVOKABLE bool removeComponent(int row);
	Q_INVOKABLE bool moveComponentUp(int row);
	Q_INVOKABLE bool moveComponentDown(int row);
	Q_INVOKABLE bool customizeComponent(int row);
	Q_INVOKABLE bool revertComponent(int row);

	/// Re-reads mmc-pack.json and the patches from disk and resolves online;
	/// failures go to lastError().
	Q_INVOKABLE bool reloadProfile();

	/// Defined in the .cpp: TaskWatcher is only forward-declared here.
	QObject* task() const;
	bool busy() const;
	QString lastError() const
	{
		return m_lastError;
	}

	bool installEmpty(const QString& uid, const QString& name);

	QString patchFilePathForUid(const QString& uid) const;

	/// The enabled components' mod loaders, named as the mod platforms name
	/// them, in instance order. Components without a platform name (LiteLoader)
	/// are left out.
	QStringList getModLoaders();

	/// The loader to search content with, or empty: the first in instance
	/// order.
	QString primaryModLoader();

	/// Whether a loader the content browser can search with is installed (false
	/// for a LiteLoader-only instance).
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

	/// Sets m_lastError and emits lastErrorChanged(); the QML-facing wrappers
	/// report failure through it.
	void setLastError(const QString& error);

  private: /* data */
	std::unique_ptr<PackProfileData> d;

	/// Backs the `task` property; parented to this and kept until destruction.
	TaskWatcher* m_taskWatcher = nullptr;
	QString m_lastError;
};
