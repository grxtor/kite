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

/*
 * NewsChecker — downloads and parses the launcher's RSS feed, plus any
 * extra feeds configured at build time (Kite_NEWS_EXTRA_FEEDS).
 *
 * Feed 0 is the launcher's own feed and is the one the news bar in the
 * main window speaks for: if it fails, the whole load is reported as
 * failed. Extra feeds are additive — one of them being unreachable
 * leaves the rest of the news perfectly usable, so it is logged and
 * otherwise ignored rather than blanking the news bar.
 *
 * getNewsEntries() returns every feed's entries merged and sorted
 * newest first, each tagged with the index of the feed it came from.
 */
class NewsChecker : public QObject
{
	Q_OBJECT
  public:
	/*!
	 * Constructs a news reader to read from the given RSS feed URL.
	 */
	NewsChecker(shared_qobject_ptr<QNetworkAccessManager> network,
				const QString& feedUrl);

	/*!
	 * Constructs a news reader over several feeds. The first URL is the
	 * primary feed; empty entries are dropped and duplicates collapsed.
	 */
	NewsChecker(shared_qobject_ptr<QNetworkAccessManager> network,
				const QStringList& feedUrls);

	/*!
	 * The feeds being watched, in the order their indices refer to.
	 */
	QStringList feedUrls() const;

	/*!
	 * Returns the error message for the last time the news was loaded.
	 * Empty string if the last load was successful.
	 */
	QString getLastLoadErrorMsg() const;

	/*!
	 * Returns true if the news has been loaded successfully.
	 */
	bool isNewsLoaded() const;

	//! True if the news is currently loading. If true, reloadNews() will do
	//! nothing.
	bool isLoadingNews() const;

	/*!
	 * Every feed's entries merged into one list, newest first. Each
	 * entry carries the index of the feed it came from.
	 */
	QList<NewsEntryPtr> getNewsEntries() const;

	/*!
	 * Reloads the news from the website's RSS feed.
	 * If the news is already loading, this does nothing.
	 */
	void Q_SLOT reloadNews();

  signals:
	/*!
	 * Signal fired after the news has finished loading.
	 */
	void newsLoaded();

	/*!
	 * Signal fired after the news fails to load.
	 */
	void newsLoadingFailed(QString errorMsg);

  protected slots:
	void rssDownloadFinished(int feedIndex);
	void rssDownloadFailed(int feedIndex, QString reason);

  protected: /* methods */
	/*!
	 * Called once per feed as its download settles, whichever way it
	 * went. Emits newsLoaded()/newsLoadingFailed() once the last
	 * outstanding feed has reported in — never before, so a caller
	 * never sees a half-populated list.
	 */
	void feedSettled();

  protected: /* data */
	/*! Everything one feed needs to be downloaded and parsed
	 *  independently of the others. */
	struct Feed {
		QString url;
		QByteArray data;
		QList<NewsEntryPtr> entries;
		NetJob::Ptr job;
	};

	QList<Feed> m_feeds;

	//! Feeds still being downloaded in the current reload.
	int m_pendingFeeds = 0;

	//! Set when the primary feed failed during the current reload.
	QString m_primaryError;

	//! True if news has been loaded.
	bool m_loadedNews = false;

	/*!
	 * Gets the error message that was given last time the news was loaded.
	 * If the last news load succeeded, this will be an empty string.
	 */
	QString m_lastLoadError;

	shared_qobject_ptr<QNetworkAccessManager> m_network;

  protected slots:
	/// Emits newsLoaded() and sets m_lastLoadError to empty string.
	void succeed();

	/// Emits newsLoadingFailed() and sets m_lastLoadError to the given message.
	void fail(const QString& errorMsg);
};
