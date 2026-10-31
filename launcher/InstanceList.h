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

#include <QObject>
#include <QAbstractListModel>
#include <QSet>
#include <QList>
#include <QHash>
#include <QStringList>

#include "BaseInstance.h"

#include "QObjectPtr.h"

class QFileSystemWatcher;
class InstanceTask;
class LaunchProgressTracker;
using InstanceId = QString;
using GroupId = QString;
using InstanceLocator = std::pair<InstancePtr, int>;

enum class InstCreateError {
	NoCreateError = 0,
	NoSuchVersion,
	UnknownCreateError,
	InstExists,
	CantCreateDir
};

enum class GroupsState { NotLoaded, Steady, Dirty };

/// A shortcut that went to the trash with its instance, and where it went, so
/// both can be put back.
struct TrashedShortcut {
	ShortcutData shortcut;
	QString trashPath;
};

struct TrashedInstance {
	InstanceId id;
	/// Display name as it was: the instance's config file is in the trash, so
	/// it cannot be looked up later.
	QString name;
	QString path;
	QString trashPath;
	GroupId group;
	QList<TrashedShortcut> shortcuts;
};

class InstanceList : public QAbstractListModel
{
	Q_OBJECT

  public:
	/* @p instDirs are the instance roots, most important first. The first is
	 * the primary folder: where a new instance goes when nothing asked for
	 * somewhere specific. Duplicates and paths that cannot be created are
	 * dropped. */
	explicit InstanceList(SettingsObjectPtr settings,
						  const QStringList& instDirs, QObject* parent = 0);
	virtual ~InstanceList();

  public:
	QModelIndex index(int row, int column = 0,
					  const QModelIndex& parent = QModelIndex()) const override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	QHash<int, QByteArray> roleNames() const override;

	bool setData(const QModelIndex& index, const QVariant& value,
				 int role) override;

	enum AdditionalRoles {
		GroupRole = Qt::UserRole,
		InstancePointerRole = 0x34B1CB48, ///< Return pointer to real instance
		InstanceIDRole = 0x34B1CB49		  ///< Return id if the instance
	};

	/* GroupRole is pinned to Qt::UserRole: InstanceView.h (the widget instance
	 * grid) declares its own GroupRole = Qt::UserRole, ProgressValueRole = +1
	 * and ProgressMaximumRole = +2 and reads them numerically. QmlRoles
	 * therefore starts at Qt::UserRole + 10. */
	static_assert(int(GroupRole) == int(Qt::UserRole),
				  "GroupRole is numerically pinned to InstanceViewRoles::GroupRole "
				  "in InstanceView.h; do not renumber");

	/* Roles for the QML instance list. instanceId, name, iconKey, instanceRoot
	 * and group are named in roleNames() but reuse existing data() cases; only
	 * the ten below are new. InstancePointerRole is left unnamed: it is a raw
	 * void* QML cannot dereference, so a delegate reaches an instance by
	 * instanceId. */
	enum QmlRoles {
		IsRunningRole = Qt::UserRole + 10,
		CanLaunchRole,
		LastLaunchRole,
		TotalTimePlayedRole,
		GameVersionRole,
		LoaderRole,
		IconTintRole,
		/* Status/step text of the instance's active launch task (e.g.
		 * "Downloading assets..."). Empty when no launch is in progress and
		 * once the game is running; pair with LaunchProgressRole to tell those
		 * apart. */
		LaunchStatusRole,
		/* 0..1 while the launch task reports determinate progress; -1 while
		 * indeterminate, and also when nothing is happening (LaunchStatusRole
		 * is then empty too). */
		LaunchProgressRole,
		/* file:// URL of the newest image in the instance's screenshots
		 * folder, or empty when there is none or it is not looked up yet. The
		 * scan runs on a worker (scheduleCoverImageScan()), so data() returns
		 * the cached value or empty at once; the entry is dropped when the
		 * instance stops running. */
		CoverImageRole,
		/* Whether the instance's last launch crashed
		 * (BaseInstance::hasCrashed()). setCrashed() already emits
		 * propertiesChanged(), which add() turns into a row-wide
		 * dataChanged(), so no separate notification is needed. */
		HasCrashedRole,
		/* Cover picture of an instance installed from a platform (when there
		 * is no screenshot): "image://instanceicon/<key>" if its icon is an
		 * icon file, else the logo's upstream URL (PackIconUrl), else empty. */
		PackArtRole
	};
	enum InstListError { NoError = 0, UnknownError };

	InstancePtr at(int i) const
	{
		return m_instances.at(i);
	}

	int count() const
	{
		return m_instances.count();
	}

