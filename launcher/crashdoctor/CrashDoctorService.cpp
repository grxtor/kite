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
#include <QFile>
#include <QFutureWatcher>
#include <QUrl>
#include <QtConcurrent>

#include "BuildConfig.h"
#include "Logging.h"
#include "core/LauncherContext.h"
#include "launch/LaunchTask.h"
#include "minecraft/MinecraftInstance.h"
#include "net/Download.h"
#include "net/HttpMetaCache.h"
#include "translations/TranslationsModel.h"

namespace
{
	const QString kEmbeddedCatalog = QStringLiteral(":/kite/crash-rules.json");
	const QString kCatalogPath = QStringLiteral("kite/crash-rules.json");
	const QString kCache = QStringLiteral("general");

	QString language()
	{
		auto* context = LAUNCHER;
		auto translations = context ? context->translations() : nullptr;
		return translations ? translations->selectedLanguage() : QString();
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
	return true;
}

void CrashDoctorService::refreshCatalog()
{
	auto* context = LAUNCHER;
	auto cache = context ? context->metacache() : nullptr;
	if (m_refreshed || !cache) {
		return;
	}
	m_refreshed = true;
	// The copy downloaded last time first, then ask for a newer one.
	if (auto cachedEntry = cache->getEntry(kCache, kCatalogPath)) {
		QFile cached(cachedEntry->getFullPath());
		if (cached.open(QIODevice::ReadOnly)) {
			useCatalog(cached.readAll(), QStringLiteral("cached"));
		}
	}
	auto entry = cache->resolveEntry(kCache, kCatalogPath);
	entry->setStale(true);
	m_refreshJob.reset(new NetJob(tr("Crash rules"), context->network()));
	m_refreshJob->addNetAction(
		Net::Download::makeCached(QUrl(BuildConfig.META_URL + kCatalogPath), entry));
	connect(m_refreshJob.get(), &NetJob::succeeded, this, [this, entry] {
		QFile file(entry->getFullPath());
		if (file.open(QIODevice::ReadOnly)) {
			useCatalog(file.readAll(), QStringLiteral("downloaded"));
		}
	});
	connect(m_refreshJob.get(), &NetJob::failed, this, [](QString reason) {
		qCDebug(crashDoctorLog) << "No newer crash rules:" << reason;
	});
	m_refreshJob->start();
}

void CrashDoctorService::watch(const shared_qobject_ptr<LaunchTask>& task)
{
	if (!task || !task->instance()) {
		return;
	}
	const QString id = task->instance()->id();
	forget(id);
	refreshCatalog();
	LaunchTask* raw = task.get();
	connect(raw, &Task::finished, this, [this, raw, id] { onFinished(raw, id); });
}

void CrashDoctorService::onFinished(LaunchTask* task, const QString& instanceId)
{
	auto instance = std::dynamic_pointer_cast<MinecraftInstance>(task->instance());
	// A Kill is no crash, and a clean run has nothing to say.
	if (!instance || task->isAborting() || !instance->hasCrashed()) {
		return;
	}
	Q_UNUSED(instanceId);
	run(CrashDoctor::snapshot(*instance, task), true);
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
	auto* watcher = new QFutureWatcher<CrashDoctor::Report>(this);
	connect(watcher, &QFutureWatcher<CrashDoctor::Report>::finished, this,
			[this, watcher, id, fresh] {
				m_reports.insert(id, watcher->result());
				m_busy.remove(id);
				watcher->deleteLater();
				emit busyChanged(id);
				emit analysed(id, fresh);
			});
	watcher->setFuture(QtConcurrent::run(
		[catalog = m_catalog, snapshot, lang = language()] {
			return CrashDoctor::analyse(catalog, snapshot, lang);
		}));
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
	if (m_reports.remove(instanceId) > 0) {
		emit forgotten(instanceId);
	}
}

QString CrashDoctorService::topCause(const QString& instanceId) const
{
	const auto it = m_reports.constFind(instanceId);
	return it == m_reports.constEnd() || it->causes.isEmpty() ? QString()
															  : it->causes.first().title;
}
