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
#include <cassert>

#include <QObject>
#include "QObjectPtr.h"
#include <QDateTime>
#include <QSet>
#include <QProcess>

#include "settings/SettingsObject.h"

#include "settings/INIFile.h"
#include "BaseVersionList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "MessageLevel.h"
#include "pathmatcher/IPathMatcher.h"

#include "net/Mode.h"

#include "minecraft/launch/MinecraftServerTarget.h"

class QDir;
class Task;
class LaunchTask;
class BaseInstance;

// pointer for lazy people
typedef std::shared_ptr<BaseInstance> InstancePtr;

/// Where a shortcut to an instance was asked to be written.
enum class ShortcutTarget : quint8 { Desktop, Applications, Other };

/// One shortcut an instance knows it is responsible for, so that
/// deleting the instance can take its shortcuts with it.
struct ShortcutData {
	/// What the shortcut is called, for log messages.
	QString name;
	/// The file (or, on macOS, the .app bundle) that was written.
	QString filePath;
	/// Which folder it was written into.
	ShortcutTarget target = ShortcutTarget::Other;
};

/*!
 * \brief Base class for instances.
 * This class implements many functions that are common between instances and
 * provides a standard interface for all instances.
 *
 * To create a new instance type, create a new class inheriting from this class
 * and implement the pure virtual functions.
 */
