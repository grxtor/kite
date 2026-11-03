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

#include "CrashFixer.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

#include "Sys.h"
#include "java/GpuPreference.h"
#include "minecraft/GraphicsBackend.h"
#include "minecraft/mod/VersionRange.h"
#include "settings/SettingsObject.h"

namespace CrashDoctor
{
	namespace
	{
		const QString kDisabledSuffix = QStringLiteral(".disabled");
		const QString kJarSuffix = QStringLiteral(".jar");
		const QString kConfigFolder = QStringLiteral("config");
		const QString kOptionsFile = QStringLiteral("options.txt");
		const QString kShadersKey = QStringLiteral("enableShaders");
		const QStringList kShaderConfigs{QStringLiteral("iris.properties"),
										 QStringLiteral("oculus.properties")};
		const QString kBackupStamp = QStringLiteral("yyyyMMdd-HHmmss");

		/* lupdate files these strings under the namespace's own tr() context. */
		QString tr(const char* text)
		{
			return QCoreApplication::translate("CrashDoctor", text);
		}

		/// Whether @p path is inside @p folder once links are resolved.
		bool inside(const QString& path, const QString& folder)
		{
			const QString file = QFileInfo(path).canonicalFilePath();
			const QString root = QDir(folder).canonicalPath();
			return !file.isEmpty() && !root.isEmpty() &&
				   file.startsWith(root + QLatin1Char('/'));
		}

		/// The enabled files @p ref names: a jar of the mods folder, or every
		/// file that is or provides mod id @p ref.
		QStringList filesFor(const QString& ref, const Report& report, const QString& modsDir)
		{
			QStringList paths;
			if (ref.endsWith(kJarSuffix)) {
				const QString path = QDir(modsDir).filePath(ref);
				if (QFileInfo(path).isFile() && inside(path, modsDir)) {
					paths << path;
				}
				return paths;
			}
			for (const ModFile& file : report.modFiles.value(ref.toLower())) {
				if (QFileInfo(file.path).isFile() && inside(file.path, modsDir) &&
					!paths.contains(file.path)) {
					paths << file.path;
				}
			}
			return paths;
		}

		/// The copies of @p mod to turn off: every one but the newest version
		/// (the newest file when versions do not compare).
		QStringList duplicatesToDisable(const QString& mod, const Report& report,
										const QString& modsDir)
		{
			QList<ModFile> files;
			for (const ModFile& file : report.modFiles.value(mod.toLower())) {
				if (QFileInfo(file.path).isFile() && inside(file.path, modsDir) &&
					std::none_of(files.begin(), files.end(),
								 [&](const ModFile& f) { return f.path == file.path; })) {
					files.append(file);
				}
			}
			if (files.size() < 2) {
				return {};
			}
			std::sort(files.begin(), files.end(), [](const ModFile& a, const ModFile& b) {
				const auto order = VersionRange::compare(a.version, b.version);
				if (order && *order != 0) {
					return *order > 0;
				}
				return QFileInfo(a.path).lastModified() > QFileInfo(b.path).lastModified();
			});
			QStringList extra;
			for (qsizetype i = 1; i < files.size(); ++i) {
				extra << files.at(i).path;
			}
			return extra;
		}

		QStringList stringsOf(const QVariant& value)
		{
			return value.typeId() == QMetaType::QStringList ? value.toStringList()
															 : QStringList{value.toString()};
		}

		QString configPath(const QString& gameRoot, const QString& file)
		{
			return QDir(QDir(gameRoot).filePath(kConfigFolder)).filePath(file);
		}

		/// Records @p ids' current values, then sets them; gates ("Override…")
		/// last, so the values land on the instance.
		void setAll(SettingsObject& settings, const QList<QPair<QString, QVariant>>& values,
					QVariantMap& old)
		{
			for (const auto& [id, value] : values) {
				if (!old.contains(id)) {
					old.insert(id, settings.get(id));
				}
			}
			for (const auto& [id, value] : values) {
				if (!id.startsWith(QLatin1String("Override"))) {
					settings.set(id, value);
				}
			}
			for (const auto& [id, value] : values) {
				if (id.startsWith(QLatin1String("Override"))) {
					settings.set(id, value);
				}
			}
		}

