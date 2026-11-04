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

#include "CrashDoctorService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QFutureWatcher>
#include <QtConcurrent>

#include "CrashExitCodes.h"
#include "CrashFollowUp.h"
#include "CrashOutbox.h"
#include "CrashStreak.h"
#include "Logging.h"
#include "core/LauncherContext.h"
#include "launch/LaunchTask.h"
#include "meta/MetaCatalog.h"
#include "minecraft/MinecraftInstance.h"
#include "settings/SettingsObject.h"

namespace
{
	const QString kEmbeddedCatalog = QStringLiteral(":/kite/crash-rules.json");
	const QString kCatalogPath = QStringLiteral("kite/crash-rules.json");
	const QString kFollowUpDir = QStringLiteral("crash-reports");
	// After the window is up and the first requests of the start are done.
	constexpr int kFollowUpDelayMs = 20 * 1000;

	/// Keeps @p instance's native crashes in a row (NativeCrashStreak) after a
	/// run that crashed (with @p log, the run's live log) or, without, worked.
	void keepStreak(MinecraftInstance& instance, bool crashed, const QString& log = QString())
	{
		auto settings = instance.settings();
		const QString before = settings->get(CrashDoctor::kNativeCrashesSetting).toString();
		const auto streak = CrashDoctor::NativeCrashStreak::fromJson(before);
		CrashDoctor::NativeCrashStreak after;
		if (crashed) {
			const auto exit = CrashDoctor::exitFromLog(log);
			after = streak.afterCrash(exit && CrashDoctor::ExitCodes::windowsStatus(exit->first));
		} else {
			after = streak.afterCleanRun(QDateTime::currentDateTimeUtc());
		}
		// No write for an instance that never crashed natively.
		const QString json = after.toJson();
		if (json != streak.toJson()) {
			settings->set(CrashDoctor::kNativeCrashesSetting, json);
		}
	}
} // namespace

CrashDoctorService* CrashDoctorService::instance()
{
	static CrashDoctorService* service = [] {
		auto* made = new CrashDoctorService(QCoreApplication::instance());
		return made;
	}();
	return service;
}

CrashDoctorService::CrashDoctorService(QObject* parent) : QObject(parent)
{
	loadCatalog();
}

void CrashDoctorService::loadCatalog()
{
	// Only the embedded file here: the service can be made while the launcher
	// is still starting (InstanceList), before the meta cache exists.
	QFile embedded(kEmbeddedCatalog);
	if (!embedded.open(QIODevice::ReadOnly) ||
		!useCatalog(embedded.readAll(), QStringLiteral("built-in"))) {
		qCWarning(crashDoctorLog) << "The built-in crash rules are unusable";
	}
}

bool CrashDoctorService::useCatalog(const QByteArray& json, const QString& from)
{
	QString error;
	QStringList skipped;
	auto parsed = CrashDoctor::Catalog::parse(json, &error, &skipped);
	if (!parsed) {
		qCWarning(crashDoctorLog) << "Ignoring the" << from << "crash rules:" << error;
		return false;
	}
	for (const QString& line : skipped) {
		qCWarning(crashDoctorLog) << "Crash rule left out (" << from << "):" << line;
	}
	if (!m_catalog.rules.isEmpty() && parsed->catalogVersion < m_catalog.catalogVersion) {
		qCDebug(crashDoctorLog) << "Ignoring the" << from << "crash rules: older than"
								<< m_catalog.catalogVersion;
		return false;
	}
	m_catalog = std::move(*parsed);
	if (m_followUp) {
		m_followUp->catalogChanged(m_catalog.catalogVersion);
	}
	return true;
}

void CrashDoctorService::refreshCatalog()
{
	auto* context = LAUNCHER;
	if (m_refreshed || !context || !context->metacache()) {
		return;
	}
	m_refreshed = true;
	// The copy downloaded last time first, then ask for a newer one.
	const QByteArray cached = MetaCatalog::cached(kCatalogPath);
	if (!cached.isEmpty()) {
		useCatalog(cached, QStringLiteral("cached"));
	}
	m_refreshJob = MetaCatalog::refresh(
		kCatalogPath, tr("Crash rules"), this,
		[this](const QByteArray& json) { useCatalog(json, QStringLiteral("downloaded")); });
}

void CrashDoctorService::watch(const shared_qobject_ptr<LaunchTask>& task)
{
	if (!task || !task->instance()) {
		return;
	}
	const QString id = task->instance()->id();
	forget(id);
	followUp()->launched(id);
	refreshCatalog();
	LaunchTask* raw = task.get();
	connect(raw, &Task::finished, this, [this, raw, id] { onFinished(raw, id); });
}

