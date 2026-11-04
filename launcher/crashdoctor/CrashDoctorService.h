/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <optional>

#include "CrashCollector.h"
#include "CrashRules.h"
#include "QObjectPtr.h"
#include "net/NetJob.h"

class LaunchTask;
class MinecraftInstance;

/*
 * The crash doctor for the whole launcher: watches every launch, and when one
 * ends in a crash (a failed launch that was not a Kill, or a run that left a
 * crash report) analyses it on a worker thread and keeps the report until the
 * instance launches again. The catalog is the embedded one, replaced by the
 * meta server's copy when that one is valid and not older (asked once a run).
 * Nothing leaves the computer: the only network use is fetching the catalog.
 */
class CrashDoctorService : public QObject
{
	Q_OBJECT

  public:
	/// The one service, made on first use on the calling (GUI) thread.
	static CrashDoctorService* instance();

	const CrashDoctor::Catalog& catalog() const
	{
		return m_catalog;
	}
	/// Replaces the catalog with @p json if it is valid and its catalogVersion
	/// is not lower than the current one. Whether it did.
	bool useCatalog(const QByteArray& json, const QString& from);

	/// Watches @p task; MinecraftInstance::createLaunchTask() calls it.
	void watch(const shared_qobject_ptr<LaunchTask>& task);
	/// Analyses @p instance's last run from its files (no launch to watch).
	/// @p fresh says the crash just happened (only the dev route sets it).
	void analyseFiles(MinecraftInstance& instance, bool fresh = false);

	std::optional<CrashDoctor::Report> report(const QString& instanceId) const;
	bool busy(const QString& instanceId) const;
	/// Forgets the report (a new launch, or the player dismissed it).
	void forget(const QString& instanceId);

	/// The title of the first cause of @p instanceId's report; "" without one.
	QString topCause(const QString& instanceId) const;

  signals:
	/// A report for @p instanceId is ready, possibly with no known cause.
	/// @p fresh: the crash of a launch that just ended, as opposed to one
	/// read again later; only a fresh one opens the "why" window.
	void analysed(const QString& instanceId, bool fresh);
	void busyChanged(const QString& instanceId);
	void forgotten(const QString& instanceId);

  private:
	explicit CrashDoctorService(QObject* parent = nullptr);
	void loadCatalog();
	void refreshCatalog();
	void onFinished(LaunchTask* task, const QString& instanceId);
	void run(const CrashDoctor::Snapshot& snapshot, bool fresh);

	CrashDoctor::Catalog m_catalog;
	QHash<QString, CrashDoctor::Report> m_reports;
	QSet<QString> m_busy;
	bool m_refreshed = false;
	NetJob::Ptr m_refreshJob;
};
