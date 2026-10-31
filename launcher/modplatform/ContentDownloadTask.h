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

#include <QObject>
#include <QList>
#include <QString>
#include <memory>

#include "modplatform/ModDownloadTypes.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

class ModMetadataIndex;

class ContentDownloadTask : public Task
{
	Q_OBJECT

  public:
	explicit ContentDownloadTask(const QList<ModPlatform::DownloadItem>& items,
								 const QString& targetDir,
								 QObject* parent = nullptr);

	/* When set, a provenance sidecar is written for every file placed on disk
	 * and removed for any file superseded (`DownloadItem::replacesFileName`). */
	void setMetadataIndex(std::shared_ptr<ModMetadataIndex> index);

	/* The progress dialog has a Skip button. What is already on disk stays;
	 * the folder model rescans anyway. */
	bool canAbort() const override
	{
		return true;
	}

	/* Whether the user stopped it, as opposed to a transfer going wrong:
	 * emitAborted() sets the reason to "Aborted.", so the caller tells them
	 * apart. */
	bool wasAborted() const
	{
		return m_aborted;
	}

  public slots:
	bool abort() override;

  protected:
	void executeTask() override;

  private slots:
	void onDownloadSucceeded();
	void onDownloadFailed(QString reason);
	void onDownloadProgress(qint64 current, qint64 total);

  private:
	void writeSidecars();

	QList<ModPlatform::DownloadItem> m_items;
	QString m_targetDir;
	NetJob::Ptr m_netJob;
	std::shared_ptr<ModMetadataIndex> m_metadata;

	/* Latched by abort(): the job reports failed() as it unwinds, which must
	 * not be a second verdict. */
	bool m_aborted = false;
};
