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

/// One shortcut that went to the trash along with its instance, and
/// where it went, so that both halves can be put back.
struct TrashedShortcut {
	ShortcutData shortcut;
	QString trashPath;
};

/// Everything needed to undo one trashInstance() call.
struct TrashedInstance {
	InstanceId id;
	/// Display name as it was, for the offer to restore it. The instance's
	/// own config file is in the trash, so this cannot be looked up later.
	QString name;
	/// The folder it came from, and where it goes back to.
	QString path;
	QString trashPath;
	/// The group it was in, which is not recorded inside the folder.
	GroupId group;
	QList<TrashedShortcut> shortcuts;
};

class InstanceList : public QAbstractListModel
{
	Q_OBJECT

  public:
	/* @p instDirs are the instance roots, most important first. The first
	 * one is the primary folder: it is where a new instance goes when
	 * nothing asked for somewhere specific, and where the group file
	 * lives. Duplicates and paths that cannot be created are dropped, so
	 * the list this ends up with may be shorter than the one passed in.
	 */
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

	/* GroupRole is pinned to Qt::UserRole on purpose: InstanceView.h (the
	 * still-shipping QtWidgets instance grid) independently declares
	 * InstanceViewRoles::GroupRole = Qt::UserRole, ProgressValueRole =
	 * Qt::UserRole + 1 and ProgressMaximumRole = Qt::UserRole + 2, and
	 * reads those numerically rather than through this enum. Renumbering
	 * GroupRole here would desync the two, and a new role at +1 or +2
	 * would silently collide with the progress roles. QmlRoles below
	 * therefore starts at Qt::UserRole + 10, leaving 257 and 258 reserved
	 * for InstanceView.h until the widget grid is deleted. */
	static_assert(int(GroupRole) == int(Qt::UserRole),
				  "GroupRole is numerically pinned to InstanceViewRoles::GroupRole "
				  "in InstanceView.h; do not renumber");

	/* Roles added for the QML instance list. instanceId, name, iconKey,
	 * instanceRoot and group are named in roleNames() but reuse the
	 * InstanceIDRole/Qt::DisplayRole/Qt::DecorationRole/Qt::ToolTipRole/
	 * GroupRole cases already handled in data() - only the nine below are
	 * genuinely new. InstancePointerRole is deliberately left unnamed: it
	 * is a raw void*, and QML has no way to dereference one; a QML
	 * delegate reaches an instance by instanceId instead. */
	enum QmlRoles {
		IsRunningRole = Qt::UserRole + 10,
		CanLaunchRole,
		LastLaunchRole,
		TotalTimePlayedRole,
		GameVersionRole,
		LoaderRole,
		IconTintRole,
		/* Human-readable status/step text of the instance's active
		 * launch task (e.g. "Downloading assets..."). Empty when no
		 * launch is in progress, and empty again once the game is
		 * running - pair it with LaunchProgressRole to tell those two
		 * apart. */
		LaunchStatusRole,
		/* 0..1 while the active launch task reports determinate
		 * progress; -1 while it is indeterminate; and -1 as well when
		 * nothing is happening at all, in which case LaunchStatusRole
		 * is also empty. */
		LaunchProgressRole
	};
	/*!
	 * \brief Error codes returned by functions in the InstanceList class.
	 * NoError Indicates that no error occurred.
	 * UnknownError indicates that an unspecified error occurred.
	 */
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

	/* An instance that was installed from @p packId on @p provider, if
	 * there is one.
	 *
	 * Asked before installing a modpack, so that installing a pack the
	 * user already has can offer to update that instance instead of
	 * quietly producing a second copy of it.
	 *
	 * Only the first match is returned. Having two instances of the same
	 * pack is a perfectly reasonable thing - one to play, one to
	 * experiment in - and nothing here could pick between them; the
	 * caller is offering the user a choice, not making one. */
	InstancePtr getInstanceByManagedPack(const QString& provider,
										 const QString& packId) const;

	QModelIndex getInstanceIndexById(const QString& id) const;
	QStringList getGroups();
	bool isGroupCollapsed(const QString& groupName);

