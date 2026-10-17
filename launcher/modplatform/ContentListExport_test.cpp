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

#include "modplatform/ContentListExport.h"

using namespace ContentListExport;

/*
 * The Markdown list escapes only what changes meaning in place; dots,
 * dashes and parentheses in names and versions stay readable.
 */
class ContentListExportTest : public QObject
{
	Q_OBJECT

	static QString markdownLine(const Item& item, Fields fields = NoFields)
	{
		return render({item}, Format::Markdown, fields);
	}

  private slots:
	void versionsAndDashesStayPlain()
	{
		Item item;
		item.name = QStringLiteral("Entity Culling");
		item.version = QStringLiteral("1.7.2-fabric+mc1.21");
		QCOMPARE(markdownLine(item, Version),
				 QStringLiteral("- Entity Culling [1.7.2-fabric+mc1.21]"));
	}

	void brokenSyntaxIsEscaped()
	{
		Item item;
		item.name = QStringLiteral("A *bold* [name] <b> `x` \\ y");
		QCOMPARE(markdownLine(item),
				 QStringLiteral("- A \\*bold\\* \\[name\\] \\<b> \\`x\\` \\\\ y"));
	}

	void underscoresOnlyAtWordEdges()
	{
		Item item;
		item.name = QStringLiteral("mod_name_1 _lead trail_");
		QCOMPARE(markdownLine(item),
				 QStringLiteral("- mod_name_1 \\_lead trail\\_"));
	}

	void leadingNumberedListMarkerIsEscaped()
	{
		Item item;
		item.name = QStringLiteral("1. Intro");
		QCOMPARE(markdownLine(item), QStringLiteral("- 1\\. Intro"));

		// A version-like name is not a list marker.
		item.name = QStringLiteral("1.20.1 Pack");
		QCOMPARE(markdownLine(item), QStringLiteral("- 1.20.1 Pack"));
	}

	void linksKeepTheirAddress()
	{
		Item item;
		item.name = QStringLiteral("Sodium");
		item.url = QStringLiteral("https://modrinth.com/mod/sodium");
		QCOMPARE(markdownLine(item, Url),
				 QStringLiteral("- [Sodium](https://modrinth.com/mod/sodium)"));
	}
};

QTEST_GUILESS_MAIN(ContentListExportTest)
#include "ContentListExport_test.moc"