void CrashDoctorService::onFinished(LaunchTask* task, const QString& instanceId)
{
	auto instance = std::dynamic_pointer_cast<MinecraftInstance>(task->instance());
	// A Kill is no crash, and a clean run has nothing to say, but that its old
	// crashes are done with.
	if (!instance || task->isAborting()) {
		return;
	}
	if (!instance->hasCrashed()) {
		keepStreak(*instance, false);
		followUp()->ranClean(instanceId);
		return;
	}
	// The snapshot reads the streak before this crash joins it.
	const CrashDoctor::Snapshot snapshot = CrashDoctor::snapshot(*instance, task);
	keepStreak(*instance, true, snapshot.liveLog);
	run(snapshot, true);
}

void CrashDoctorService::analyseFiles(MinecraftInstance& instance, bool fresh)
{
	if (busy(instance.id())) {
		return;
	}
	refreshCatalog();
	run(CrashDoctor::snapshot(instance, nullptr), fresh);
}

void CrashDoctorService::run(const CrashDoctor::Snapshot& snapshot, bool fresh)
{
	const QString id = snapshot.instanceId;
	m_busy.insert(id);
	emit busyChanged(id);
	CrashDoctor::Snapshot read = snapshot;
	if (!m_debugOs.isEmpty()) {
		read.facts.os = m_debugOs;
	}
	analyseAsync(read, [this, id, fresh](const CrashDoctor::Report& report) {
		m_reports.insert(id, report);
		m_busy.remove(id);
		emit busyChanged(id);
		emit analysed(id, fresh);
		followUp()->analysed(report, fresh, m_catalog.catalogVersion);
	});
}

void CrashDoctorService::analyseAsync(const CrashDoctor::Snapshot& snapshot,
									  std::function<void(const CrashDoctor::Report&)> done)
{
	auto* watcher = new QFutureWatcher<CrashDoctor::Report>(this);
	connect(watcher, &QFutureWatcher<CrashDoctor::Report>::finished, this,
			[watcher, done = std::move(done)] {
				done(watcher->result());
				watcher->deleteLater();
			});
	watcher->setFuture(QtConcurrent::run(
		[catalog = m_catalog, snapshot, lang = MetaCatalog::selectedLanguage()] {
			return CrashDoctor::analyse(catalog, snapshot, lang);
		}));
}

CrashFollowUp* CrashDoctorService::followUp()
{
	if (!m_followUp) {
		m_followUp = new CrashFollowUp(
			QDir::current().filePath(kFollowUpDir),
			[this](const CrashDoctor::Snapshot& snapshot, std::function<void(const CrashDoctor::Report&)> done) {
				analyseAsync(snapshot, std::move(done));
			},
			this);
		connect(m_followUp, &CrashFollowUp::solved, this, &CrashDoctorService::keepSolved);
		connect(m_followUp->outbox(), &CrashOutbox::stateChanged, this, &CrashDoctorService::sendStateChanged);
	}
	return m_followUp;
}

void CrashDoctorService::keepSolved(const CrashDoctor::Report& report)
{
	const QString id = report.instanceId;
	// A crash since then, or one being read, says more than an old one.
	const auto shown = m_reports.constFind(id);
	if (busy(id) || (shown != m_reports.constEnd() && !shown->solvedLaterFor.isValid())) {
		return;
	}
	m_reports.insert(id, report);
	emit analysed(id, false);
}

void CrashDoctorService::startFollowUp()
{
	QTimer::singleShot(kFollowUpDelayMs, this, [this] {
		if (followUp()->hasCrashes()) {
			refreshCatalog();
		}
		followUp()->start(m_catalog.catalogVersion);
	});
}

void CrashDoctorService::send(const QString& instanceId, bool alwaysFromNow)
{
	if (const auto it = m_reports.constFind(instanceId); it != m_reports.constEnd()) {
		followUp()->send(*it, alwaysFromNow);
	}
}

bool CrashDoctorService::debugUseCatalog(const QString& path)
{
	QFile file(path);
	followUp();
	return file.open(QIODevice::ReadOnly) && useCatalog(file.readAll(), QStringLiteral("dev"));
}

void CrashDoctorService::debugUseOs(const QString& os)
{
	m_debugOs = os;
}

std::optional<CrashDoctor::Report> CrashDoctorService::report(const QString& instanceId) const
{
	const auto it = m_reports.constFind(instanceId);
	if (it == m_reports.constEnd()) {
		return std::nullopt;
	}
	return *it;
}

bool CrashDoctorService::busy(const QString& instanceId) const
{
	return m_busy.contains(instanceId);
}

void CrashDoctorService::forget(const QString& instanceId)
{
	const auto it = m_reports.constFind(instanceId);
	if (it == m_reports.constEnd()) {
		return;
	}
	if (it->solvedLaterFor.isValid()) {
		followUp()->dismissed(instanceId);
	}
	m_reports.remove(instanceId);
	emit forgotten(instanceId);
}

QString CrashDoctorService::topCause(const QString& instanceId) const
{
	const auto it = m_reports.constFind(instanceId);
	return it == m_reports.constEnd() || it->causes.isEmpty() ? QString()
															  : it->causes.first().title;
}
