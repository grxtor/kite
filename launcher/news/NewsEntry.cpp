/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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

#include "NewsEntry.h"

#include <QDomNodeList>
#include <QRegularExpression>
#include <QVariant>

NewsEntry::NewsEntry(QObject* parent) : QObject(parent)
{
	this->title = tr("Untitled");
	this->content = tr("No content.");
	this->link = "";
	this->author = tr("Unknown Author");
	this->pubDate = QDateTime::currentDateTime();
}

NewsEntry::NewsEntry(const QString& title, const QString& content,
					 const QString& link, const QString& author,
					 const QDateTime& pubDate, QObject* parent)
	: QObject(parent)
{
	this->title = title;
	this->content = content;
	this->link = link;
	this->author = author;
	this->pubDate = pubDate;
}

/*!
 * Gets the text content of the given child element as a QVariant.
 */
inline QString childValue(const QDomElement& element, const QString& childName,
						  QString defaultVal = "")
{
	QDomNodeList nodes = element.elementsByTagName(childName);
	if (nodes.count() > 0) {
		QDomElement childElement = nodes.at(0).toElement();
		return childElement.text();
	} else {
		return defaultVal;
	}
}

bool NewsEntry::fromXmlElement(const QDomElement& element, NewsEntry* entry,
							   QString* errorMsg)
{
	(void)errorMsg;
	QString title = childValue(element, "title", tr("Untitled"));
	QString content = childValue(element, "description", tr("No content."));
	QString link = childValue(element, "link");
	QString author = childValue(element, "dc:creator", tr("Unknown Author"));
	QString pubDateStr = childValue(element, "pubDate");

	/* RSS dates are RFC 2822 ("Wed, 25 Mar 2026 00:00:00 GMT"). The old
	 * zone-less format below never matched a date that names its zone, so
	 * every entry of such a feed came out with an invalid date. Qt's own
	 * RFC 2822 parser takes a numeric zone but not the obsolete names
	 * RFC 2822 still allows, so "GMT"/"UTC"/"UT"/"Z" become "+0000" first.
	 * The old format stays as a fallback for a feed that leaves the zone
	 * off. */
	QString dateText = pubDateStr.trimmed();
	static const QRegularExpression utcName(
		QStringLiteral("\\s+(GMT|UTC|UT|Z)$"),
		QRegularExpression::CaseInsensitiveOption);
	dateText.replace(utcName, QStringLiteral(" +0000"));
	QDateTime pubDate = QDateTime::fromString(dateText, Qt::RFC2822Date);
	if (!pubDate.isValid()) {
		QString dateFormat("ddd, dd MMM yyyy hh:mm:ss");
		pubDate = QDateTime::fromString(pubDateStr.trimmed(), dateFormat);
	}

	entry->title = title;
	entry->content = content;
	entry->link = link;
	entry->author = author;
	entry->pubDate = pubDate;
	return true;
}
