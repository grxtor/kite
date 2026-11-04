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

#include "CrashDoctorModel.h"

#include <QCoreApplication>
#include <QFileInfo>

#include "CrashActions.h"
#include "CrashDoctorService.h"
#include "CrashFixer.h"
#include "core/LauncherContext.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "settings/SettingsObject.h"

namespace
{

	QString runnerName(CrashActions::Runner runner)
	{
		switch (runner) {
			case CrashActions::Runner::Fixer:
				return QStringLiteral("fixer");
			case CrashActions::Runner::Flow:
				return QStringLiteral("flow");
			case CrashActions::Runner::Navigation:
				return QStringLiteral("navigation");
		}
		return QString();
	}

	QString firstOf(const QVariantMap& captures, const QStringList& names)
	{
		for (const QString& name : names) {
			const QString value = captures.value(name).toString();
			if (!value.isEmpty()) {
				return value;
			}
		}
		return QString();
	}

	/// What one finding of a grouped card is about and who needs it, as the
	/// names the installed files give them; empty "what" when it names none.
	QPair<QString, QString> rowParts(const CrashDoctor::Finding& finding,
									 const QHash<QString, QString>& names)
	{
		const auto named = [&names](const QString& value) { return names.value(value, value); };
		return {named(firstOf(finding.captures, {"modName", "mod", "clsModName", "clsMod", "jar",
												 "file", "bName", "b"})),
				named(firstOf(finding.captures, {"byName", "by", "aName", "a"}))};
	}

	SettingsObject* launcherSettings()
	{
		auto* context = LAUNCHER;
		return context ? context->settings().get() : nullptr;
	}
} // namespace

CrashDoctorModel::CrashDoctorModel(std::shared_ptr<MinecraftInstance> instance, QObject* parent)
	: QObject(parent), m_instance(std::move(instance))
{
	if (!m_instance) {
		return;
	}
	m_id = m_instance->id();
	auto* service = CrashDoctorService::instance();
	const auto mine = [this](const QString& id) { return id == m_id; };
	connect(service, &CrashDoctorService::analysed, this, [this, mine](const QString& id) {
		if (mine(id)) {
			emit changed();
		}
	});
	connect(service, &CrashDoctorService::forgotten, this, [this, mine](const QString& id) {
		if (mine(id)) {
			emit changed();
		}
	});
	connect(service, &CrashDoctorService::busyChanged, this, [this, mine](const QString& id) {
		if (mine(id)) {
			emit busyChanged();
		}
	});
	connect(service, &CrashDoctorService::sendStateChanged, this, &CrashDoctorModel::sendChanged);
	connect(this, &CrashDoctorModel::changed, this, &CrashDoctorModel::sendChanged);
}

CrashDoctorModel::~CrashDoctorModel() = default;

bool CrashDoctorModel::hasReport() const
{
	return CrashDoctorService::instance()->report(m_id).has_value();
}

bool CrashDoctorModel::busy() const
{
	return CrashDoctorService::instance()->busy(m_id);
}

QVariantList CrashDoctorModel::causes() const
{
	QVariantList out;
	const auto report = CrashDoctorService::instance()->report(m_id);
	if (!report) {
		return out;
	}
	for (const CrashDoctor::Cause& cause : report->causes) {
		QVariantList rows;
		for (const CrashDoctor::Finding& finding : cause.findings) {
			const auto [what, by] = rowParts(finding, report->modNames);
			rows.append(what.isEmpty()	 ? finding.line
						: by.isEmpty() ? what
									   : tr("%1 (needed by %2)").arg(what, by));
		}
		QVariantList fixes;
		for (const CrashDoctor::ResolvedFix& fix : cause.fixes) {
			const CrashActions::Spec* spec = CrashActions::find(fix.action);
			if (!spec) {
				continue;
			}
			QString confirm;
			if (spec->confirm) {
				const QStringList files = CrashDoctor::filesTouched(fix, *report);
				const QString names = files.join(QStringLiteral(", "));
				if (fix.action == QLatin1String("disableMod")) {
					confirm = tr("Turn off %1? The file is not deleted: you can turn it back on under Content.").arg(names);
				} else if (fix.action == QLatin1String("removeDuplicateMod")) {
					confirm = tr("Turn off the older copy (%1)? The file is not deleted.").arg(names);
				} else if (fix.action == QLatin1String("resetConfigFile")) {
					confirm = tr("Reset %1? Kite keeps the old file next to it, and Undo puts it back.").arg(names);
				} else if (fix.action == QLatin1String("resetOptions")) {
					confirm = tr("Reset options.txt? Keys and video settings go back to default. Kite keeps the old file, and Undo puts it back.");
				} else if (fix.action == QLatin1String("clearCustomJvmArgs")) {
					confirm = tr("Turn off the custom Java arguments for this instance? Undo brings them back.");
				}
			}
			fixes.append(QVariantMap{{"action", fix.action},
									 {"label", fix.label},
									 {"params", fix.params},
									 {"runner", runnerName(spec->runner)},
									 {"confirm", confirm}});
		}
		const CrashDoctor::Finding& first = cause.findings.first();
		out.append(QVariantMap{{"ruleId", cause.ruleId},
							   {"title", cause.title},
							   {"explanation", cause.explanation},
							   {"line", first.line},
							   {"count", int(cause.findings.size())},
							   {"rows", rows},
							   {"fixes", fixes}});
	}
	return out;
}