		bool renameAside(const QString& from, const QString& to, Undo& undo, QString* error)
		{
			if (QFileInfo::exists(to) || !QFile::rename(from, to)) {
				*error = tr("Could not rename %1.").arg(QFileInfo(from).fileName());
				return false;
			}
			undo.renames.append({to, from});
			return true;
		}

		bool writeFile(const QString& path, const QByteArray& content)
		{
			QFile file(path);
			return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
				   file.write(content) == content.size();
		}

		/// @p content with key=false, other lines kept.
		QByteArray withShadersOff(const QByteArray& content)
		{
			QList<QByteArray> lines = content.split('\n');
			bool found = false;
			for (QByteArray& line : lines) {
				if (line.trimmed().startsWith(kShadersKey.toUtf8() + '=')) {
					line = kShadersKey.toUtf8() + "=false";
					found = true;
				}
			}
			if (!found) {
				if (!lines.isEmpty() && lines.last().isEmpty()) {
					lines.removeLast();
				}
				lines.append(kShadersKey.toUtf8() + "=false");
				lines.append(QByteArray());
			}
			return lines.join('\n');
		}

		int clampMemory(int wanted)
		{
			const int ram = int(Sys::getSystemRam() / Sys::mebibyte);
			const int most = std::max(kMinMemoryMb, ram - kKeepForSystemMb);
			return std::clamp(wanted, kMinMemoryMb, most);
		}
	} // namespace

	QString backupPath(const QString& path, const QDateTime& when)
	{
		return path + QStringLiteral(".kite-backup-") + when.toString(kBackupStamp);
	}

	QStringList filesTouched(const ResolvedFix& fix, const Report& report)
	{
		const QString& modsDir = report.modsDir;
		QStringList names;
		const auto add = [&names](const QStringList& paths) {
			for (const QString& path : paths) {
				names << QFileInfo(path).fileName();
			}
		};
		if (fix.action == QLatin1String("disableMod")) {
			for (const QString& ref : stringsOf(fix.params.value("target"))) {
				add(filesFor(ref, report, modsDir));
			}
		} else if (fix.action == QLatin1String("removeDuplicateMod")) {
			add(duplicatesToDisable(fix.params.value("mod").toString(), report, modsDir));
		} else if (fix.action == QLatin1String("resetConfigFile")) {
			names << fix.params.value("file").toString();
		} else if (fix.action == QLatin1String("resetOptions")) {
			names << kOptionsFile;
		}
		return names;
	}