	GroupId getInstanceGroup(const InstanceId& id) const;
	void setInstanceGroup(const InstanceId& id, const GroupId& name);

	void deleteGroup(const GroupId& name);

	/// Delete an instance and its shortcuts for good.
	void deleteInstance(const InstanceId& id);

	/**
	 * Move an instance and its shortcuts to the platform's trash.
	 *
	 * Returns false if the platform has no usable trash, or if the move
	 * failed; in both cases nothing has been removed and the caller
	 * should offer deleteInstance() instead.
	 */
	bool trashInstance(const InstanceId& id);

	/// Whether there is anything left to undo.
	bool trashedSomething() const;

	/**
	 * Display name of the instance undoTrashInstance() would put back, so
	 * that the offer to restore it can say which one it means. Empty when
	 * there is nothing to restore.
	 *
	 * Read from the record rather than from disk: the instance's own
	 * config file is in the trash by now.
	 */
	QString lastTrashedName() const;

	/**
	 * Put the most recently trashed instance back, shortcuts included.
	 *
	 * Returns true when there was nothing to do. Returns false if a part
	 * could not be restored; the entry is then kept only when the
	 * instance folder itself is still in the trash, so that a second
	 * attempt can retry it.
	 */
	bool undoTrashInstance();

	// Wrap an instance creation task in some more task machinery and make it
	// ready to be used
	Task* wrapInstanceTask(InstanceTask* task);

	/**
	 * Create a new empty staging area for instance creation and @return a
	 * path/key top commit it later. Used by instance manipulation tasks.
	 *
	 * @p targetDir names which configured instance root to stage inside;
	 * empty means the primary one. Staging inside the destination root is
	 * not an implementation detail - committing is a rename, and a rename
	 * only works within one filesystem. Staging somewhere else would turn
	 * every install on a second drive into a full copy.
	 *
	 * Returns an empty string on failure, including when @p targetDir is
	 * not a configured root: an unknown destination is a caller bug or a
	 * folder the user removed from the settings while a task was queued,
	 * and guessing at a substitute would put the instance somewhere
	 * nobody asked for.
	 */
	QString getStagedInstancePath(const QString& targetDir = QString());

	/// The folder new instances go to when nothing asks for another one.
	QString primaryDir() const
	{
		return m_instDirs.isEmpty() ? QString() : m_instDirs.first();
	}

	/// Every configured instance root, primary first.
	QStringList instanceDirs() const
	{
		return m_instDirs;
	}

	/* The "AdditionalInstanceDirs" setting, decoded and encoded.
	 *
	 * It is stored as a JSON array inside a single string rather than as a
	 * QStringList, because INIFile writes every value out through
	 * QVariant::toString() - and that yields an *empty string* for a list
	 * holding anything other than exactly one element. Stored as a list,
	 * the setting would come back empty on the next launch and the user's
	 * extra folders would quietly disappear. Prism can store a list
	 * directly because it keeps settings in QSettings; we cannot.
	 *
	 * JSON rather than a joined string because these are filesystem paths,
	 * and any separator worth picking is a character that some path is
	 * allowed to contain.
	 */
	static QStringList decodeInstanceDirList(const QVariant& value);
	static QVariant encodeInstanceDirList(const QStringList& dirs);

	/**
	 * Commit the staging area given by @keyPath to the provider - used when
	 * creation succeeds. Used by instance manipulation tasks.
	 *
	 * With @p overrideInstanceId empty this creates a new instance: the
	 * staging directory is moved into place under an id derived from
	 * @p instanceName, and the group index gains an entry.
	 *
	 * With @p overrideInstanceId set, the staging directory is instead
	 * merged over that existing instance, which is how a modpack update
	 * lands. The instance keeps its id, its directory and its group -
	 * @p instanceName and @p groupName are then only used for the new
	 * instance's own name - so that everything referring to it by id
	 * (shortcuts, the group index, play-time records) stays valid.
	 *
	 * @p filesToRemove are absolute paths deleted only after an override
	 * has landed successfully - the files a pack update makes obsolete.
	 * Ignored when not overriding.
	 *
	 * @p removalWarnings, when given, collects a ready-to-show message
	 * for every one of those files that could not be deleted. Failing to
	 * delete is not failing to update - the new version is already in
	 * place and consistent - but it does leave a mod behind that the
	 * pack no longer includes, and the user is the only one who can do
	 * anything about that. So it is handed back for the task to report
	 * rather than being buried in the log.
	 */
	bool commitStagedInstance(const QString& keyPath,
							  const QString& instanceName,
							  const QString& groupName,
							  const QString& overrideInstanceId = QString(),
							  const QStringList& filesToRemove = {},
							  QStringList* removalWarnings = nullptr);

