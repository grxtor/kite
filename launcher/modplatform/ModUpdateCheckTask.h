/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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
#include <QPointer>
#include <QString>
#include <memory>

#include "modplatform/ContentType.h"
#include "modplatform/ModDownloadTypes.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

class ModMetadataIndex;

/* Walks every index entry with a remote provenance and asks the source
 * platform for the newest compatible version. When it differs from the
 * recorded one, a ready DownloadItem is produced (replaceExisting and
 * replacesFileName set).
 *
 * Read-only on the index: the caller hands the plan to a
 * ContentDownloadTask, which writes the files and updates the sidecars. */
class ModUpdateCheckTask : public Task
{
	Q_OBJECT
  public:
	struct UpdateInfo {
		QString currentFileName;
		QString currentVersionId;
		QString newVersionId;
		QString name;
		QString platform;
		ModPlatform::DownloadItem item;
		/* The versions as a person reads them ("0.6.5"), for a review list; the
		 * ids above are opaque. Either may be empty. */
		QString currentVersionNumber;
		QString newVersionNumber;
	};

	/* `loader` is ignored for content without loaders (resource, shader and
	 * data packs). */
	ModUpdateCheckTask(std::shared_ptr<ModMetadataIndex> index,
					   QString mcVersion, QString loader,
					   ModPlatform::ContentType contentType,
					   QObject* parent = nullptr);

	/* What was found so far; still meaningful after abort(). */
	QList<UpdateInfo> availableUpdates() const
	{
		return m_updates;
	}

	/* One lookup per tracked mod, so a large instance takes a while. */
	bool canAbort() const override
	{
		return true;
	}

  public slots:
	bool abort() override;

  protected:
	void executeTask() override;

  private:
	void onOneDone();

	std::shared_ptr<ModMetadataIndex> m_index;
	QString m_mcVersion;
	QString m_loader;
	ModPlatform::ContentType m_contentType = ModPlatform::ContentType::Mod;
	QList<UpdateInfo> m_updates;
	int m_pending = 0;
	int m_total = 0;
	int m_completed = 0;

	/* Lookups in flight, for abort(). Guarded: a job deletes itself after
	 * reporting. */
	QList<QPointer<NetJob>> m_activeJobs;
	/* Latched by abort(): replies are dropped and no second verdict given. */
	bool m_aborted = false;
};
