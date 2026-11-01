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

#include <QByteArray>
#include <QDir>
#include <QFileInfoList>
#include <QFuture>
#include <QFutureWatcher>
#include <QList>
#include <QMap>
#include <QPointer>
#include <QString>

#include <atomic>

#include "MMCZip.h"
#include "QObjectPtr.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

class MinecraftInstance;

/* Writes an instance out as a Modrinth pack (`.mrpack`): a manifest plus
 * `overrides/`. Files the manifest can name by URL are downloaded on
 * install; the rest are carried. Naming comes from the instance's sidecar
 * index (ModMetadataIndex); files without a record are looked up once by
 * hash, and what stays unresolved ships in `overrides/`. Only URLs on the
 * hosts the format allows (ModrinthApi::mrpackHosts()) are named, as
 * those are what our importer fetches from. */
class ModrinthPackExportTask : public Task
{
	Q_OBJECT
  public:
	ModrinthPackExportTask(QString name, QString version, QString summary,
						   bool optionalFiles, MinecraftInstance* instance,
						   QString output,
						   MMCZip::FilterFileFunction filter,
						   QObject* parent = nullptr);

	bool canAbort() const override
	{
		return true;
	}

  public slots:
	bool abort() override;

  protected:
	void executeTask() override;

  private:
	/* A file the manifest can name: on a trusted host, with enough known for
	 * an installer to verify it. */
	struct ResolvedFile {
		QString url;
		QString sha1;
		QString sha512;
		qint64 size = 0;
		/* "client" / "server" / "both" / empty, as the sidecar recorded it.
		 * Decides the manifest's `env` block. */
		QString side;
	};

	struct PendingFile {
		QString path; /* relative to the game directory */
		QString sha1;
		QString sha512;
		qint64 size = 0;
	};

	struct ScanResult {
		bool ok = false;
		QFileInfoList files;
		QMap<QString, ResolvedFile> resolved;
		QList<PendingFile> pending;
	};

	/* Runs on a worker thread: walks the game directory, hashes candidate
	 * archives and reads their sidecars (seconds of I/O). */
	ScanResult scanFiles() const;
	void onScanFinished();

	/* One lookup per file not resolved locally. A failure is not an error:
	 * the mod travels inside the pack. */
	void lookUpPendingFiles();

	/* Folds one lookup's answer into m_resolved if it named the file asked
	 * about. Separate from the bookkeeping: a useless answer still counts as
	 * answered. */
	void recordLookupResult(const PendingFile& pending,
							const QByteArray& bytes);
	void onOneLookupDone();

	void buildZip();
	QByteArray generateIndex() const;

	const QString m_name;
	const QString m_version;
	const QString m_summary;
	const bool m_optionalFiles;
	MinecraftInstance* m_instance;
	const QDir m_gameRoot;
	const QString m_output;
	const MMCZip::FilterFileFunction m_filter;

	QFileInfoList m_files;
	QMap<QString, ResolvedFile> m_resolved;
	QList<PendingFile> m_pending;

	int m_lookupsOutstanding = 0;
	int m_lookupsDone = 0;
	int m_lookupsTotal = 0;
	QList<QPointer<NetJob>> m_activeJobs;

	Task::Ptr m_zipTask;

	QFuture<ScanResult> m_scanFuture;
	QFutureWatcher<ScanResult> m_scanWatcher;

	/* Latched by abort(); read by the scan between files and by handlers
	 * that could report a second verdict. */
	std::atomic<bool> m_aborted{false};
};
