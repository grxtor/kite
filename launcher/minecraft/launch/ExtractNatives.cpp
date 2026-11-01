/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include "ExtractNatives.h"
#include <minecraft/MinecraftInstance.h>
#include <launch/LaunchTask.h>

#include "MMCZip.h"
#include "FileSystem.h"
#include <QDir>

#ifdef major
#undef major
#endif
#ifdef minor
#undef minor
#endif

static QString replaceSuffix(QString target, const QString& suffix,
							 const QString& replacement)
{
	if (!target.endsWith(suffix)) {
		return target;
	}
	target.resize(target.length() - suffix.length());
	return target + replacement;
}

static bool unzipNatives(QString source, QString targetFolder,
						 bool applyJnilibHack, bool nativeOpenAL,
						 bool nativeGLFW, QString* error)
{
	QDir directory(targetFolder);
	QStringList entries = MMCZip::listEntries(source);
	if (entries.isEmpty()) {
		if (error) {
			*error = QStringLiteral("archive is empty or unreadable");
		}
		return false;
	}
	for (const auto& name : entries) {
		if (nativeGLFW && name.contains("glfw")) {
			continue;
		}
		if (nativeOpenAL && name.contains("openal")) {
			continue;
		}
		// Skip directories
		if (name.endsWith('/'))
			continue;

		QString outName = name;
		if (applyJnilibHack) {
			outName = replaceSuffix(outName, ".jnilib", ".dylib");
		}
		QString absFilePath = directory.absoluteFilePath(outName);
		if (!MMCZip::extractRelFile(source, name, absFilePath, error)) {
			return false;
		}
	}
	return true;
}

void ExtractNatives::executeTask()
{
	auto instance = m_parent->instance();
	std::shared_ptr<MinecraftInstance> minecraftInstance =
		std::dynamic_pointer_cast<MinecraftInstance>(instance);
	auto toExtract = minecraftInstance->getNativeJars();
	if (toExtract.isEmpty()) {
		emitSucceeded();
		return;
	}
	auto settings = minecraftInstance->settings();
	bool nativeOpenAL = settings->get("UseNativeOpenAL").toBool();
	bool nativeGLFW = settings->get("UseNativeGLFW").toBool();

	auto outputPath = minecraftInstance->getNativePath();
	auto javaVersion = minecraftInstance->getJavaVersion();
	bool jniHackEnabled = javaVersion.major() >= 8;
	for (const auto& source : toExtract) {
		QString error;
		if (!unzipNatives(source, outputPath, jniHackEnabled, nativeOpenAL,
						  nativeGLFW, &error)) {
			const char* reason = QT_TR_NOOP(
				"Couldn't extract native jar '%1' to destination '%2': %3");
			emit logLine(QString(reason).arg(source, outputPath, error),
						 MessageLevel::Fatal);
			// Must not fall through: reaching emitSucceeded() below made the
			// task report both failed and succeeded, so a launch with no usable
			// natives looked fine.
			emitFailed(tr(reason).arg(source, outputPath, error));
			return;
		}
	}
	emitSucceeded();
}

void ExtractNatives::finalize()
{
	auto instance = m_parent->instance();
	QString target_dir = FS::PathCombine(instance->instanceRoot(), "natives/");
	QDir dir(target_dir);
	dir.removeRecursively();
}