	std::optional<Undo> applyFix(const Target& target, const ResolvedFix& fix,
								 const Report& report, QString* error)
	{
		Undo undo;
		undo.action = fix.action;
		SettingsObject* settings = &target.settings;
		SettingsObject* launcherSettings = target.launcherSettings;
		const QString& action = fix.action;
		const QString& gameRoot = target.gameRoot;
		const QString& modsDir = target.modsDir;
		QString ignored;
		QString* why = error ? error : &ignored;

		if (action == QLatin1String("useJavaForInstance")) {
			setAll(*settings, {{"JavaMajorAtLeast", fix.params.value("major").toInt()},
							   {"JavaRepick", true}},
				   undo.settings);
		} else if (action == QLatin1String("useRecommendedJava")) {
			setAll(*settings, {{"JavaMajorAtLeast", 0}, {"JavaRepick", true},
							   {"OverrideJavaLocation", false}},
				   undo.settings);
		} else if (action == QLatin1String("useMatchingArchJava")) {
			setAll(*settings, {{"JavaRepick", true}, {"OverrideJavaLocation", false}},
				   undo.settings);
		} else if (action == QLatin1String("installJava")) {
			// Downloads are a launcher-wide choice; this lets this instance
			// download, with the vendor the launcher uses now.
			const QVariant vendor = settings->get("JavaAutoDownloadVendor");
			setAll(*settings, {{"JavaAutoDownload", true}, {"JavaAutoDownloadVendor", vendor},
							   {"JavaRepick", true}, {"OverrideJavaAutoDownloadVendor", true}},
				   undo.settings);
		} else if (action == QLatin1String("setMemory")) {
			const int max = clampMemory(fix.params.value("maxMb").toInt());
			const int min = std::min(settings->get("MinMemAlloc").toInt(), max);
			setAll(*settings, {{"MaxMemAlloc", max}, {"MinMemAlloc", min}, {"OverrideMemory", true}},
				   undo.settings);
		} else if (action == QLatin1String("clearCustomJvmArgs")) {
			setAll(*settings, {{"JvmArgs", QString()}, {"OverrideJavaArgs", true}}, undo.settings);
		} else if (action == QLatin1String("setMacFirstThread")) {
			setAll(*settings, {{"MacStartOnFirstThread", true}}, undo.settings);
		} else if (action == QLatin1String("turnOffKiteMod")) {
			setAll(*settings, {{"KiteMod", false}}, undo.settings);
		} else if (action == QLatin1String("repairGameFiles")) {
			setAll(*settings, {{"RepairGameFiles", true}}, undo.settings);
		} else if (action == QLatin1String("useDedicatedGpu")) {
			if (!launcherSettings) {
				*why = tr("The launcher settings are not available.");
				return std::nullopt;
			}
			undo.launcherSettings.insert(GpuPreference::kSetting,
										 launcherSettings->get(GpuPreference::kSetting));
			launcherSettings->set(GpuPreference::kSetting, true);
		} else if (action == QLatin1String("setGraphicsBackend")) {
			undo.graphicsBackend = GraphicsBackend::read(gameRoot);
			if (!GraphicsBackend::write(gameRoot, fix.params.value("backend").toString())) {
				*why = tr("Could not change options.txt.");
				return std::nullopt;
			}
		} else if (action == QLatin1String("disableMod") ||
				   action == QLatin1String("removeDuplicateMod")) {
			QStringList paths;
			if (action == QLatin1String("disableMod")) {
				for (const QString& ref : stringsOf(fix.params.value("target"))) {
					paths << filesFor(ref, report, modsDir);
				}
			} else {
				paths = duplicatesToDisable(fix.params.value("mod").toString(), report, modsDir);
			}
			paths.removeDuplicates();
			if (paths.isEmpty()) {
				*why = tr("The mod is no longer in the mods folder.");
				return std::nullopt;
			}
			for (const QString& path : paths) {
				if (!renameAside(path, path + kDisabledSuffix, undo, why)) {
					undoFix(target, undo, nullptr);
					return std::nullopt;
				}
			}
		} else if (action == QLatin1String("resetConfigFile") ||
				   action == QLatin1String("resetOptions")) {
			const QString path = action == QLatin1String("resetOptions")
									 ? QDir(gameRoot).filePath(kOptionsFile)
									 : configPath(gameRoot, fix.params.value("file").toString());
			if (!QFileInfo(path).isFile() || !inside(path, gameRoot)) {
				*why = tr("The file is not there any more.");
				return std::nullopt;
			}
			if (!renameAside(path, backupPath(path, QDateTime::currentDateTime()), undo, why)) {
				return std::nullopt;
			}
		} else if (action == QLatin1String("turnOffShaders")) {
			QStringList targets;
			for (const QString& name : kShaderConfigs) {
				if (QFileInfo(configPath(gameRoot, name)).isFile()) {
					targets << configPath(gameRoot, name);
				}
			}
			if (targets.isEmpty()) {
				QDir().mkpath(QDir(gameRoot).filePath(kConfigFolder));
				targets << configPath(gameRoot, kShaderConfigs.first());
			}
			for (const QString& path : targets) {
				QFile file(path);
				const bool existed = file.open(QIODevice::ReadOnly);
				const QByteArray read = existed ? file.readAll() : QByteArray();
				file.close();
				// A null array means "was not there"; an empty file stays empty.
				const QByteArray old = existed && read.isNull() ? QByteArray("") : read;
				undo.contents.insert(path, old);
				if (!writeFile(path, withShadersOff(old))) {
					*why = tr("Could not change %1.").arg(QFileInfo(path).fileName());
					undoFix(target, undo, nullptr);
					return std::nullopt;
				}
			}
		} else {
			*why = tr("This fix is not one Kite can do here.");
			return std::nullopt;
		}
		settings->set("CrashDoctorUndo", undo.toJson());
		return undo;
	}

