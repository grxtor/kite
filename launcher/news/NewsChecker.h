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

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>

#include <net/NetJob.h>

#include "NewsEntry.h"

/* Downloads and parses the launcher's RSS feed plus any extra feeds from
 * Kite_NEWS_EXTRA_FEEDS. Feed 0 is the launcher's own: if it fails the
 * whole load fails; an unreachable extra feed is logged and ignored.
 * getNewsEntries() merges every feed, newest first, tagged with its feed
 * index. */
class NewsChecker : public QObject
{
	Q_OBJECT
  public:
	NewsChecker(shared_qobject_ptr<QNetworkAccessManager> network,
				const QString& feedUrl);

	/*! Several feeds; the first URL is the primary one. Empty entries are
	 * dropped and duplicates collapsed. */
	NewsChecker(shared_qobject_ptr<QNetworkAccessManager> network,
				const QStringList& feedUrls);

	QStringList feedUrls() const;

	/*! The error message of the last load; empty if it succeeded. */
	QString getLastLoadErrorMsg() const;

	bool isNewsLoaded() const;

	//! True while loading; reloadNews() then does nothing.
	bool isLoadingNews() const;

	/*! Every feed's entries merged, newest first. */
	QList<NewsEntryPtr> getNewsEntries() const;

	/*! Reloads the feeds; does nothing while already loading. */
	void Q_SLOT reloadNews();

  signals:
	void newsLoaded();

	void newsLoadingFailed(QString errorMsg);

  protected slots:
	void rssDownloadFinished(int feedIndex);
	void rssDownloadFailed(int feedIndex, QString reason);

  protected: /* methods */
	/*! Called as each feed settles; emits newsLoaded()/newsLoadingFailed()
	* once, after the last one. */
	void feedSettled();

  protected: /* data */
	/*! One feed's download and parse state. */
	struct Feed {
		QString url;
		QByteArray data;
		QList<NewsEntryPtr> entries;
		NetJob::Ptr job;
	};

	QList<Feed> m_feeds;

	int m_pendingFeeds = 0;

	QString m_primaryError;

	bool m_loadedNews = false;

	QString m_lastLoadError;

	shared_qobject_ptr<QNetworkAccessManager> m_network;

  protected slots:
	void succeed();

	void fail(const QString& errorMsg);
};
