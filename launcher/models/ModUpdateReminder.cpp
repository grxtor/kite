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

#include "ModUpdateReminder.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFutureWatcher>
#include <QSet>
#include <QtConcurrent>

#include <memory>

#include "InstanceList.h"
#include "core/LauncherContext.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "plugin/PluginHooks.h"
#include "settings/Setting.h"
#include "settings/SettingsObject.h"

namespace
{
	std::shared_ptr<MinecraftInstance> minecraftInstance(const QString& id)
	{
		if (!LAUNCHER || id.isEmpty()) {
			return nullptr;
		}
		return std::dynamic_pointer_cast<MinecraftInstance>(
			LAUNCHER->instances()->getInstanceById(id));
	}

	qint64 nowMs()
	{
		return QDateTime::currentMSecsSinceEpoch();
	}
} // namespace

QString ModUpdateReminder::stampFor(const QString& mcVersion, const QString& loader)
{
	if (mcVersion.isEmpty()) {
		return QString();
	}
	return loader.isEmpty() ? mcVersion : mcVersion + QLatin1Char(' ') + loader;
}

bool ModUpdateReminder::checkDue(const Record& record, const QString& stamp,
								 qint64 now)
{
	if (stamp.isEmpty()) {
		return false;
	}
	if (record.checkedAtMs <= 0 || record.checkedFor != stamp) {
		return true;
	}
	/* A clock set back counts as a day gone by, not as never again. */
	return now - record.checkedAtMs >= kCheckIntervalMs || now < record.checkedAtMs;
}

int ModUpdateReminder::shownCount(const Record& record, const QString& stamp)
{
	return !stamp.isEmpty() && record.checkedFor == stamp ? qMax(0, record.found) : 0;
}

int ModUpdateReminder::countUpdates(const QList<ModUpdateCheckTask::UpdateInfo>& updates)
{
	QSet<QString> files;
	for (const auto& update : updates) {
		files.insert(ModMetadataIndex::canonicalFileName(update.currentFileName));
	}
	return int(files.size());
}

bool ModUpdateReminder::answered(int lookups, int failed)
{
	return lookups <= 0 || failed < lookups;
}

bool ModUpdateReminder::notifyDue(bool optedIn, qint64 lastNotifiedMs, qint64 now)
{
	if (!optedIn) {
		return false;
	}
	return lastNotifiedMs <= 0 || now - lastNotifiedMs >= kNotifyIntervalMs ||
		   now < lastNotifiedMs;
}

ModUpdateReminder::Record ModUpdateReminder::recordOf(const SettingsObject& settings)
{
	Record record;
	record.checkedAtMs = settings.get(kCheckedAtSetting).toLongLong();
	record.found = settings.get(kFoundSetting).toInt();
	record.checkedFor = settings.get(kForSetting).toString();
	return record;
}

QString ModUpdateReminder::stampOf(MinecraftInstance& instance)
{
	auto profile = instance.getPackProfile();
	if (!profile) {
		return QString();
	}
	return stampFor(profile->getComponentVersion("net.minecraft"),
					profile->primaryModLoader());
}

ModUpdateReminder* ModUpdateReminder::instance()
{
	static ModUpdateReminder* reminder =
		new ModUpdateReminder(QCoreApplication::instance());
	return reminder;
}

ModUpdateReminder::ModUpdateReminder(QObject* parent) : QObject(parent)
{
	if (LAUNCHER) {
		if (auto setting = LAUNCHER->settings()->getSetting(kEnabledSetting)) {
			connect(setting.get(), &Setting::SettingChanged, this,
					&ModUpdateReminder::bump);
		}
	}
}

ModUpdateReminder::~ModUpdateReminder()
{
	if (m_task && m_task->isRunning()) {
		m_task->abort();
	}
}

bool ModUpdateReminder::enabled() const
{
	return LAUNCHER && LAUNCHER->settings()->get(kEnabledSetting).toBool();
}

void ModUpdateReminder::bump()
{
	++m_revision;
	emit changed();
}

