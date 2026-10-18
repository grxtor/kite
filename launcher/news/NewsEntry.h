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
#include <QDomElement>
#include <QDateTime>

#include <memory>

class NewsEntry : public QObject
{
	Q_OBJECT

  public:
	/*!
	 * Constructs an empty news entry.
	 */
	explicit NewsEntry(QObject* parent = 0);

	/*!
	 * Constructs a new news entry.
	 * Note that content may contain HTML.
	 */
	NewsEntry(const QString& title, const QString& content, const QString& link,
			  const QString& author, const QDateTime& pubDate,
			  QObject* parent = 0);

	/*!
	 * Attempts to load information from the given XML element into the given
	 * news entry pointer. If this fails, the function will return false and
	 * store an error message in the errorMsg pointer.
	 */
	static bool fromXmlElement(const QDomElement& element, NewsEntry* entry,
							   QString* errorMsg = 0);

	//! The post title.
	QString title;

	//! The post's content. May contain HTML.
	QString content;

	//! URL to the post.
	QString link;

	//! The post's author.
	QString author;

	//! The date and time that this post was published.
	QDateTime pubDate;

	/*!
	 * Which of NewsChecker's feeds this entry came from. 0 is the
	 * launcher's own feed; anything higher is an extra feed configured
	 * at build time via Kite_NEWS_EXTRA_FEEDS. Set by NewsChecker
	 * after parsing, not by fromXmlElement() — the XML says nothing
	 * about where it was downloaded from.
	 */
	int feedIndex = 0;
};

typedef std::shared_ptr<NewsEntry> NewsEntryPtr;
