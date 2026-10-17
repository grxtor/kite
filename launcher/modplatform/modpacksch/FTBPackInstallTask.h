/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2020-2021 Jamie Mansfield <jmansfield@cadixdev.org>
 * Copyright 2020-2021 Petr Mrazek <peterix@gmail.com>
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

#include "FTBPackManifest.h"
#include "modplatform/BlockedMod.h"

#include <QHash>
#include <QList>
#include <QStringList>

#include "InstanceTask.h"
#include "net/NetJob.h"

namespace ModpacksCH
{
	/* Sorts the URLs a pack's download job could not fetch: a file FTB
	 * serves from CurseForge's CDN (it has CurseForge ids) can be fetched
	 * by hand, so it becomes a BlockedMod for the restricted-files
	 * question, aimed at @p targets' path for it; anything else is named
	 * in @p unrecoverable. @p files and @p targets are keyed by the
	 * download URL as the job reports it. */
	void classifyFailedDownloads(const QStringList& failedUrls,
								 const QHash<QString, VersionFile>& files,
								 const QHash<QString, QString>& targets,
								 QList<BlockedMod>& blocked,
								 QStringList& unrecoverable);

	/* The failure message for files nothing can be done about: the first
	 * few by name and a count of the rest, since a dropped connection
	 * fails hundreds of them at once. */
	QString undownloadableMessage(const QStringList& names);


	class PackInstallTask : public InstanceTask
	{
		Q_OBJECT

	  public:
		explicit PackInstallTask(Modpack pack, QString version);
		virtual ~PackInstallTask() {}

		bool canAbort() const override
		{
			/* Two abortable stretches, with a gap between them: the
			 * pack's own downloads while `abortable` is set, and the
			 * optional game-file download the base class runs once the
			 * instance is built. Answering "always" would light up a
			 * button that abort() then refuses to act on. */
			return abortable || InstanceTask::canAbort();
		}
		bool abort() override;

	  protected:
		virtual void executeTask() override;

	  private slots:
		void onDownloadSucceeded();
		void onDownloadFailed(QString reason);

	  private:
		/* Some files failed: hand the ones CurseForge restricts to the
		 * user, as CurseForge packs do, or fail naming the rest. */
		void onModsDownloadFailed(const QString& reason);
		void downloadPack();
		void install();

	  private:
		bool abortable = false;

		NetJob::Ptr jobPtr;
		QByteArray response;

		Modpack m_pack;
		QString m_version_name;
		Version m_version;

		QMap<QString, QString> filesToCopy;
		/* By download URL (as the job reports it): the file, and where it
		 * goes in the staging folder. */
		QHash<QString, VersionFile> m_filesByUrl;
		QHash<QString, QString> m_targetByUrl;
	};

} // namespace ModpacksCH
