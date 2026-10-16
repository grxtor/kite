/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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

#include "LauncherLogs.h"

#include <QRegularExpression>

#include "models/LogUploader.h"
#include "models/OtherLogsModel.h"
#include "pathmatcher/RegexpMatcher.h"

LauncherLogs::LauncherLogs(const QString& folder, const QString& namePrefix,
						   QObject* parent)
	: QObject(parent), m_folder(folder)
{
	m_logs = new OtherLogsModel(
		folder, std::make_shared<RegexpMatcher>(fileNamePattern(namePrefix)),
		this, /* recursive */ false);
	m_uploader = new LogUploader(this);
}

LauncherLogs::~LauncherLogs() = default;

QObject* LauncherLogs::logs() const
{
	return m_logs;
}

QObject* LauncherLogs::uploader() const
{
	return m_uploader;
}

QString LauncherLogs::fileNamePattern(const QString& namePrefix)
{
	return QStringLiteral("^%1-\\d+\\.log$")
		.arg(QRegularExpression::escape(namePrefix));
}