	InstListError loadList();
	void saveNow();

	InstancePtr getInstanceById(QString id) const;

	/* An instance installed from @p packId on @p provider, if there is one;
	 * asked before installing a modpack so a pack the user already has can
	 * offer to update it instead of making a second copy. Only the first match
	 * is returned: two instances of one pack is reasonable (play, experiment)
	 * and nothing here could pick. */
	InstancePtr getInstanceByManagedPack(const QString& provider,
										 const QString& packId) const;

	QModelIndex getInstanceIndexById(const QString& id) const;
	QStringList getGroups();
	bool isGroupCollapsed(const QString& groupName);

	GroupId getInstanceGroup(const InstanceId& id) const;
	void setInstanceGroup(const InstanceId& id, const GroupId& name);

	void deleteGroup(const GroupId& name);

	/** Moves every instance in group @p from into group @p to, keeping whether
	 * the group was collapsed; renaming onto an existing group merges them.
	 * Does nothing for an empty name on either side ("no group" has no name,
	 * and renaming a group to nothing is deleteGroup()). */
	void renameGroup(const GroupId& from, const GroupId& to);

	/// Delete an instance and its shortcuts for good. False when the
	/// instance folder could not be removed (or the id is unknown).
	bool deleteInstance(const InstanceId& id);

	/** Moves an instance and its shortcuts to the platform's trash. False if
	 * there is no usable trash or the move failed; nothing has been removed
	 * then and the caller should offer deleteInstance(). */
	bool trashInstance(const InstanceId& id);

	bool trashedSomething() const;

	/** Display name of the instance undoTrashInstance() would put back, so the
	 * restore offer can name it; empty when there is nothing to restore. Read
	 * from the record: the instance's config file is in the trash. */
	QString lastTrashedName() const;

	/** Puts the most recently trashed instance back, shortcuts included. True
	 * when there was nothing to do. False if a part could not be restored; the
	 * entry is kept only if the instance folder is still in the trash, so a
	 * retry is possible. */
	bool undoTrashInstance();

	Task* wrapInstanceTask(InstanceTask* task);

	/** Creates an empty staging area for instance creation; returns a path/key
	 * to commit it later. @p targetDir names the configured root to stage
	 * inside (empty: the primary one); a commit is a rename, which works only
	 * within one filesystem. Empty string on failure, including an unknown @p
	 * targetDir, which is never guessed. */
	QString getStagedInstancePath(const QString& targetDir = QString());

	QString primaryDir() const
	{
		return m_instDirs.isEmpty() ? QString() : m_instDirs.first();
	}

	QStringList instanceDirs() const
	{
		return m_instDirs;
	}

	/* The "AdditionalInstanceDirs" setting, decoded and encoded. Stored as a
	 * JSON array in a single string, not a QStringList: INIFile writes values
	 * through QVariant::toString(), which is empty for a list of anything but
	 * one element. JSON rather than a joined string because any separator is a
	 * character some path may contain. */
	static QStringList decodeInstanceDirList(const QVariant& value);
	static QVariant encodeInstanceDirList(const QStringList& dirs);

	/** Commits the staging area @keyPath when creation succeeds. With @p
	 * overrideInstanceId empty it creates a new instance under an id derived
	 * from @p instanceName. With it set, the staging directory is merged over
	 * that instance (how a modpack update lands), keeping its id, directory
	 * and group; @p instanceName/@p groupName then only name the new instance.
	 * @p filesToRemove (absolute paths) are deleted only after an override
	 * landed; @p removalWarnings collects a message for each that could not be
	 * deleted. */
	bool commitStagedInstance(const QString& keyPath,
							  const QString& instanceName,
							  const QString& groupName,
							  const QString& overrideInstanceId = QString(),
							  const QStringList& filesToRemove = {},
							  QStringList* removalWarnings = nullptr);

	bool destroyStagingPath(const QString& keyPath);

	int getTotalPlayTime();

	/* Newest-modified png/jpg/jpeg directly inside @p screenshotsDir as a
	 * file:// URL, or empty. Pure (no InstanceList or QObject access), so it
	 * can run on a worker thread and be tested directly. Same order as
	 * ScreenshotListModel::listEntries() (mtime descending, file name
	 * tiebreak). */
	static QString newestScreenshotUrl(const QString& screenshotsDir);

	Qt::DropActions supportedDragActions() const override;

	Qt::DropActions supportedDropActions() const override;

	bool canDropMimeData(const QMimeData* data, Qt::DropAction action, int row,
						 int column, const QModelIndex& parent) const override;

	bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
					  int column, const QModelIndex& parent) override;

	QStringList mimeTypes() const override;
	QMimeData* mimeData(const QModelIndexList& indexes) const override;

  signals:
	void dataIsInvalid();
	void instancesChanged();
	void instanceSelectRequest(QString instanceId);
	void groupsChanged(QSet<QString> groups);

  public slots:
	void on_InstFolderChanged(const Setting& setting, QVariant value);
	void on_GroupStateChanged(const QString& group, bool collapsed);

  private slots:
	void propertiesChanged(BaseInstance* inst);
	void providerUpdated();
	void instanceDirContentsChanged(const QString& path);

  private:
	int getInstIndex(BaseInstance* inst) const;
	void updateTotalPlayTime();
	void suspendWatch();
	void resumeWatch();
	void add(const QList<InstancePtr>& list);
	void loadGroupList();
	void saveGroupList();
	QList<InstanceId> discoverInstances();
	InstancePtr loadInstance(const InstanceId& id);

	/* Which root @p id was discovered in. Falls back to the primary folder for
	 * an id nothing has discovered yet (an instance being committed right
	 * now), so callers get a usable path. */
	QString rootDirOf(const InstanceId& id) const;

	/* Which configured root contains @p stagingPath, or empty. The staging
	 * directory was created inside its destination root, so this reads the
	 * answer off the path instead of trusting a second copy of it. */
	QString rootForStaging(const QString& stagingPath) const;

	/* Canonicalises, deduplicates and creates @p dirs. Resolution only, kept
	 * apart from adopting the result so the caller can compare the new list
	 * against the one in use, and act on the old one, before the switch. */
	QStringList resolveInstanceDirs(const QStringList& dirs) const;

	void applyInstanceDirs(const QStringList& resolved);

	/* Wires @p inst's launch-progress reporting for the QML roles: a
	 * LaunchProgressTracker (parented to the instance) watches the current
	 * LaunchTask, and dataChanged is emitted for the row whenever that changes
	 * or isRunning() does. Called once, from add(). */
	void trackLaunchProgress(BaseInstance* inst);
	void emitLaunchProgressChanged(BaseInstance* inst);
	/* Emits IsRunningRole's dataChanged and, when @p inst just stopped, drops
	 * its m_coverImageCache entry and emits CoverImageRole's, since a new
	 * screenshot is likely. */
	void emitIsRunningChanged(BaseInstance* inst);
	/* Starts a background scan of @p screenshotsDir for CoverImageRole unless
	 * one for @p id is running. The result goes to m_coverImageCache and
	 * dataChanged is emitted, unless m_coverImageGeneration moved on meanwhile
	 * (the instance stopped running): then it is discarded and a fresh scan
	 * started. */
	void scheduleCoverImageScan(const InstanceId& id,
								const QString& screenshotsDir);

  private:
	int m_watchLevel = 0;
	int totalPlayTime = 0;
	bool m_dirty = false;
	QList<InstancePtr> m_instances;
	QSet<QString> m_groupNameCache;

	SettingsObjectPtr m_globalSettings;
	QStringList m_instDirs;
	/* Which root each discovered instance lives in. Ids are directory names,
	 * so the same id can exist in two roots; discoverInstances() keeps the
	 * first it sees. Rebuilt on every discovery pass, since a stale entry
	 * would point an instance at the wrong disk. */
	QHash<InstanceId, QString> m_instanceRootDirMap;
	QFileSystemWatcher* m_watcher;
	// FIXME: this is so inefficient that looking at it is almost painful.
	QSet<QString> m_collapsedGroups;
	QMap<InstanceId, GroupId> m_instanceGroupIndex;
	QSet<InstanceId> instanceSet;
	QList<TrashedInstance> m_trashHistory;
	bool m_groupsLoaded = false;
	bool m_instancesProbed = false;
	/* One tracker per instance, for LaunchStatusRole/LaunchProgressRole. The
	 * instance owns the tracker (QObject parenting); this is a lookup table
	 * for data(), kept in sync with the instance's destroyed() signal. */
	QHash<BaseInstance*, LaunchProgressTracker*> m_launchTrackers;
	/* Newest-screenshot URL per instance id for CoverImageRole, filled lazily
	 * by data() and dropped when the instance stops running. Mutable because
	 * data() is const; an absent id has not been looked up, a present empty
	 * string means "looked up, none found". */
	mutable QHash<InstanceId, QString> m_coverImageCache;
	/* Ids with a cover scan running on a worker thread; stops data() queuing a
	 * second QtConcurrent::run() for the same id. Mutable like
	 * m_coverImageCache. */
	mutable QSet<InstanceId> m_coverImageScansPending;
	QHash<InstanceId, int> m_coverImageGeneration;
};
