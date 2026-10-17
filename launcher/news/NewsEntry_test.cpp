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

#include <QDomDocument>
#include <QTest>
#include <QTimeZone>

#include "news/NewsEntry.h"

/* NewsEntry::fromXmlElement()'s RSS date: Home's news banner hides posts by
 * age, which only works once the feed's RFC 2822 dates parse. */
class NewsEntryTest : public QObject
{
	Q_OBJECT

	static QDateTime parsedDate(const QString& pubDate)
	{
		QDomDocument doc;
		const QString xml =
			QStringLiteral("<item><title>News</title>"
						   "<link>https://example.org/post</link>"
						   "<pubDate>%1</pubDate></item>")
				.arg(pubDate);
		if (!doc.setContent(xml)) {
			return QDateTime();
		}
		NewsEntry entry;
		NewsEntry::fromXmlElement(doc.documentElement(), &entry);
		return entry.pubDate;
	}

  private slots:
	void namedUtcZone_data()
	{
		QTest::addColumn<QString>("pubDate");
		QTest::newRow("GMT") << QStringLiteral("Wed, 25 Mar 2026 00:00:00 GMT");
		QTest::newRow("UTC") << QStringLiteral("Wed, 25 Mar 2026 00:00:00 UTC");
		QTest::newRow("lower case, padded")
			<< QStringLiteral("  Wed, 25 Mar 2026 00:00:00 gmt ");
	}
	void namedUtcZone()
	{
		QFETCH(QString, pubDate);
		const QDateTime date = parsedDate(pubDate);
		QVERIFY(date.isValid());
		QCOMPARE(date.toUTC(),
				 QDateTime(QDate(2026, 3, 25), QTime(0, 0), QTimeZone::UTC));
	}

	void numericZone()
	{
		const QDateTime date =
			parsedDate(QStringLiteral("Sat, 05 Sep 2026 10:00:00 +0200"));
		QVERIFY(date.isValid());
		QCOMPARE(date.toUTC(),
				 QDateTime(QDate(2026, 9, 5), QTime(8, 0), QTimeZone::UTC));
	}

	void noZoneStillParses()
	{
		const QDateTime date =
			parsedDate(QStringLiteral("Sat, 05 Sep 2026 10:00:00"));
		QVERIFY(date.isValid());
		QCOMPARE(date.date(), QDate(2026, 9, 5));
	}

	void garbageIsInvalid()
	{
		QVERIFY(!parsedDate(QStringLiteral("yesterday-ish")).isValid());
	}
};

QTEST_GUILESS_MAIN(NewsEntryTest)

#include "NewsEntry_test.moc"
