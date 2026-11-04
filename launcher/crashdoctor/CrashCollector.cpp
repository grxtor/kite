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

#include "CrashCollector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSysInfo>

#include <algorithm>

#include "CrashFiles.h"
#include "Sys.h"
#include "core/LauncherContext.h"
#include "java/JavaVersion.h"
#include "launch/LaunchTask.h"
#include "launch/LogModel.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/PerformanceSetupRules.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/launch/VerifyJavaInstall.h"
#include "minecraft/mod/LocalModParseTask.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/BundleCatalog.h"

namespace CrashDoctor
{
	namespace
	{
		const QString kVanilla = QStringLiteral("vanilla");
		const QString kLatestLog = QStringLiteral("logs/latest.log");
		const QHash<QString, QString> kLoaderComponents{
			{QStringLiteral("fabric"), QStringLiteral("net.fabricmc.fabric-loader")},
			{QStringLiteral("quilt"), QStringLiteral("org.quiltmc.quilt-loader")},
			{QStringLiteral("forge"), QStringLiteral("net.minecraftforge")},
			{QStringLiteral("neoforge"), QStringLiteral("net.neoforged")},
		};
		const QStringList kPerformanceBundles{QStringLiteral("performance"),
											  QStringLiteral("comfort")};
		/// latest.log is rewritten at each start; without a launch to go by,
		/// files this much older than its last line are from another run.
		constexpr int kRunWindowSecs = 60 * 60;

		QString osName()
		{
#if defined(Q_OS_MACOS)
			return QStringLiteral("macos");
#elif defined(Q_OS_WIN)
			return QStringLiteral("windows");
#else
			return QStringLiteral("linux");
#endif
		}

		QString archName()
		{
			const QString arch = QSysInfo::currentCpuArchitecture();
			return arch == QLatin1String("arm64") ? arch : QStringLiteral("x86_64");
		}

		/// The last @p maxBytes of @p path, as text.
		QString readTail(const QString& path, qsizetype maxBytes)
		{
			QFile file(path);
			if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
				return QString();
			}
			if (file.size() > maxBytes) {
				file.seek(file.size() - maxBytes);
			}
			return QString::fromUtf8(file.readAll());
		}