class BaseInstance : public QObject,
					 public std::enable_shared_from_this<BaseInstance>
{
	Q_OBJECT
  protected:
	/// no-touchy!
	BaseInstance(SettingsObjectPtr globalSettings, SettingsObjectPtr settings,
				 const QString& rootDir);

  public: /* types */
	enum class Status {
		Present,
		Gone // either nuked or invalidated
	};

  public:
	/// virtual destructor to make sure the destruction is COMPLETE
	virtual ~BaseInstance() {};

	virtual void saveNow() = 0;

	/***
	 * the instance has been invalidated - it is no longer tracked by MeshMC for
	 * some reason, but it has not necessarily been deleted.
	 *
	 * Happens when the instance folder changes to some other location, or the
	 * instance is removed by external means.
	 */
	void invalidate();

	/// The instance's ID. The ID SHALL be determined by MESHMC internally. The
	/// ID IS guaranteed to be unique.
	virtual QString id() const;

	void setRunning(bool running);
	bool isRunning() const;
	int64_t totalTimePlayed() const;
	int64_t lastTimePlayed() const;
	void resetTimePlayed();

	/// get the type of this instance
	QString instanceType() const;

	/// Path to the instance's root directory.
	QString instanceRoot() const;

	/// Path to the instance's game root directory.
	virtual QString gameRoot() const
	{
		return instanceRoot();
	}

	/// Path to the instance's mods directory.
	virtual QString modsRoot() const = 0;

	QString name() const;
	void setName(QString val);

	/// Value used for instance window titles
	QString windowTitle() const;

	QString iconKey() const;
	void setIconKey(QString val);

	QString notes() const;
	void setNotes(QString val);

	/* ---- Managed pack provenance ------------------------------------
	 *
	 * An instance is "managed" when it came from a Modrinth or
	 * CurseForge modpack and we still know *which* pack, and which
	 * version of it, is currently on disk. That is exactly enough to
	 * ask the catalogue what other versions exist and to replace the
	 * instance with one of them in place - which is what
	 * ManagedPackPage does.
	 *
	 * These read the pack-source keys registered in the constructor.
	 * Deliberately thin wrappers rather than a cached struct:
	 * isManagedPack() is asked on every repaint of a page that shows
	 * an instance, and a settings lookup is already a hash probe.
	 *
	 * Every field is allowed to be empty. An older MeshMC recorded
	 * fewer of them, and a pack imported by drag-and-drop never had a
	 * catalogue entry to record in the first place, so the page has to
	 * cope with partial records rather than assume all-or-nothing.
	 */

	/* "modrinth" or "curseforge", matching ContentApi::id(). Empty
	 * means this instance is not tied to a catalogue. */
	QString managedPackProvider() const;
	/* Modrinth project id / CurseForge numeric project id as a
	 * string. */
	QString managedPackId() const;
	/* Modrinth slug; empty on CurseForge, which has no equivalent. */
	QString managedPackSlug() const;
	/* Pack title as the catalogue spells it, which is not necessarily
	 * the instance name - the user is free to rename the instance.
	 * Falls back to the slug, then to the instance name, so that
	 * instances imported before this field was recorded still show
	 * something meaningful. */
	QString managedPackName() const;
	/* Modrinth version id / CurseForge file id, as a string. */
	QString managedPackVersionId() const;
	/* Human-readable version, e.g. "1.2.3". */
	QString managedPackVersionName() const;
	/* Canonical pack page recorded at import time, if any. */
	QString managedPackSourceUrl() const;

	/* True when this instance came from a catalogue at all, i.e. a
	 * provider is recorded.
	 *
	 * Deliberately does *not* require a pack id. An instance can have a
	 * provider and no id - that is what an import from a local .mrpack
	 * or a pack imported by an older MeshMC looks like - and such an
	 * instance is still worth showing the pack page for, because it can
	 * be updated from a file or from a URL the user supplies. Requiring
	 * the id here would hide the page in exactly the case where it is
	 * the only way to update. */
	bool isManagedPack() const;

	/* True when there is enough recorded to *ask the catalogue*: a
	 * provider and a pack id. Everything that issues an API request
	 * has to check this, because every endpoint is keyed by the id. */
	bool hasManagedPackId() const;

	/* URL the user typed for a pack we have no catalogue entry for.
	 * Lets a hand-made or drag-dropped instance still be updated from
	 * a file or a link the user vouches for. */
	QString managedPackUpdateUrl() const;
	void setManagedPackUpdateUrl(const QString& url);

	/* Move the recorded version forward. Called after an update has
	 * actually landed on disk, so that the page and the catalogue
	 * agree about what is installed. */
	void setManagedPackVersion(const QString& versionId,
							   const QString& versionName);

	/**
	 * Shortcuts written for this instance that are still where they were
	 * written.
	 *
	 * An entry whose file has since been moved or deleted is dropped on
	 * the way out, which is what keeps the launcher from deleting
	 * something at a path it no longer owns.
	 */
	QList<ShortcutData> shortcuts() const;

	/// Take responsibility for one more shortcut.
	void registerShortcut(const ShortcutData& shortcut);

	/// Replace the whole list.
	void setShortcuts(const QList<ShortcutData>& shortcuts);

	/**
	 * Key of the profiler this instance launches under, empty for none.
	 *
	 * The key is whatever Application::profilers() is indexed by. It is
	 * kept verbatim even when no such profiler is registered in this
	 * build, so that an instance shared between machines does not
	 * quietly lose the setting.
	 */
	QString profilerKey() const;

	/// Set (or, with an empty key, clear) the profiler. Emits
	/// profilerChanged() only when the value actually moves.
	void setProfilerKey(const QString& key);

	QString getPreLaunchCommand();
	QString getPostExitCommand();
	QString getWrapperCommand();

	/// guess log level from a line of game log
	virtual MessageLevel::Enum guessLevel(const QString&,
										  MessageLevel::Enum level)
	{
		return level;
	};

	virtual QStringList extraArguments() const;

	/// Traits. Normally inside the version, depends on instance implementation.
	virtual QSet<QString> traits() const = 0;

	/**
	 * Gets the time that the instance was last launched.
	 * Stored in milliseconds since epoch.
	 */
	qint64 lastLaunch() const;
	/// Sets the last launched time to 'val' milliseconds since epoch
	void setLastLaunch(qint64 val = QDateTime::currentMSecsSinceEpoch());

	/*!
	 * \brief Gets this instance's settings object.
	 * This settings object stores instance-specific settings.
	 * \return A pointer to this instance's settings object.
	 */
	virtual SettingsObjectPtr settings() const;

	/// returns a valid update task
	virtual Task::Ptr createUpdateTask(Net::Mode mode) = 0;

	/// returns a valid launcher (task container)
	virtual shared_qobject_ptr<LaunchTask>
	createLaunchTask(AuthSessionPtr account,
					 MinecraftServerTargetPtr serverToJoin) = 0;

	/// returns the current launch task (if any)
	shared_qobject_ptr<LaunchTask> getLaunchTask();

	/*!
	 * Create envrironment variables for running the instance
	 */
	virtual QProcessEnvironment createEnvironment() = 0;

	/*!
	 * Returns a matcher that can maps relative paths within the instance to
	 * whether they are 'log files'
	 */
	virtual IPathMatcher::Ptr getLogFileMatcher() = 0;

	/*!
	 * Returns the root folder to use for looking up log files
	 */
	virtual QString getLogFileRoot() = 0;

	virtual QString getStatusbarDescription() = 0;

	/// FIXME: this really should be elsewhere...
	virtual QString instanceConfigFolder() const = 0;

	/// get variables this instance exports
	virtual QMap<QString, QString> getVariables() const = 0;

	virtual QString typeName() const = 0;

	/**
	 * The Minecraft version this instance runs, e.g. "1.21.4".
	 *
	 * Empty for an instance type that has no such concept, which is the
	 * default here. MinecraftInstance overrides this; kept on the base
	 * class so generic code (the QML instance list) can ask any instance
	 * without a dynamic_cast.
	 */
	virtual QString gameVersion() const
	{
		return {};
	}

	/**
	 * Human name of the mod loader this instance uses ("Fabric", "Forge",
	 * ...), or empty when none is installed or this instance type has no
	 * such concept.
	 *
	 * Overridden by MinecraftInstance, for the same reason as
	 * gameVersion().
	 */
	virtual QString modLoaderName() const
	{
		return {};
	}

	bool hasVersionBroken() const
	{
		return m_hasBrokenVersion;
	}
	void setVersionBroken(bool value)
	{
		if (m_hasBrokenVersion != value) {
			m_hasBrokenVersion = value;
			emit propertiesChanged(this);
		}
	}

	bool hasUpdateAvailable() const
	{
		return m_hasUpdate;
	}
	void setUpdateAvailable(bool value)
	{
		if (m_hasUpdate != value) {
			m_hasUpdate = value;
			emit propertiesChanged(this);
		}
	}

	bool hasCrashed() const
	{
		return m_crashed;
	}
	void setCrashed(bool value)
	{
		if (m_crashed != value) {
			m_crashed = value;
			emit propertiesChanged(this);
		}
	}

	virtual bool canLaunch() const;
	virtual bool canEdit() const = 0;
	virtual bool canExport() const = 0;

	bool reloadSettings();

	/**
	 * 'print' a verbose description of the instance into a QStringList
	 */
	virtual QStringList
	verboseDescription(AuthSessionPtr session,
					   MinecraftServerTargetPtr serverToJoin) = 0;

	Status currentStatus() const;

	int getConsoleMaxLines() const;
	bool shouldStopOnConsoleOverflow() const;

  protected:
	void changeStatus(Status newStatus);

  signals:
	/*!
	 * \brief Signal emitted when properties relevant to the instance view
	 * change
	 */
	void propertiesChanged(BaseInstance* inst);

	void launchTaskChanged(shared_qobject_ptr<LaunchTask>);

	void runningStatusChanged(bool running);

	/// The profiler this instance launches under was changed.
	void profilerChanged();

	void statusChanged(Status from, Status to);

  protected slots:
	void iconUpdated(QString key);

  protected: /* data */
	QString m_rootDir;
	SettingsObjectPtr m_settings;
	// InstanceFlags m_flags;
	bool m_isRunning = false;
	shared_qobject_ptr<LaunchTask> m_launchProcess;
	QDateTime m_timeStarted;

  private: /* data */
	Status m_status = Status::Present;
	bool m_crashed = false;
	bool m_hasUpdate = false;
	bool m_hasBrokenVersion = false;
};

Q_DECLARE_METATYPE(shared_qobject_ptr<BaseInstance>)
// Q_DECLARE_METATYPE(BaseInstance::InstanceFlag)
// Q_DECLARE_OPERATORS_FOR_FLAGS(BaseInstance::InstanceFlags)
