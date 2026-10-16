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

#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

/*
 * What the QML About section shows -- the facts the widget AboutDialog
 * puts on screen (version, platform, build date, commit, channels, source
 * link, copyright, credits, licences) plus the Help links the main
 * window's Help menu offers (report a bug, Discord, Reddit, Patreon).
 * Everything comes from BuildConfig and the bundled documents
 * (:/documents/credits.html, :/documents/COPYING.md), so nothing here
 * changes while the launcher runs.
 */
class AboutInfo : public QObject
{
	Q_OBJECT

	Q_PROPERTY(QString name READ name CONSTANT)
	/// BuildConfig::printableVersionString().
	Q_PROPERTY(QString version READ version CONSTANT)
	/// Empty when the build does not say.
	Q_PROPERTY(QString platform READ platform CONSTANT)
	Q_PROPERTY(QString buildDate READ buildDate CONSTANT)
	/// Empty when the build had no git checkout to read it from.
	Q_PROPERTY(QString commit READ commit CONSTANT)
	Q_PROPERTY(QString channel READ channel CONSTANT)
	Q_PROPERTY(QString updateChannel READ updateChannel CONSTANT)
	Q_PROPERTY(QString sourceUrl READ sourceUrl CONSTANT)
	Q_PROPERTY(QString copyright READ copyright CONSTANT)
	/// [{id, label, url}] -- see helpLinksFor().
	Q_PROPERTY(QVariantList helpLinks READ helpLinks CONSTANT)
	/// The credits, as HTML; empty if the document is missing.
	Q_PROPERTY(QString creditsHtml READ creditsHtml CONSTANT)
	/// The licences (COPYING.md), as Markdown; empty if missing.
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

	/* The Help links with a URL, in the main window's Help menu order:
	 * report a bug, Discord, Reddit, then Patreon. A build that leaves one
	 * of the URLs empty simply has no such link. Static, from plain
	 * strings, so it can be tested without a particular BuildConfig. */
	static QVariantList helpLinksFor(const QString& bugTrackerUrl,
									 const QString& discordUrl,
									 const QString& subredditUrl,
									 const QString& patreonUrl);
	/* @p gitCommit, or empty for the placeholder CMake writes when the
	 * build had no git checkout -- the same rule AboutDialog applies. */
	static QString commitFor(const QString& gitCommit);
};
