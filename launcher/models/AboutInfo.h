/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

/* What the QML About section shows: version, platform, build date, commit,
 * channels, source link, copyright, credits, licences and the Help links (bug,
 * Discord, Reddit, Patreon). All from BuildConfig and the bundled documents, so
 * nothing changes while the launcher runs. */
class AboutInfo : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString name READ name CONSTANT)
	Q_PROPERTY(QString version READ version CONSTANT)
	Q_PROPERTY(QString platform READ platform CONSTANT)
	Q_PROPERTY(QString buildDate READ buildDate CONSTANT)
	/// Empty when the build had no git checkout to read it from.
	Q_PROPERTY(QString commit READ commit CONSTANT)
	Q_PROPERTY(QString channel READ channel CONSTANT)
	Q_PROPERTY(QString updateChannel READ updateChannel CONSTANT)
	Q_PROPERTY(QString sourceUrl READ sourceUrl CONSTANT)
	Q_PROPERTY(QString copyright READ copyright CONSTANT)
	Q_PROPERTY(QVariantList helpLinks READ helpLinks CONSTANT)
	Q_PROPERTY(QString creditsHtml READ creditsHtml CONSTANT)
	Q_PROPERTY(QString licenseText READ licenseText CONSTANT)

  public:
	explicit AboutInfo(QObject* parent = nullptr);

	QString name() const;
	QString version() const;
	QString platform() const;
	QString buildDate() const;
	QString commit() const;
	QString channel() const;
	QString updateChannel() const;
	QString sourceUrl() const;
	QString copyright() const;
	QVariantList helpLinks() const;
	QString creditsHtml() const;
	QString licenseText() const;

	/* The Help links with a URL, in the Help menu order; a build with an empty URL
	 * has no such link. Static and from plain strings, so testable. */
	static QVariantList helpLinksFor(const QString& bugTrackerUrl,
									 const QString& discordUrl,
									 const QString& subredditUrl,
									 const QString& patreonUrl);
	/* @p gitCommit, or empty for CMake's placeholder when there was no git checkout. */
	static QString commitFor(const QString& gitCommit);
};
