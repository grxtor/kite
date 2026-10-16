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

#include <QTest>

#include "models/AboutInfo.h"

class AboutInfoTest : public QObject
{
	Q_OBJECT

  private slots:
	void helpLinksKeepTheMenuOrder()
	{
		const auto links = AboutInfo::helpLinksFor(
			QStringLiteral("https://bugs.example"),
			QStringLiteral("https://discord.example"),
			QStringLiteral("https://reddit.example"),
			QStringLiteral("https://patreon.example"));
		QCOMPARE(links.size(), 4);
		QCOMPARE(links.at(0).toMap().value("id").toString(), QStringLiteral("bug"));
		QCOMPARE(links.at(1).toMap().value("id").toString(), QStringLiteral("discord"));
		QCOMPARE(links.at(2).toMap().value("id").toString(), QStringLiteral("reddit"));
		QCOMPARE(links.at(3).toMap().value("id").toString(), QStringLiteral("patreon"));
		QCOMPARE(links.at(1).toMap().value("url").toString(),
				 QStringLiteral("https://discord.example"));
		QVERIFY(!links.at(0).toMap().value("label").toString().isEmpty());
	}

	// A build that leaves a URL empty (or blank) simply has no such link.
	void helpLinksSkipEmptyUrls()
	{
		const auto links = AboutInfo::helpLinksFor(
			QStringLiteral("https://bugs.example"), QString(),
			QStringLiteral("  "), QStringLiteral("https://patreon.example"));
		QCOMPARE(links.size(), 2);
		QCOMPARE(links.at(0).toMap().value("id").toString(), QStringLiteral("bug"));
		QCOMPARE(links.at(1).toMap().value("id").toString(), QStringLiteral("patreon"));
	}

	void commitHidesThePlaceholder()
	{
		QCOMPARE(AboutInfo::commitFor(QStringLiteral("GITDIR-NOTFOUND")), QString());
		QCOMPARE(AboutInfo::commitFor(QString()), QString());
		QCOMPARE(AboutInfo::commitFor(QStringLiteral("abc123")),
				 QStringLiteral("abc123"));
	}

	// Build facts come straight from BuildConfig.
	void versionIsTheBuildsOwn()
	{
		AboutInfo info;
		QVERIFY(!info.version().isEmpty());
		QVERIFY(!info.name().isEmpty());
	}
};

QTEST_GUILESS_MAIN(AboutInfoTest)

#include "AboutInfo_test.moc"