	/**
	 * Destroy a previously created staging area given by @keyPath - used when
	 * creation fails. Used by instance manipulation tasks.
	 */
	bool destroyStagingPath(const QString& keyPath);

	int getTotalPlayTime();

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

	/* Which root @p id was discovered in.
	 *
	 * Falls back to the primary folder for an id nothing has discovered
	 * yet - an instance being committed right now is the normal case - so
	 * callers get a usable path rather than an empty string.
	 */
	QString rootDirOf(const InstanceId& id) const;

	/* Which configured root contains @p stagingPath, or empty if none do.
	 *
	 * The staging directory was created inside its destination root on
	 * purpose, so this reads the answer back off the path instead of
	 * trusting a second copy of it to still agree by commit time.
	 */
	QString rootForStaging(const QString& stagingPath) const;

	/* Canonicalise, deduplicate and create @p dirs.
	 *
	 * Resolution only - deliberately kept apart from adopting the result,
	 * because the caller has to be able to compare the new list against
	 * the one in use, and act on the old one, before the switch happens.
	 */
	QStringList resolveInstanceDirs(const QStringList& dirs) const;

	/* Adopt @p resolved as the configured roots, moving the filesystem
	 * watcher onto them. */
	void applyInstanceDirs(const QStringList& resolved);

	/* Wire @p inst's launch-progress reporting for the QML roles:
	 * a LaunchProgressTracker (parented to the instance, so it goes away
	 * with it) watches whichever LaunchTask is current, and dataChanged
	 * is emitted for this instance's row whenever that changes or
	 * isRunning() does. Called once, from add(). */
	void trackLaunchProgress(BaseInstance* inst);
	void emitLaunchProgressChanged(BaseInstance* inst);
	void emitIsRunningChanged(BaseInstance* inst);

  private:
	int m_watchLevel = 0;
	int totalPlayTime = 0;
	bool m_dirty = false;
	QList<InstancePtr> m_instances;
	QSet<QString> m_groupNameCache;

	SettingsObjectPtr m_globalSettings;
	/// Instance roots, canonical and deduplicated, primary first.
	QStringList m_instDirs;
	/* Which root each discovered instance lives in.
	 *
	 * Instance ids are directory names, so the same id can exist in two
	 * roots; discoverInstances() keeps the first it sees and this map is
	 * what every later path calculation goes through, instead of assuming
	 * one folder. Rebuilt from scratch on every discovery pass - a stale
	 * entry would point an instance at the wrong disk.
	 */
	QHash<InstanceId, QString> m_instanceRootDirMap;
	QFileSystemWatcher* m_watcher;
	// FIXME: this is so inefficient that looking at it is almost painful.
	QSet<QString> m_collapsedGroups;
	QMap<InstanceId, GroupId> m_instanceGroupIndex;
	QSet<InstanceId> instanceSet;
	/// Trashed instances, most recent last, for undoTrashInstance().
	QList<TrashedInstance> m_trashHistory;
	bool m_groupsLoaded = false;
	bool m_instancesProbed = false;
	/* One tracker per instance, for LaunchStatusRole/LaunchProgressRole -
	 * see trackLaunchProgress(). The tracker itself is owned by the
	 * instance (QObject parenting); this is only a lookup table for
	 * data(), kept in sync with the instance's destroyed() signal. */
	QHash<BaseInstance*, LaunchProgressTracker*> m_launchTrackers;
};
