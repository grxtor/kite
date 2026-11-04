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

#include "CrashFollowUp.h"

#include <QDir>
#include <QFileInfo>
#include <QLocale>

#include <algorithm>

#include "BuildConfig.h"
#include "CrashOutbox.h"
#include "CrashPayload.h"
#include "InstanceList.h"
#include "Logging.h"
#include "core/LauncherContext.h"
#include "meta/MetaCatalog.h"
#include "minecraft/MinecraftInstance.h"
#include "plugin/PluginHooks.h"
#include "settings/SettingsObject.h"

namespace
{
	const QString kFile = QStringLiteral("unresolved.json");
	const QString kAutoSendSetting = QStringLiteral("CrashReportsAutoSend");
	const QString kServerUrlSetting = QStringLiteral("FriendsServerUrl");
	constexpr int kDaysInWeek = 7;

	QString keyOf(const CrashWatch::Entry& entry)
	{
		return entry.instanceId + QLatin1Char('/') + entry.signature;
	}

	SettingsObject* settings()
	{
		auto* context = LAUNCHER;
		return context ? context->settings().get() : nullptr;
	}

	bool exists(const QString& path)
	{
		return QFileInfo::exists(path);
	}
} // namespace

CrashFollowUp::CrashFollowUp(const QString& dir, Runner runner, QObject* parent)
	: QObject(parent), m_path(QDir(dir).filePath(kFile)), m_runner(std::move(runner))
{
	m_outbox = new CrashOutbox(dir, [] {
		auto* launcher = settings();
		return launcher ? launcher->get(kServerUrlSetting).toString() : QString();
	}, this);
	m_entries = CrashWatch::load(m_path);
}

CrashFollowUp::~CrashFollowUp() = default;

void CrashFollowUp::replace(const QList<CrashWatch::Entry>& entries)
{
	m_entries = entries;
	if (!CrashWatch::save(m_path, m_entries)) {
		qCWarning(crashDoctorLog) << "Could not keep the crashes to read again in" << m_path;
	}
}

void CrashFollowUp::prune()
{
	replace(CrashWatch::pruned(m_entries, QDateTime::currentDateTimeUtc(), exists));
}

void CrashFollowUp::analysed(const CrashDoctor::Report& report, bool fresh, int catalogVersion)
{
	if (!fresh || report.signature.isEmpty()) {
		return;
	}
	if (report.causes.isEmpty()) {
		const QDateTime when = report.launchedAt.isValid() ? report.launchedAt : report.analysedAt;
		replace(CrashWatch::added(m_entries, {report.instanceId, report.signature, when.toUTC(), report.files,
											  catalogVersion, QString()}));
	}
	auto* launcher = settings();
	if (CrashPayload::sendable(report) && launcher && launcher->get(kAutoSendSetting).toBool()) {
		m_autoSent.insert(report.signature);
		m_outbox->send(CrashPayload::build(report, BuildConfig.printableVersionString()));
	}
}

void CrashFollowUp::send(const CrashDoctor::Report& report, bool alwaysFromNow)
{
	if (!CrashPayload::sendable(report)) {
		return;
	}
	if (auto* launcher = settings(); alwaysFromNow && launcher) {
		launcher->set(kAutoSendSetting, true);
	}
	m_outbox->send(CrashPayload::build(report, BuildConfig.printableVersionString()));
}

void CrashFollowUp::start(int catalogVersion)
{
	prune();
	m_outbox->retry();
	// A fix found before a restart shows again; nobody is told twice. Copies:
	// reading can change the list.
	const QList<CrashWatch::Entry> entries = m_entries;
	for (const CrashWatch::Entry& entry : entries) {
		if (!entry.solvedBy.isEmpty()) {
			reread(entry, catalogVersion, true);
		}
	}
	catalogChanged(catalogVersion);
}

void CrashFollowUp::catalogChanged(int catalogVersion)
{
	const QList<CrashWatch::Entry> entries = m_entries;
	for (const CrashWatch::Entry& entry : entries) {
		if (entry.solvedBy.isEmpty() && entry.catalogVersion < catalogVersion) {
			reread(entry, catalogVersion, false);
		}
	}
}

