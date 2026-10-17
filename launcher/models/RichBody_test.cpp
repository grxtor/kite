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

#include "models/RichBody.h"

namespace
{
const QString kFigure = QStringLiteral("<table class=\"figure\"");

QString markdown(const QString& body)
{
	return RichBody::toArticleHtml(body, RichBody::Format::Markdown);
}

QString html(const QString& body)
{
	return RichBody::toArticleHtml(body, RichBody::Format::Html);
}
} // namespace

class RichBodyTest : public QObject
{
	Q_OBJECT

  private slots:
	void emptyBodyIsEmpty()
	{
		QVERIFY(markdown(QStringLiteral("  \n ")).isEmpty());
		QVERIFY(html(QString()).isEmpty());
	}

	// The Cobbleverse page: a banner, then "Welcome to..." on the next
	// line of the same paragraph. The picture gets its own centred block,
	// before the text and not beside it.
	void imageLeavesItsParagraph()
	{
		const QString out = markdown(QStringLiteral(
			"![Banner](https://cdn.example/banner.png)\nWelcome to the **pack**!"));
		const int figure = out.indexOf(kFigure);
		const int text = out.indexOf(QStringLiteral("<p>Welcome to the <b>pack</b>!</p>"));
		QVERIFY2(figure >= 0, qPrintable(out));
		QVERIFY2(text > figure, qPrintable(out));
		QVERIFY(out.contains(QStringLiteral(
			"<img src=\"https://cdn.example/banner.png\" style=\"max-width: 100%\">")));
	}

	// Qt's Markdown parser drops a raw <img> or <br> and everything after
	// it; the body must survive whole.
	void rawHtmlVoidElementsKeepTheRest()
	{
		const QString out = markdown(QStringLiteral(
			"Line one<br>Line two\n\n<img src=\"https://a.example/x.png\">\n\nAfter"));
		QVERIFY2(out.contains(QStringLiteral("<p>Line one<br>Line two</p>")), qPrintable(out));
		QVERIFY(out.contains(QStringLiteral("https://a.example/x.png")));
		QVERIFY(out.contains(QStringLiteral("<p>After</p>")));
	}

	// [![alt](img)](href) and <a href><img></a> keep their link; pictures
	// of one paragraph share one row.
	void linkedImagesKeepTheirLinks()
	{
		const QString out = markdown(QStringLiteral(
			"[![Install](https://i.example/a.png)](https://example.org/install)\n"
			"<a href=\"https://example.org/discord\"><img src=\"https://i.example/b.png\"></a>"));
		QCOMPARE(out.count(kFigure), 1);
		QVERIFY2(out.contains(QStringLiteral(
					 "<a href=\"https://example.org/install\"><img src=\"https://i.example/a.png\"")),
				 qPrintable(out));
		QVERIFY(out.contains(QStringLiteral(
			"<a href=\"https://example.org/discord\"><img src=\"https://i.example/b.png\"")));
	}

	// A page written as one HTML block (no blank lines) reads as HTML:
	// nothing after its first picture is lost, lists stay lists.
	void htmlBlockBodyReadsAsHtml()
	{
		const QString out = markdown(QStringLiteral(
			"<p><center><img src=\"https://a.example/banner.png\"/></center></p>\n"
			"<p><center><font size=\"5\" color=\"red\">The Official Pack</font></center></p>\n"
			"<ul>\n<li>Catch them</li>\n<li>Battle them</li>\n</ul>\n"
			"<p>Last line</p>"));
		QVERIFY2(out.contains(kFigure), qPrintable(out));
		QVERIFY(out.contains(QStringLiteral("<p align=\"center\">The Official Pack</p>")));
		QVERIFY(out.contains(QStringLiteral("<ul><li>Catch them</li><li>Battle them</li></ul>")));
		QVERIFY(out.contains(QStringLiteral("<p>Last line</p>")));
		QVERIFY(!out.contains(QStringLiteral("red")));
	}

	// An image the author sized as an icon stays in its line of text.
	void iconsStayInline()
	{
		const QString out = markdown(QStringLiteral(
			"Join us <img src=\"https://a.example/discord.png\" width=\"20\" height=\"20\"> today"));
		QVERIFY2(!out.contains(kFigure), qPrintable(out));
		QVERIFY(out.contains(QStringLiteral("<p>Join us <img src=\"https://a.example/discord.png\"")));
		QVERIFY(out.contains(QStringLiteral("width=\"20\"> today</p>")));
	}

	// Only http(s) pictures and http(s)/mailto links: the page opens links
	// with the system.
	void unsafeLinksAndImagesGo()
	{
		const QString out = markdown(QStringLiteral(
			"[click](javascript:alert(1)) and [mail](mailto:a@example.org)\n\n"
			"![local](file:///etc/hosts.png)"));
		QVERIFY2(!out.contains(QStringLiteral("javascript")), qPrintable(out));
		QVERIFY(out.contains(QStringLiteral("click")));
		QVERIFY(out.contains(QStringLiteral("<a href=\"mailto:a@example.org\">mail</a>")));
		QVERIFY(!out.contains(QStringLiteral("file:")));
	}

	// HTML bodies: headings and rules kept, the author's styling dropped,
	// players gone.
	void htmlBodyIsNormalised()
	{
		const QString out = html(QStringLiteral(
			"<h2 style=\"color:#f00\">About</h2><p><span style=\"font-size:30px\">Big</span> text</p>"
			"<hr><iframe src=\"https://www.youtube.com/embed/x\"></iframe><p>End</p>"));
		QVERIFY2(out.contains(QStringLiteral("<h2>About</h2>")), qPrintable(out));
		QVERIFY(out.contains(QStringLiteral("<p>Big text</p>")));
		QVERIFY(out.contains(QStringLiteral("class=\"rule\"")));
		QVERIFY(!out.contains(QStringLiteral("youtube")));
		QVERIFY(out.contains(QStringLiteral("<p>End</p>")));
	}

	// Fenced code is shown exactly as written, tags and all.
	void fencedCodeIsLeftAlone()
	{
		const QString out = markdown(QStringLiteral(
			"```\n<img src=\"https://a.example/x.png\">\n```\n\nAfter"));
		QVERIFY2(out.contains(QStringLiteral("<pre>&lt;img src=&quot;https://a.example/x.png&quot;&gt;</pre>")),
				 qPrintable(out));
		QVERIFY(!out.contains(kFigure));
	}

	// Two or more breaks in a row are an author's spacer: a new paragraph.
	void doubleBreaksSplitParagraphs()
	{
		const QString out = html(QStringLiteral("<p>One<br><br><br>Two<br>Three</p>"));
		QVERIFY2(out.contains(QStringLiteral("<p>One</p><p>Two<br>Three</p>")), qPrintable(out));
	}
};

QTEST_MAIN(RichBodyTest)

#include "RichBody_test.moc"
