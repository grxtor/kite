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

class PackProfile : public QAbstractListModel
{
	Q_OBJECT
	friend ComponentUpdateTask;

  public:
	enum Columns { NameColumn = 0, VersionColumn, NUM_COLUMNS };

	// Column-independent roles for QML consumers. Kept separate from
	// Columns above, which the QtWidgets tree view still relies on.
	enum ModelRoles { NameRole = Qt::UserRole, VersionRole, ProblemSeverityRole };

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

	QString getComponentVersion(const QString& uid) const;

	bool setComponentVersion(const QString& uid, const QString& version,
							 bool important = false);

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

  private: /* data */
	std::unique_ptr<PackProfileData> d;
};