		/// Enabled jars of @p modsDir, read like the mods tab reads them.
		void scanMods(const QString& modsDir, const QSet<QString>& performanceFiles,
					  Report& report, Facts& facts)
		{
			const QFileInfoList jars =
				QDir(modsDir).entryInfoList({QStringLiteral("*.jar")}, QDir::Files);
			for (const QFileInfo& jar : jars) {
				LocalModParseTask task(0, Mod::MOD_ZIPFILE, jar);
				task.run();
				const auto result = task.result();
				if (!result || !result->details) {
					continue;
				}
				const ModDetails& details = *result->details;
				QStringList ids{details.mod_id};
				for (const ProvidedMod& provided : details.provides) {
					ids << provided.id;
				}
				if (!details.mod_id.isEmpty() && !details.name.isEmpty()) {
					report.modNames.insert(details.mod_id.toLower(), details.name);
				}
				if (!details.mod_id.isEmpty()) {
					report.mods.append({details.mod_id.toLower(), details.version});
				}
				const bool fromPerformance = performanceFiles.contains(
					ModMetadataIndex::canonicalFileName(jar.fileName()));
				for (const QString& raw : ids) {
					const QString id = raw.toLower();
					if (id.isEmpty()) {
						continue;
					}
					facts.installed.insert(id);
					report.modFiles[id].append({jar.absoluteFilePath(), details.version});
					if (fromPerformance) {
						facts.performanceMods.insert(id);
					}
				}
			}
		}
		/// The user's home folder and names: the system user's and every
		/// Minecraft account's.
		Personal personalNow()
		{
			QStringList names{qEnvironmentVariable("USER"), qEnvironmentVariable("USERNAME"),
							  QDir::home().dirName()};
			if (auto* context = LAUNCHER; context && context->accounts()) {
				names << context->accounts()->profileNames();
			}
			return Personal({QDir::homePath()}, names);
		}
	} // namespace

	std::optional<QPair<int, QString>> exitFromLog(const QString& log)
	{
		static const QRegularExpression kExit(QStringLiteral(
			"^Process (exited with code|crashed with exitcode) (-?\\d+)\\.$"),
			QRegularExpression::MultilineOption);
		std::optional<QPair<int, QString>> last;
		auto it = kExit.globalMatch(log);
		while (it.hasNext()) {
			const auto match = it.next();
			last = qMakePair(match.captured(2).toInt(),
							 match.captured(1).startsWith(QLatin1String("crashed"))
								 ? QStringLiteral("crash")
								 : QStringLiteral("normal"));
		}
		return last;
	}

	int javaFromLog(const QString& log)
	{
		static const QRegularExpression kJava(
			QStringLiteral("^Java is version ([\\w.+-]+),"),
			QRegularExpression::MultilineOption);
		const auto match = kJava.match(log);
		return match.hasMatch() ? JavaVersion(match.captured(1)).major() : 0;
	}

	Snapshot snapshot(MinecraftInstance& instance, LaunchTask* task)
	{
		Snapshot out;
		auto settings = instance.settings();
		out.instanceId = instance.id();
		out.gameRoot = instance.gameRoot();
		out.modsDir = instance.modsRoot();
		if (task) {
			out.since = task->startedAt();
			out.liveLog = task->getLogModel()->toPlainText();
		}

		Facts& facts = out.facts;
		facts.minecraft = instance.minecraftVersion();
		auto profile = instance.getPackProfile();
		const QString loader = profile ? profile->primaryModLoader() : QString();
		facts.loader = loader.isEmpty() ? kVanilla : loader;
		if (profile && kLoaderComponents.contains(facts.loader)) {
			out.loaderVersion = profile->getComponentVersion(kLoaderComponents.value(facts.loader));
		}
		out.personal = personalNow();
		const QJsonObject lastFix =
			QJsonDocument::fromJson(settings->get("CrashDoctorLastFix").toString().toUtf8()).object();
		out.lastFixRule = lastFix.value("rule").toString();
		out.lastFixAt = QDateTime::fromString(lastFix.value("at").toString(), Qt::ISODate);
		facts.os = osName();
		facts.arch = archName();
		facts.javaNow = JavaVersion(settings->get("JavaVersion").toString()).major();
		facts.javaRecommended = VerifyJavaInstall::requiredJavaMajor(instance);
		facts.memoryMb = settings->get("MaxMemAlloc").toInt();
		facts.systemMemoryMb = int(Sys::getSystemRam() / Sys::mebibyte);
		facts.customJvmArgs = !settings->get("JvmArgs").toString().trimmed().isEmpty();

		if (settings->get("KitePerformance").toBool() && facts.loader != kVanilla) {
			if (auto folder = instance.loaderModList()) {
				BundleCatalog catalog;
				catalog.loadCached();
				QStringList projects;
				for (const QVariant& mod : catalog.mods(kPerformanceBundles, facts.loader)) {
					projects.append(mod.toMap().value("project").toString());
				}
				const auto files =
					PerformanceSetup::filesToRematch(folder->metadataIndex()->all(), projects);
				for (const QString& file : files) {
					out.performanceFiles.insert(file);
				}
			}
		}
		return out;
	}

	Report analyse(const Catalog& catalog, const Snapshot& snapshot,
				   const QString& language)
	{
		Report report;
		report.instanceId = snapshot.instanceId;
		report.gameRoot = snapshot.gameRoot;
		report.modsDir = snapshot.modsDir;
		report.analysedAt = QDateTime::currentDateTime();

		report.launchedAt = snapshot.since;
		report.loaderVersion = snapshot.loaderVersion;

		Evidence evidence;
		QDateTime since = snapshot.since;
		const QString latest = QDir(snapshot.gameRoot).filePath(kLatestLog);
		if (!snapshot.files.isEmpty()) {
			evidence.log = readTail(snapshot.files.log, kMaxLogBytes);
			report.crashReportPath = snapshot.files.crashReport;
			report.jvmErrorPath = snapshot.files.jvmError;
		} else if (!snapshot.liveLog.isEmpty()) {
			evidence.log = snapshot.liveLog.right(kMaxLogBytes);
		} else {
			evidence.log = readTail(latest, kMaxLogBytes);
			if (!since.isValid()) {
				since = QFileInfo(latest).lastModified().addSecs(-kRunWindowSecs);
			}
		}
		if (snapshot.files.isEmpty() && since.isValid()) {
			report.crashReportPath = CrashFiles::newestCrashReport(snapshot.gameRoot, since);
			report.jvmErrorPath = CrashFiles::newestJvmErrorReport(snapshot.gameRoot, since);
		}
		report.files = {report.crashReportPath, report.jvmErrorPath,
						snapshot.files.isEmpty() ? latest : snapshot.files.log};
		evidence.crashReport = readTail(report.crashReportPath, kMaxCrashReportBytes);
		evidence.jvmError = readTail(report.jvmErrorPath, kMaxJvmErrorBytes);

		Facts facts = snapshot.facts;
		if (const auto exit = exitFromLog(evidence.log)) {
			facts.exitCode = exit->first;
			facts.exitStatus = exit->second;
			report.exitCode = exit->first;
		}
		if (const int logged = javaFromLog(evidence.log)) {
			facts.javaNow = logged;
		}
		scanMods(snapshot.modsDir, snapshot.performanceFiles, report, facts);

		report.causes = CrashDoctor::analyse(catalog, evidence, facts, language);
		report.facts = facts;
		report.fixDidNotHelp =
			snapshot.since.isValid() && snapshot.lastFixAt.isValid() && snapshot.lastFixAt < snapshot.since &&
			std::any_of(report.causes.begin(), report.causes.end(),
						[&snapshot](const Cause& cause) { return cause.ruleId == snapshot.lastFixRule; });
		std::sort(report.mods.begin(), report.mods.end(),
				  [](const ModEntry& a, const ModEntry& b) { return a.id < b.id; });
		report.lines = reportLines(evidence, snapshot.personal, catalog.classHints, facts.installed);
		if (!report.lines.lines.isEmpty()) {
			report.signature = signatureOf(report.lines.lines);
		}
		return report;
	}
} // namespace CrashDoctor