	bool undoFix(const Target& target, const Undo& undo, QString* error)
	{
		bool ok = true;
		SettingsObject* settings = &target.settings;
		SettingsObject* launcherSettings = target.launcherSettings;
		QStringList ids = undo.settings.keys();
		std::stable_partition(ids.begin(), ids.end(), [](const QString& id) {
			return !id.startsWith(QLatin1String("Override"));
		});
		for (const QString& id : ids) {
			settings->set(id, undo.settings.value(id));
		}
		if (launcherSettings) {
			for (auto it = undo.launcherSettings.begin(); it != undo.launcherSettings.end(); ++it) {
				launcherSettings->set(it.key(), it.value());
			}
		}
		if (!undo.graphicsBackend.isEmpty()) {
			ok = GraphicsBackend::write(target.gameRoot, undo.graphicsBackend) && ok;
		}
		for (const auto& [now, was] : undo.renames) {
			if (QFileInfo::exists(was) || !QFile::rename(now, was)) {
				ok = false;
				if (error) {
					*error = tr("%1 could not be put back.").arg(QFileInfo(was).fileName());
				}
			}
		}
		for (auto it = undo.contents.begin(); it != undo.contents.end(); ++it) {
			const bool restored = it.value().isNull() ? QFile::remove(it.key())
													  : writeFile(it.key(), it.value());
			ok = restored && ok;
		}
		settings->set("CrashDoctorUndo", QString());
		return ok;
	}

	QString Undo::toJson() const
	{
		QJsonObject root;
		root.insert("action", action);
		root.insert("settings", QJsonObject::fromVariantMap(settings));
		root.insert("launcherSettings", QJsonObject::fromVariantMap(launcherSettings));
		QJsonArray moved;
		for (const auto& [now, was] : renames) {
			moved.append(QJsonArray{now, was});
		}
		root.insert("renames", moved);
		QJsonObject files;
		for (auto it = contents.begin(); it != contents.end(); ++it) {
			files.insert(it.key(), it.value().isNull()
									   ? QJsonValue(QJsonValue::Null)
									   : QJsonValue(QString::fromLatin1(it.value().toBase64())));
		}
		root.insert("contents", files);
		root.insert("graphicsBackend", graphicsBackend);
		return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
	}

	std::optional<Undo> Undo::fromJson(const QString& json)
	{
		const QJsonObject root = QJsonDocument::fromJson(json.toUtf8()).object();
		if (root.value("action").toString().isEmpty()) {
			return std::nullopt;
		}
		Undo undo;
		undo.action = root.value("action").toString();
		undo.settings = root.value("settings").toObject().toVariantMap();
		undo.launcherSettings = root.value("launcherSettings").toObject().toVariantMap();
		for (const QJsonValue& pair : root.value("renames").toArray()) {
			const QJsonArray items = pair.toArray();
			undo.renames.append({items.at(0).toString(), items.at(1).toString()});
		}
		const QJsonObject files = root.value("contents").toObject();
		for (auto it = files.begin(); it != files.end(); ++it) {
			undo.contents.insert(it.key(), it.value().isNull()
											   ? QByteArray()
											   : QByteArray::fromBase64(it.value().toString().toLatin1()));
		}
		undo.graphicsBackend = root.value("graphicsBackend").toString();
		return undo;
	}
} // namespace CrashDoctor
