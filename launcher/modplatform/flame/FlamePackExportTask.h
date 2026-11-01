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
#include <QString>

#include <atomic>

#include "MMCZip.h"
#include "QObjectPtr.h"
#include "tasks/Task.h"

class MinecraftInstance;

namespace Net
{
	class JsonPost;
}

/* What the export dialog collected, as one struct so fields cannot be
 * swapped in a long constructor. */
struct FlamePackExportOptions {
	QString name;
	QString version;
	QString author;
	bool optionalFiles = true;
	MinecraftInstance* instance = nullptr;
	QString output;
	MMCZip::FilterFileFunction filter;
	/* MiB; 0 = no requirement stated. */
	int recommendedRAM = 0;
};

/* Writes an instance out as a CurseForge pack: a manifest plus
 * `overrides/`, with whatever the manifest can name left out. CurseForge
 * only knows files by FlameFingerprint and answers in bulk over POST, so
 * this hashes candidates on a worker thread, asks about the unknown ones
 * at once, then asks `/v1/mods` for the title, slug and authors that
 * `modlist.html` prints (sidecars hold no author list). Only `mods/` and
 * `resourcepacks/` are named; everything else goes in `overrides/`. */
class FlamePackExportTask : public Task
{
	Q_OBJECT
  public:
	explicit FlamePackExportTask(FlamePackExportOptions options,
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
	/* A file the manifest can name. `addonId` / `fileId` are CurseForge's
	 * project and file ids; the rest is for the mod list. */
	struct ResolvedFile {
		int addonId = 0;
		int fileId = 0;
		/* Whether the file is switched on, which decides `required`. */
		bool enabled = true;
		/* Resource packs are named in the manifest but left out of the mod list. */
		bool isMod = true;

		QString name;
		QString slug;
		QString authors;
	};

	struct PendingFile {
		QString path; /* relative to the game directory */
		quint32 fingerprint = 0;
		bool enabled = true;
		bool isMod = true;
	};

	struct ScanResult {
		bool ok = false;
		QFileInfoList files;
		QMap<QString, ResolvedFile> resolved;
		QList<PendingFile> pending;
	};

	/* Runs on a worker thread: walks the game directory, fingerprints the
	 * candidates and reads their sidecars. */
	ScanResult scanFiles() const;
	void onScanFinished();

	/* The bulk fingerprint question; skipped when sidecars covered everything. */
	void matchFingerprints();
	void onFingerprintsMatched();

	void lookUpProjects();
	void onProjectsLookedUp();

	void buildZip();
	QByteArray generateManifest() const;
	QByteArray generateModList() const;

	/* Neither question is worth failing an export over: a failure carries on
	 * (files ship in the pack, or a mod list entry uses the sidecar). */
	void onFingerprintMatchFailed(const QString& reason);
	void onProjectLookupFailed(const QString& reason);

	const FlamePackExportOptions m_options;
	const QDir m_gameRoot;

	QFileInfoList m_files;
	QMap<QString, ResolvedFile> m_resolved;
	QList<PendingFile> m_pending;

	shared_qobject_ptr<Net::JsonPost> m_request;
	Task::Ptr m_zipTask;

	QFuture<ScanResult> m_scanFuture;
	QFutureWatcher<ScanResult> m_scanWatcher;

	/* Latched by abort(); read by the scan between files and by handlers
	 * that could report a second verdict. */
	std::atomic<bool> m_aborted{false};
};
