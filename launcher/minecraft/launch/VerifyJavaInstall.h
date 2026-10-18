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
	QString findInstalledJava(int requiredMajor) const;
	QString javaInstallDir() const;
#ifndef Kite_DISABLE_JAVA_DOWNLOADER
	void autoDownloadJava(int requiredMajor);
	void fetchVersionList(int requiredMajor);
	void fetchRuntimes(const QString& versionId, int requiredMajor);
	void startDownload(const JavaDownload::RuntimeEntry& runtime,
					   int requiredMajor);
	void setJavaPathAndSucceed(const QString& javaPath);

	QString m_preferredVendor;
	NetJob::Ptr m_fetchJob;
	QByteArray m_fetchData;
	std::unique_ptr<JavaDownloadTask> m_downloadTask;
#endif
};
