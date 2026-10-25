/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include "CheckJava.h"
#include <launch/LaunchTask.h>
#include <FileSystem.h>
#include <QStandardPaths>
#include <QFileInfo>
#include "Sys.h"

void CheckJava::executeTask()
{
	setStatus(tr("Checking Java"));
	auto instance = m_parent->instance();
	auto settings = instance->settings();
	m_javaPath = FS::ResolveExecutable(settings->get("JavaPath").toString());
	bool perInstance = settings->get("OverrideJava").toBool() ||
					   settings->get("OverrideJavaLocation").toBool();

	auto realJavaPath = QStandardPaths::findExecutable(m_javaPath);
	if (realJavaPath.isEmpty()) {
		if (perInstance) {
			emit logLine(
				QString("The java binary \"%1\" couldn't be found. Please fix "
						"the java path "
						"override in the instance's settings or disable it.")
					.arg(m_javaPath),
				MessageLevel::Warning);
			emitFailed(QString("Java path is not valid."));
			return;
		}
		// No usable Java in the global settings is not an error: the
		// welcome screen offers "let Kite download the right version for
		// each game" instead of picking one. VerifyJavaInstall, later in
		// this launch, picks an installed Java that suits this Minecraft
		// version or downloads one, and records it on the instance.
		emit logLine(
			m_javaPath.isEmpty()
				? QString("No Java is set up. Kite will pick one for this "
						  "Minecraft version.\n\n")
				: QString("The java binary \"%1\" couldn't be found. Kite will "
						  "pick one for this Minecraft version.\n\n")
					  .arg(m_javaPath),
			MessageLevel::Kite);
		emitSucceeded();
		return;
	} else {
		emit logLine("Java path is:\n" + m_javaPath + "\n\n",
					 MessageLevel::Kite);
	}

	QFileInfo javaInfo(realJavaPath);
	qlonglong javaUnixTime = javaInfo.lastModified().toMSecsSinceEpoch();
	auto storedUnixTime = settings->get("JavaTimestamp").toLongLong();
	auto storedArchitecture = settings->get("JavaArchitecture").toString();
	auto storedVersion = settings->get("JavaVersion").toString();
	auto storedVendor = settings->get("JavaVendor").toString();
	auto storedRealArchitecture =
		settings->get("JavaRealArchitecture").toString();
	// Which Java the values above were recorded for. An instance with a Java
	// of its own that has no record yet reads the global one's -- reloading
	// settings even writes those into the instance -- and natives follow the
	// architecture, so values recorded for another Java are not trusted.
	auto storedCheckedPath = settings->get("JavaCheckedPath").toString();
	m_javaUnixTime = javaUnixTime;
	// if timestamps are not the same, or something is missing, check!
	if (javaUnixTime != storedUnixTime || storedVersion.size() == 0 ||
		storedArchitecture.size() == 0 || storedVendor.size() == 0 ||
		storedRealArchitecture.size() == 0 ||
		storedCheckedPath != realJavaPath) {
		m_JavaChecker = new JavaChecker();
		emit logLine(QString("Checking Java version..."), MessageLevel::Kite);
		connect(m_JavaChecker.get(), &JavaChecker::checkFinished, this,
				&CheckJava::checkJavaFinished);
		m_JavaChecker->m_path = realJavaPath;
		m_JavaChecker->performCheck();
		return;
	} else {
		auto verString = instance->settings()->get("JavaVersion").toString();
		auto archString =
			instance->settings()->get("JavaArchitecture").toString();
		auto vendorString = instance->settings()->get("JavaVendor").toString();
		printJavaInfo(verString, archString, vendorString);
	}
	emitSucceeded();
}

void CheckJava::checkJavaFinished(JavaCheckResult result)
{
	switch (result.validity) {
		case JavaCheckResult::Validity::Errored: {
			// Error message displayed if java can't start
			emit logLine(QString("Could not start java:"), MessageLevel::Error);
			emit logLines(result.errorLog.split('\n'), MessageLevel::Error);
			emit logLine("\nCheck your Kite Java settings.",
						 MessageLevel::Kite);
			printSystemInfo(false, false);
			emitFailed(QString("Could not start java!"));
			return;
		}
		case JavaCheckResult::Validity::ReturnedInvalidData: {
			emit logLine(QString("Java checker returned some invalid data "
								 "Kite doesn't understand:"),
						 MessageLevel::Error);
			emit logLines(result.outLog.split('\n'), MessageLevel::Warning);
			emit logLine("\nMinecraft might not start properly.",
						 MessageLevel::Kite);
			printSystemInfo(false, false);
			emitSucceeded();
			return;
		}
		case JavaCheckResult::Validity::Valid: {
			auto instance = m_parent->instance();
			printJavaInfo(result.javaVersion.toString(), result.mojangPlatform,
						  result.javaVendor);
			instance->settings()->set("JavaVersion",
									  result.javaVersion.toString());
			instance->settings()->set("JavaArchitecture",
									  result.mojangPlatform);
			instance->settings()->set("JavaRealArchitecture",
									  result.realPlatform);
			instance->settings()->set("JavaCheckedPath", result.path);
			instance->settings()->set("JavaVendor", result.javaVendor);
			instance->settings()->set("JavaTimestamp", m_javaUnixTime);
			emitSucceeded();
			return;
		}
	}
}

void CheckJava::printJavaInfo(const QString& version,
							  const QString& architecture,
							  const QString& vendor)
{
	emit logLine(
		QString("Java is version %1, using %2-bit architecture, from %3.\n\n")
			.arg(version, architecture, vendor),
		MessageLevel::Kite);
	printSystemInfo(true, architecture == "64");
}

void CheckJava::printSystemInfo(bool javaIsKnown, bool javaIs64bit)
{
	auto cpu64 = Sys::isCPU64bit();
	auto system64 = Sys::isSystem64bit();
	if (cpu64 != system64) {
		emit logLine(QString("Your CPU architecture is not matching your "
							 "system architecture. You might want to install a "
							 "64bit Operating System.\n\n"),
					 MessageLevel::Error);
	}
	if (javaIsKnown) {
		if (javaIs64bit != system64) {
			emit logLine(QString("Your Java architecture is not matching your "
								 "system architecture. You might want to "
								 "install a 64bit Java version.\n\n"),
						 MessageLevel::Error);
		}
	}
}