void ModUpdateReminder::check(const QString& instanceId)
{
	if (!enabled() || instanceId == m_running || m_queue.contains(instanceId)) {
		return;
	}
	auto instance = minecraftInstance(instanceId);
	if (!instance || instance->isManagedPack()) {
		return;
	}
	auto profile = instance->getPackProfile();
	/* Without a loader there are no mods to update. */
	if (!profile || profile->primaryModLoader().isEmpty()) {
		return;
	}
	const QString stamp = stampOf(*instance);
	if (!checkDue(recordOf(*instance->settings()), stamp, nowMs())) {
		return;
	}
	m_queue.append(instanceId);
	startNext();
}

int ModUpdateReminder::count(const QString& instanceId) const
{
	if (!enabled()) {
		return 0;
	}
	auto instance = minecraftInstance(instanceId);
	if (!instance || instance->isManagedPack()) {
		return 0;
	}
	return shownCount(recordOf(*instance->settings()), stampOf(*instance));
}

void ModUpdateReminder::startNext()
{
	if (!m_running.isEmpty() || m_queue.isEmpty()) {
		return;
	}
	const QString id = m_queue.takeFirst();
	auto instance = minecraftInstance(id);
	auto profile = instance ? instance->getPackProfile() : nullptr;
	if (!profile) {
		startNext();
		return;
	}
	m_running = id;
	const QString stamp = stampOf(*instance);
	const QString mcVersion = profile->getComponentVersion("net.minecraft");
	const QString loader = profile->primaryModLoader();
	auto index = std::make_shared<ModMetadataIndex>(QDir(instance->modsRoot()));

	/* The sidecars are read off the interface thread: a big pack has hundreds. */
	auto* loading = new QFutureWatcher<void>(this);
	connect(loading, &QFutureWatcher<void>::finished, this,
			[this, loading, index, id, stamp, mcVersion, loader] {
				loading->deleteLater();
				auto* task = new ModUpdateCheckTask(index, mcVersion, loader,
													ModPlatform::ContentType::Mod, this);
				m_task = task;
				connect(task, &Task::finished, this, [this, task, id, stamp] {
					task->deleteLater();
					finish(id, stamp, task->lookups(), task->failedLookups(),
						   countUpdates(task->availableUpdates()));
				});
				task->start();
			});
	loading->setFuture(QtConcurrent::run([index] { index->load(); }));
}

void ModUpdateReminder::finish(const QString& instanceId, const QString& stamp,
							   int lookups, int failed, int found)
{
	m_running.clear();
	m_task = nullptr;
	auto instance = minecraftInstance(instanceId);
	/* Offline, or the instance changed while it was asked about: next time. */
	if (instance && answered(lookups, failed) && stampOf(*instance) == stamp) {
		const int before = shownCount(recordOf(*instance->settings()), stamp);
		record(*instance, found);
		if (found > before) {
			notify(*instance, found);
		}
	}
	startNext();
}

void ModUpdateReminder::record(MinecraftInstance& instance, int found)
{
	auto settings = instance.settings();
	settings->set(kCheckedAtSetting, nowMs());
	settings->set(kFoundSetting, qMax(0, found));
	settings->set(kForSetting, stampOf(instance));
	bump();
}

void ModUpdateReminder::debugRecord(const QString& instanceId, int found)
{
	if (auto instance = minecraftInstance(instanceId)) {
		record(*instance, found);
	}
}

void ModUpdateReminder::notify(MinecraftInstance& instance, int found)
{
	if (!LAUNCHER || found <= 0) {
		return;
	}
	auto settings = LAUNCHER->settings();
	const qint64 now = nowMs();
	if (!notifyDue(settings->get(kNotifySetting).toBool(),
				   settings->get(kNotifiedAtSetting).toLongLong(), now)) {
		return;
	}
	settings->set(kNotifiedAtSetting, now);
	const QByteArray id = instance.id().toUtf8();
	const QByteArray name = instance.name().toUtf8();
	const QByteArray title = tr("New mod versions").toUtf8();
	const QByteArray body =
		(found == 1 ? tr("A mod in %1 has a new version.").arg(instance.name())
					: tr("%1 mods in %2 have new versions.").arg(found).arg(instance.name()))
			.toUtf8();
	MMCOModUpdatesNotice payload{id.constData(), name.constData(), found,
								 title.constData(), body.constData()};
	LAUNCHER->dispatchPluginHook(MMCO_HOOK_MOD_UPDATES_FOUND, &payload);
}