QString CrashDoctorModel::crashReport() const
{
	const auto report = CrashDoctorService::instance()->report(m_id);
	return report ? report->crashReportPath : QString();
}

QString CrashDoctorModel::crashReportName() const
{
	const QString path = crashReport();
	return path.isEmpty() ? QString() : QFileInfo(path).fileName();
}

int CrashDoctorModel::exitCode() const
{
	const auto report = CrashDoctorService::instance()->report(m_id);
	return report && report->exitCode ? *report->exitCode : 0;
}

bool CrashDoctorModel::hasExitCode() const
{
	const auto report = CrashDoctorService::instance()->report(m_id);
	return report && report->exitCode.has_value();
}

QString CrashDoctorModel::undoAction() const
{
	if (!m_instance) {
		return QString();
	}
	const auto undo = CrashDoctor::Undo::fromJson(
		m_instance->settings()->get("CrashDoctorUndo").toString());
	return undo ? undo->action : QString();
}

void CrashDoctorModel::analyse()
{
	if (m_instance) {
		CrashDoctorService::instance()->analyseFiles(*m_instance);
	}
}

QVariantMap CrashDoctorModel::applyFix(int causeIndex, int fixIndex)
{
	const auto fail = [](const QString& message) {
		return QVariantMap{{"ok", false}, {"message", message}};
	};
	const auto report = CrashDoctorService::instance()->report(m_id);
	if (!m_instance || !report || causeIndex < 0 || causeIndex >= report->causes.size()) {
		return fail(tr("This crash is no longer known."));
	}
	const CrashDoctor::Cause& cause = report->causes.at(causeIndex);
	if (fixIndex < 0 || fixIndex >= cause.fixes.size()) {
		return fail(tr("This crash is no longer known."));
	}
	const CrashDoctor::ResolvedFix& fix = cause.fixes.at(fixIndex);
	const CrashActions::Spec* spec = CrashActions::find(fix.action);
	if (!spec || spec->runner != CrashActions::Runner::Fixer) {
		return fail(QString());
	}
	if (m_instance->isRunning()) {
		return fail(tr("Close the game first."));
	}
	QString error;
	const CrashDoctor::Target target{*m_instance->settings(), m_instance->gameRoot(),
									 m_instance->modsRoot(), launcherSettings()};
	if (!CrashDoctor::applyFix(target, fix, *report, &error)) {
		return fail(error);
	}
	noteFixTried(causeIndex);
	emit undoChanged();
	return QVariantMap{{"ok", true}, {"message", tr("Done. Play again to see if it works.")}};
}

QVariantMap CrashDoctorModel::undo()
{
	if (!m_instance) {
		return {{"ok", false}, {"message", QString()}};
	}
	const auto record = CrashDoctor::Undo::fromJson(
		m_instance->settings()->get("CrashDoctorUndo").toString());
	if (!record) {
		return {{"ok", false}, {"message", tr("There is nothing to undo.")}};
	}
	if (m_instance->isRunning()) {
		return {{"ok", false}, {"message", tr("Close the game first.")}};
	}
	QString error;
	const CrashDoctor::Target target{*m_instance->settings(), m_instance->gameRoot(),
									 m_instance->modsRoot(), launcherSettings()};
	const bool ok = CrashDoctor::undoFix(target, *record, &error);
	emit undoChanged();
	return {{"ok", ok}, {"message", ok ? tr("Undone.") : error}};
}

void CrashDoctorModel::dismiss()
{
	CrashDoctorService::instance()->forget(m_id);
}

QStringList CrashDoctorModel::fileKeysFor(const QStringList& modIds) const
{
	QStringList keys;
	const auto report = CrashDoctorService::instance()->report(m_id);
	for (const QString& id : modIds) {
		QString key;
		if (report) {
			const auto files = report->modFiles.value(id.toLower());
			if (!files.isEmpty()) {
				key = ModMetadataIndex::canonicalFileName(QFileInfo(files.first().path).fileName());
			}
		}
		keys.append(key);
	}
	return keys;
}
