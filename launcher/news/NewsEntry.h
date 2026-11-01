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
	explicit NewsEntry(QObject* parent = 0);

	NewsEntry(const QString& title, const QString& content, const QString& link,
			  const QString& author, const QDateTime& pubDate,
			  QObject* parent = 0);

	/*! Loads the entry from an XML element; on failure returns false and
	* stores a message in errorMsg. */
	static bool fromXmlElement(const QDomElement& element, NewsEntry* entry,
							   QString* errorMsg = 0);

	QString title;

	//! May contain HTML.
	QString content;

	QString link;

	QString author;

	QDateTime pubDate;

	/*! Which NewsChecker feed this came from: 0 is the launcher's own,
	* higher are extra feeds. Set by NewsChecker, not fromXmlElement(). */
	int feedIndex = 0;
};

typedef std::shared_ptr<NewsEntry> NewsEntryPtr;
