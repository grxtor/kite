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

#include "tasks/Task.h"
#include "settings/SettingsObject.h"

#include <memory>

/* Only held behind a shared_ptr here; the destructor that needs the complete
 * type lives in the .cpp. */
class MinecraftInstance;

class InstanceTask : public Task
{
	Q_OBJECT
  public:
	explicit InstanceTask();
	virtual ~InstanceTask();

	void setParentSettings(SettingsObjectPtr settings)
	{
		m_globalSettings = settings;
	}

	void setStagingPath(const QString& stagingPath)
	{
		m_stagingPath = stagingPath;
	}

	void setName(const QString& name)
	{
		m_instName = name;
	}
	QString name() const
	{
		return m_instName;
	}

	void setIcon(const QString& icon)
	{
		m_instIcon = icon;
	}

	void setGroup(const QString& group)
	{
		m_instGroup = group;
	}
	QString group() const
	{
		return m_instGroup;
	}

	/* Overwriting an existing instance: a task normally stages a new directory
	 * that gets a fresh id; a modpack update stages one that replaces an
	 * existing instance, which keeps its id, group, saves and everything
	 * referring to it by id. Lives on InstanceTask because the staging step
	 * only sees an InstanceTask. */

	/* Replace `instanceId` with whatever this task stages; an empty id turns
	 * overriding off. */
	void setOverrideInstance(const QString& instanceId)
	{
		m_overrideInstanceId = instanceId;
	}

	/* An empty id is the whole "no": a separate flag could only disagree with
	 * it. */
	bool shouldOverride() const
	{
		return !m_overrideInstanceId.isEmpty();
	}

	QString overrideInstanceId() const
	{
		return m_overrideInstanceId;
	}

	/* Files the update makes obsolete: merging the new version over the old
	 * does not remove a dropped mod, so they are named explicitly (absolute
	 * paths) and the staging step deletes them after the merge succeeds;
	 * earlier would leave a failed update missing mods. */
	void scheduleForRemoval(const QString& absolutePath)
	{
		if (!absolutePath.isEmpty() &&
			!m_filesToRemoveAfterCommit.contains(absolutePath)) {
			m_filesToRemoveAfterCommit.append(absolutePath);
		}
	}

	QStringList filesToRemoveAfterCommit() const
	{
		return m_filesToRemoveAfterCommit;
	}

	/* Which instance folder to land in: with several instance roots
	 * configured, the destination is a property of the task; empty means the
	 * primary folder. Must be set before the task is wrapped: staging happens
	 * at wrap time inside the chosen root, so the finished instance is moved
	 * within one filesystem. */
	void setTargetDir(const QString& dir)
	{
		m_targetDir = dir;
	}

	QString targetDir() const
	{
		return m_targetDir;
	}

	/* Whether downloadFiles() may pre-download the game's files at all, on top
	 * of the global setting. On by default. A caller turns it off when the
	 * user will not wait on this task; the files then come with the first
	 * launch. */
	void setDownloadGameFiles(bool download)
	{
		m_downloadGameFiles = download;
	}

	/* Abortable only while the optional game-file download runs; the earlier
	 * steps (unpacking, fetching the pack) cannot be stopped halfway usefully. */
	bool canAbort() const override
	{
		return m_gameFilesTask != nullptr;
	}

  public slots:
	bool abort() override;

  protected:
	/* Fetches the game's own files (version metadata, libraries, assets) for
	 * the instance just built, then finishes: it emits succeeded() itself,
	 * even when it does nothing, so use it instead of emitSucceeded() as the
	 * last act. A failed fetch is only a warning (the next launch fetches the
	 * rest). @p instance must stay alive until the task finishes. */
	void downloadFiles(MinecraftInstance* instance);

	/* For a caller that owns the instance it just built: such an instance is a
	 * local that dies when the configuring function returns, exactly when the
	 * download starts, so this overload holds it alive. Emits succeeded()
	 * itself, like the overload above. */
	void downloadFiles(std::shared_ptr<MinecraftInstance> instance);

  protected:
	SettingsObjectPtr m_globalSettings;
	QString m_instName;
	QString m_instIcon;
	QString m_instGroup;
	QString m_stagingPath;
	QString m_overrideInstanceId;
	QStringList m_filesToRemoveAfterCommit;
	QString m_targetDir;
	bool m_downloadGameFiles = true;
	/* The game-file download while running, null otherwise (which is how
	 * canAbort() knows). */
	Task::Ptr m_gameFilesTask;
	/* The instance that download runs against, when this task was handed
	 * ownership; only set by the shared_ptr overload. */
	std::shared_ptr<MinecraftInstance> m_ownedInstance;
};
