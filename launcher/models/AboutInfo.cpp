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

#include "models/AboutInfo.h"

#include <QDebug>
#include <QFile>
#include <QVariantMap>

#include "BuildConfig.h"

namespace
{
	QString readDocument(const QString& path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			qWarning() << "AboutInfo: could not read" << path << "-"
					   << file.errorString();
			return QString();
		}
		return QString::fromUtf8(file.readAll());
	}
} // namespace

AboutInfo::AboutInfo(QObject* parent) : QObject(parent) {}

QString AboutInfo::name() const
{
	return BuildConfig.MESHMC_DISPLAYNAME;
}

QString AboutInfo::version() const
{
	return BuildConfig.printableVersionString();
}

QString AboutInfo::platform() const
{
	return BuildConfig.BUILD_PLATFORM;
}

QString AboutInfo::buildDate() const
{
	return BuildConfig.BUILD_DATE;
}

QString AboutInfo::commit() const
{
	return commitFor(BuildConfig.GIT_COMMIT);
}

QString AboutInfo::channel() const
{
	return BuildConfig.VERSION_CHANNEL;
}

QString AboutInfo::updateChannel() const
{
	return BuildConfig.UPDATE_CHANNEL;
}

QString AboutInfo::sourceUrl() const
{
	return BuildConfig.MESHMC_GIT;
}

QString AboutInfo::copyright() const
{
	return BuildConfig.MESHMC_COPYRIGHT;
}

QVariantList AboutInfo::helpLinks() const
{
	return helpLinksFor(BuildConfig.BUG_TRACKER_URL, BuildConfig.DISCORD_URL,
						BuildConfig.SUBREDDIT_URL, BuildConfig.PATREON_URL);
}

QString AboutInfo::creditsHtml() const
{
	// Same placeholders AboutDialog fills in.
	const QString html = readDocument(QStringLiteral(":/documents/credits.html"));
	if (html.isEmpty()) {
		return html;
	}
	return html.arg(tr("%1 Developers").arg(BuildConfig.MESHMC_DISPLAYNAME),
					tr("MultiMC Developers"));
}

QString AboutInfo::licenseText() const
{
	return readDocument(QStringLiteral(":/documents/COPYING.md"));
}

QVariantList AboutInfo::helpLinksFor(const QString& bugTrackerUrl,
									 const QString& discordUrl,
									 const QString& subredditUrl,
									 const QString& patreonUrl)
{
	const struct {
		const char* id;
		QString label;
		const QString& url;
	} candidates[] = {
		{ "bug", tr("Report a bug"), bugTrackerUrl },
		{ "discord", tr("Discord"), discordUrl },
		{ "reddit", tr("Reddit"), subredditUrl },
		{ "patreon", tr("Support on Patreon"), patreonUrl },
	};

	QVariantList links;
	for (const auto& candidate : candidates) {
		if (candidate.url.trimmed().isEmpty()) {
			continue;
		}
		links.append(QVariantMap{
			{ QStringLiteral("id"), QString::fromLatin1(candidate.id) },
			{ QStringLiteral("label"), candidate.label },
			{ QStringLiteral("url"), candidate.url.trimmed() },
		});
	}
	return links;
}

QString AboutInfo::commitFor(const QString& gitCommit)
{
	if (gitCommit.isEmpty() || gitCommit == QLatin1String("GITDIR-NOTFOUND")) {
		return QString();
	}
	return gitCommit;
}
