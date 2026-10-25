/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
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

#pragma once

#include <launch/LaunchStep.h>
#include "java/JavaChecker.h"
#ifndef Kite_DISABLE_JAVA_DOWNLOADER
#include "java/download/JavaRuntime.h"
#include "java/download/JavaDownloadTask.h"
#endif
#include "net/NetJob.h"

class VerifyJavaInstall : public LaunchStep
{
	Q_OBJECT

  public:
	explicit VerifyJavaInstall(LaunchTask* parent) : LaunchStep(parent) {};
	~VerifyJavaInstall() override = default;

	void executeTask() override;
	bool canAbort() const override
	{
		return false;
	}

  private:
	int determineRequiredJavaMajor() const;
	struct InstalledJava
	{
		QString path;
		int major = 0;
		bool native = false; // runs without emulation
	};
	/// The best installed Java for @p requiredMajor. With @p preferNative
	/// off (the version has no natives for this architecture), only Javas
	/// that run under emulation are considered.
	InstalledJava findInstalledJava(int requiredMajor, bool preferNative) const;
	/// Whether this version has LWJGL natives for a Java of the architecture
	/// Kite was built for. Before LWJGL 3.3 the meta only has them where it
	/// adds a build of its own (arm64 Macs, ARM Linux); on Windows ARM such a
	/// version only runs on an x86_64 Java, under emulation.
	bool nativeJavaCanRun() const;
	QString javaInstallDir() const;
	void setJavaPathAndSucceed(const QString& javaPath);
	// Fails with @p reason, unless an installed Java newer than the one
	// needed was put aside to fall back on.
	void failOrUseFallback(const QString& reason);

	InstalledJava m_fallbackJava;

	// Checks the Java this step picked, so its version, architecture and
	// vendor are recorded before native libraries and JVM arguments are
	// worked out from them.
	JavaCheckerPtr m_javaChecker;
#ifndef Kite_DISABLE_JAVA_DOWNLOADER
	void autoDownloadJava(int requiredMajor);
	void fetchVersionList(int requiredMajor);
	void fetchRuntimes(const QString& versionId, int requiredMajor);
	void startDownload(const JavaDownload::RuntimeEntry& runtime,
					   int requiredMajor);

	QString m_preferredVendor;
	NetJob::Ptr m_fetchJob;
	QByteArray m_fetchData;
	std::unique_ptr<JavaDownloadTask> m_downloadTask;
#endif
};
