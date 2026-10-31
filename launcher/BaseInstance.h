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

typedef std::shared_ptr<BaseInstance> InstancePtr;

enum class ShortcutTarget : quint8 { Desktop, Applications, Other };

/// One shortcut an instance is responsible for, so deleting the instance takes
/// it along.
struct ShortcutData {
	QString name;
	QString filePath;
	ShortcutTarget target = ShortcutTarget::Other;
};

/* Base class for instances: common functions and a standard interface. A new
 * instance type inherits it and implements the pure virtual functions. */
class BaseInstance : public QObject,
					 public std::enable_shared_from_this<BaseInstance>
{
	Q_OBJECT
  protected:
	BaseInstance(SettingsObjectPtr globalSettings, SettingsObjectPtr settings,
				 const QString& rootDir);

  public:
	enum class Status {
		Present,
		Gone // either nuked or invalidated
	};

  public:
	virtual ~BaseInstance() {};

	virtual void saveNow() = 0;

	/** The instance is no longer tracked by Kite (folder moved, or removed by
	 * external means) but has not necessarily been deleted. */
	void invalidate();

	virtual QString id() const;

	void setRunning(bool running);
	bool isRunning() const;
	int64_t totalTimePlayed() const;
	int64_t lastTimePlayed() const;
	void resetTimePlayed();

	QString instanceType() const;

	QString instanceRoot() const;

	virtual QString gameRoot() const
	{
		return instanceRoot();
	}

	virtual QString modsRoot() const = 0;

	QString name() const;
	void setName(QString val);

	QString windowTitle() const;

	QString iconKey() const;
	void setIconKey(QString val);

	QString notes() const;
	void setNotes(QString val);

	/* Managed pack provenance: an instance is "managed" when it came from a
	 * Modrinth or CurseForge modpack and the pack and version on disk are
	 * known (ManagedPackPage uses this to offer other versions). Thin wrappers
	 * over the pack-source keys registered in the constructor; every field may
	 * be empty. */

	/* "modrinth" or "curseforge", matching ContentApi::id(). Empty means the
	 * instance is not tied to a catalogue. */
	QString managedPackProvider() const;
	QString managedPackId() const;
	QString managedPackSlug() const;
	/* Pack title as the catalogue spells it, not necessarily the instance
	 * name. Falls back to the slug, then the instance name, for instances
	 * imported before this was recorded. */
	QString managedPackName() const;
	QString managedPackVersionId() const;
	QString managedPackVersionName() const;
	QString managedPackSourceUrl() const;

	/* True when this instance came from a catalogue at all, i.e. a provider is
	 * recorded. Deliberately does not require a pack id: an import from a
	 * local .mrpack or by an older Kite has a provider and no id, and such an
	 * instance can still be updated from a file or URL, so hiding its page
	 * would hide the only way to update. */
	bool isManagedPack() const;

	/* True when enough is recorded to ask the catalogue: a provider and a pack
	 * id. Everything that issues an API request must check this. */
	bool hasManagedPackId() const;

	/* URL the user typed for a pack with no catalogue entry, so a hand-made or
	 * drag-dropped instance can still be updated from a link the user vouches
	 * for. */
	QString managedPackUpdateUrl() const;
	void setManagedPackUpdateUrl(const QString& url);

	/* Moves the recorded version forward after an update has landed on disk,
	 * so the page and the catalogue agree. */
	void setManagedPackVersion(const QString& versionId,
							   const QString& versionName);

	/** Shortcuts written for this instance that are still where they were
	 * written. An entry whose file was moved or deleted is dropped, so the
	 * launcher never deletes something at a path it no longer owns. */
	QList<ShortcutData> shortcuts() const;

	void registerShortcut(const ShortcutData& shortcut);

	void setShortcuts(const QList<ShortcutData>& shortcuts);

	/** Key of the profiler this instance launches under, empty for none:
	 * whatever Application::profilers() is indexed by. Kept verbatim even if
	 * this build has no such profiler, so an instance shared between machines
	 * does not lose the setting. */
	QString profilerKey() const;

	void setProfilerKey(const QString& key);

	QString getPreLaunchCommand();
	QString getPostExitCommand();
	QString getWrapperCommand();

	virtual MessageLevel::Enum guessLevel(const QString&,
										  MessageLevel::Enum level)
	{
		return level;
	};

	virtual QStringList extraArguments() const;

	virtual QSet<QString> traits() const = 0;

	qint64 lastLaunch() const;
	void setLastLaunch(qint64 val = QDateTime::currentMSecsSinceEpoch());

	virtual SettingsObjectPtr settings() const;

	virtual Task::Ptr createUpdateTask(Net::Mode mode) = 0;

	virtual shared_qobject_ptr<LaunchTask>
	createLaunchTask(AuthSessionPtr account,
					 MinecraftServerTargetPtr serverToJoin) = 0;

	shared_qobject_ptr<LaunchTask> getLaunchTask();

	virtual QProcessEnvironment createEnvironment() = 0;

	/*!
	 * Returns a matcher that can maps relative paths within the instance to
	 * whether they are 'log files'
	 */
	virtual IPathMatcher::Ptr getLogFileMatcher() = 0;

	virtual QString getLogFileRoot() = 0;

	virtual QString getStatusbarDescription() = 0;

	/// FIXME: this really should be elsewhere...
	virtual QString instanceConfigFolder() const = 0;

	virtual QMap<QString, QString> getVariables() const = 0;

	virtual QString typeName() const = 0;

	/** The Minecraft version this instance runs, e.g. "1.21.4"; empty for an
	 * instance type without one. On the base class so generic code (the QML
	 * instance list) can ask any instance without a dynamic_cast. */
	virtual QString gameVersion() const
	{
		return {};
	}

	/** Human name of the mod loader ("Fabric", "Forge", ...), or empty when
	 * none is installed or the type has none. Overridden by MinecraftInstance,
	 * like gameVersion(). */
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

	virtual QStringList
	verboseDescription(AuthSessionPtr session,
					   MinecraftServerTargetPtr serverToJoin) = 0;

	Status currentStatus() const;

	int getConsoleMaxLines() const;
	bool shouldStopOnConsoleOverflow() const;

  protected:
	void changeStatus(Status newStatus);

  signals:
	void propertiesChanged(BaseInstance* inst);

	void launchTaskChanged(shared_qobject_ptr<LaunchTask>);

	void runningStatusChanged(bool running);

	void profilerChanged();

	void statusChanged(Status from, Status to);

  protected slots:
	void iconUpdated(QString key);

  protected:
	QString m_rootDir;
	SettingsObjectPtr m_settings;
	bool m_isRunning = false;
	shared_qobject_ptr<LaunchTask> m_launchProcess;
	QDateTime m_timeStarted;

  private:
	Status m_status = Status::Present;
	bool m_crashed = false;
	bool m_hasUpdate = false;
	bool m_hasBrokenVersion = false;
};

Q_DECLARE_METATYPE(shared_qobject_ptr<BaseInstance>)