void CrashFollowUp::reread(const CrashWatch::Entry& entry, int catalogVersion, bool quiet)
{
	auto* context = LAUNCHER;
	if (!context || !context->instances() || m_reading.contains(keyOf(entry))) {
		return;
	}
	auto instance = std::dynamic_pointer_cast<MinecraftInstance>(context->instances()->getInstanceById(entry.instanceId));
	if (!instance) {
		replace(CrashWatch::without(m_entries, entry.instanceId, false));
		return;
	}
	if (instance->isRunning()) {
		return;
	}
	CrashDoctor::Snapshot snapshot = CrashDoctor::snapshot(*instance, nullptr);
	snapshot.files = entry.files;
	m_reading.insert(keyOf(entry));
	m_runner(snapshot, [this, entry, catalogVersion, quiet](const CrashDoctor::Report& report) {
		m_reading.remove(keyOf(entry));
		reread(entry, catalogVersion, quiet, report);
	});
}

void CrashFollowUp::reread(const CrashWatch::Entry& entry, int catalogVersion, bool quiet,
						   const CrashDoctor::Report& report)
{
	QList<CrashWatch::Entry> entries = m_entries;
	const auto it = std::find_if(entries.begin(), entries.end(),
								 [&entry](const CrashWatch::Entry& one) { return keyOf(one) == keyOf(entry); });
	if (it == entries.end()) {
		return;
	}
	// The files tell another run now (latest.log was written again): nothing left to read.
	if (report.signature != entry.signature) {
		entries.erase(it);
		replace(entries);
		return;
	}
	it->catalogVersion = std::max(it->catalogVersion, catalogVersion);
	if (report.causes.isEmpty()) {
		replace(entries);
		return;
	}
	const bool news = it->solvedBy.isEmpty();
	it->solvedBy = report.causes.first().ruleId;
	replace(entries);
	CrashDoctor::Report solvedReport = report;
	solvedReport.solvedLaterFor = entry.crashedAt;
	emit solved(solvedReport);
	if (news && !quiet) {
		notify(entry, solvedReport);
	}
}

void CrashFollowUp::notify(const CrashWatch::Entry& entry, const CrashDoctor::Report& report)
{
	auto* context = LAUNCHER;
	const auto instance = context && context->instances() ? context->instances()->getInstanceById(entry.instanceId)
														  : nullptr;
	if (!instance) {
		return;
	}
	const QByteArray id = entry.instanceId.toUtf8();
	const QByteArray name = instance->name().toUtf8();
	const QByteArray rule = report.causes.first().ruleId.toUtf8();
	const QByteArray title = tr("A fix was found").toUtf8();
	const QByteArray body = fixFoundText(instance->name(), entry.crashedAt, QDateTime::currentDateTimeUtc(),
										 MetaCatalog::selectedLanguage())
								.toUtf8();
	MMCOCrashFixNotice payload{id.constData(), name.constData(), rule.constData(), title.constData(),
							   body.constData()};
	qCInfo(crashDoctorLog) << "Newer crash rules know a kept crash of" << entry.instanceId << ":" << rule;
	context->dispatchPluginHook(MMCO_HOOK_CRASH_FIX_FOUND, &payload);
}

QString CrashFollowUp::fixFoundText(const QString& instanceName, const QDateTime& crashedAt,
									const QDateTime& now, const QString& language)
{
	const QLocale locale(language.isEmpty() ? QStringLiteral("en") : language);
	const QDate day = crashedAt.toLocalTime().date();
	const qint64 daysAgo = day.daysTo(now.toLocalTime().date());
	if (daysAgo <= 0) {
		return tr("Today's %1 crash has a fix now.").arg(instanceName);
	}
	if (daysAgo == 1) {
		return tr("Yesterday's %1 crash has a fix now.").arg(instanceName);
	}
	if (daysAgo < kDaysInWeek) {
		// Turkish writes weekdays in lower case inside a sentence.
		const QString weekday = locale.dayName(day.dayOfWeek(), QLocale::LongFormat);
		const QString word = language.startsWith(QLatin1String("tr")) ? locale.toLower(weekday) : weekday;
		return tr("Last %1's %2 crash has a fix now.").arg(word, instanceName);
	}
	return tr("The %1 crash of %2 has a fix now.").arg(instanceName, locale.toString(day, QStringLiteral("d MMMM")));
}

void CrashFollowUp::launched(const QString& instanceId)
{
	replace(CrashWatch::without(m_entries, instanceId, true));
}

void CrashFollowUp::ranClean(const QString& instanceId)
{
	replace(CrashWatch::without(m_entries, instanceId, false));
}

void CrashFollowUp::dismissed(const QString& instanceId)
{
	replace(CrashWatch::without(m_entries, instanceId, true));
}
