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

#include "models/RichBody.h"

#include <QFont>
#include <QList>
#include <QRegularExpression>
#include <QStringList>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFormat>
#include <QTextFrame>
#include <QTextList>
#include <QTextTable>
#include <QUrl>

#include <initializer_list>

namespace
{
// An image the author sized this small (width or height, in pixels) is an
// icon or an emoji in a line of text, not a picture: it stays inline.
constexpr int kInlineImageMaxSize = 48;

// Line breaks inside one paragraph (<br>, or a hard break in Markdown)
// come through the parsers as this character.
constexpr char16_t kLineSeparator = 0x2028;

/* Stand-ins for the raw HTML that Qt's Markdown parser loses (see
 * protectHtml()), from the Unicode private use area, so no real text can
 * contain them. An image is kImageOpen, its index in the image list, then
 * kImageClose. */
constexpr char16_t kImageOpen = 0xE000;
constexpr char16_t kImageClose = 0xE001;
constexpr char16_t kBreakToken = 0xE002;
constexpr char16_t kRuleToken = 0xE003;

bool hasScheme(const QString& url, std::initializer_list<const char*> schemes)
{
	const QString scheme = QUrl(url.trimmed()).scheme().toLower();
	for (const char* allowed : schemes) {
		if (scheme == QLatin1String(allowed)) {
			return true;
		}
	}
	return false;
}

bool isSafeLink(const QString& href)
{
	return hasScheme(href, {"http", "https", "mailto"});
}

bool isSafeImage(const QString& src)
{
	return hasScheme(src, {"http", "https"});
}

QString attr(const QString& value)
{
	return value.trimmed().toHtmlEscaped();
}

/* A picture as the body names it. Width and height are the author's, in
 * pixels, or 0 when not given (or given in percent). */
struct ImageRef {
	QString src;
	QString href;
	int width = 0;
	int height = 0;

	// Sized, and small in every dimension it was given.
	bool isIcon() const
	{
		return (width > 0 || height > 0) && width <= kInlineImageMaxSize &&
			   height <= kInlineImageMaxSize;
	}
};

const auto kCaseless = QRegularExpression::CaseInsensitiveOption |
					   QRegularExpression::DotMatchesEverythingOption;
const QRegularExpression kLeadingSpace(QStringLiteral("^\\s+"));

// The value of attribute @p name in the HTML tag @p tag, entities for "&"
// and quotes undone; empty when absent.
QString tagAttribute(const QString& tag, const QString& name)
{
	const QRegularExpression re(
		QStringLiteral("\\b%1\\s*=\\s*(?:\"([^\"]*)\"|'([^']*)'|([^\\s>]+))")
			.arg(name),
		kCaseless);
	const QRegularExpressionMatch match = re.match(tag);
	if (!match.hasMatch()) {
		return QString();
	}
	QString value = match.captured(1) + match.captured(2) + match.captured(3);
	value.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
	value.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
	value.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
	return value.trimmed();
}

// "300" or "300px" -> 300; "50%", "auto" or nothing -> 0.
int pixels(const QString& value)
{
	QString number = value.trimmed();
	if (number.endsWith(QStringLiteral("px"), Qt::CaseInsensitive)) {
		number.chop(2);
	}
	bool ok = false;
	const int result = number.toInt(&ok);
	return ok && result > 0 ? result : 0;
}

// Players, scripts and the like: nothing rich text can show, in either
// format.
QString withoutEmbeds(QString html)
{
	static const QRegularExpression dropped(
		QStringLiteral("<(iframe|video|audio|script|style|object)\\b[^>]*>.*?</\\1\\s*>"
					   "|<(iframe|video|audio|script|style|object|embed|source|"
					   "input|meta|link|wbr)\\b[^>]*/?>"),
		kCaseless);
	return html.remove(dropped);
}

/* Qt's Markdown parser hands inline HTML to its HTML importer one balanced
 * element at a time, and a void element (<img>, <br>, <hr>) never balances:
 * it and everything after it in the body vanish. It also loses the link
 * around a Markdown image ([![alt](src)](href)). So before parsing, those
 * become tokens the writer turns back into pictures, breaks and rules; and
 * the purely presentational wrappers (<center>, <div>, <details>) go, since
 * an open one swallows every Markdown block up to its closing tag into one
 * paragraph. */
QString protectHtml(QString text, QList<ImageRef>& images)
{
	static const QRegularExpression htmlLinkedImage(
		QStringLiteral("<a\\b([^>]*)>\\s*(<img\\b[^>]*>)\\s*</a\\s*>"), kCaseless);
	static const QRegularExpression markdownLinkedImage(
		QStringLiteral("\\[\\s*!\\[[^\\]]*\\]\\(\\s*<?([^)\\s>]+)>?(?:\\s+\"[^\"]*\")?\\s*\\)"
					   "\\s*\\]\\(\\s*<?([^)\\s>]+)>?(?:\\s+\"[^\"]*\")?\\s*\\)"));
	static const QRegularExpression htmlImage(QStringLiteral("<img\\b[^>]*>"),
											  kCaseless);
	static const QRegularExpression lineBreak(QStringLiteral("<br\\b[^>]*>"),
											  kCaseless);
	static const QRegularExpression rule(QStringLiteral("<hr\\b[^>]*>"), kCaseless);
	static const QRegularExpression wrapper(
		QStringLiteral("</?(center|div|details|picture)\\b[^>]*>"), kCaseless);
	static const QRegularExpression summary(
		QStringLiteral("<summary\\b[^>]*>(.*?)</summary\\s*>"), kCaseless);

	const auto token = [&images](const ImageRef& image) {
		images.append(image);
		return QChar(kImageOpen) + QString::number(images.size() - 1) +
			   QChar(kImageClose);
	};
	const auto imageFromTag = [](const QString& tag) {
		return ImageRef{tagAttribute(tag, QStringLiteral("src")), QString(),
						pixels(tagAttribute(tag, QStringLiteral("width"))),
						pixels(tagAttribute(tag, QStringLiteral("height")))};
	};
	const auto replaceAll = [](QString& text, const QRegularExpression& re,
							   const auto& replacement) {
		QString result;
		qsizetype last = 0;
		auto it = re.globalMatch(text);
		while (it.hasNext()) {
			const QRegularExpressionMatch match = it.next();
			result += text.mid(last, match.capturedStart() - last);
			result += replacement(match);
			last = match.capturedEnd();
		}
		result += text.mid(last);
		text = result;
	};

	text = withoutEmbeds(text);
	replaceAll(text, htmlLinkedImage, [&](const QRegularExpressionMatch& m) {
		ImageRef image = imageFromTag(m.captured(2));
		image.href = tagAttribute(m.captured(1), QStringLiteral("href"));
		return token(image);
	});
	replaceAll(text, markdownLinkedImage, [&](const QRegularExpressionMatch& m) {
		return token(ImageRef{m.captured(1), m.captured(2), 0, 0});
	});
	replaceAll(text, htmlImage, [&](const QRegularExpressionMatch& m) {
		return token(imageFromTag(m.captured(0)));
	});
	text.replace(lineBreak, QString(QChar(kBreakToken)));
	text.replace(rule, QString(QChar(kRuleToken)));
	replaceAll(text, summary, [](const QRegularExpressionMatch& m) {
		return QStringLiteral("\n\n<b>%1</b>\n\n").arg(m.captured(1).trimmed());
	});
	return text.replace(wrapper, QStringLiteral("\n"));
}

/* A Markdown body cut where CommonMark itself switches between Markdown
 * and raw HTML: a line opening a block-level tag starts an HTML block,
 * which runs to the next blank line, and is HTML through and through (a
 * pack page written as one long HTML document is one such block). Each
 * piece goes to the parser that reads it properly -- HTML to Qt's HTML
 * importer, which has none of the Markdown parser's trouble with it
 * (protectHtml()) -- in order, into one document. Fenced code stays
 * Markdown, exactly as written. */
void readMarkdown(QTextDocument& document, const QString& body,
				  QList<ImageRef>& images)
{
	static const QRegularExpression htmlBlockStart(
		QStringLiteral(
			"^ {0,3}</?(address|article|aside|blockquote|body|caption|center|col|"
			"colgroup|dd|details|dialog|dir|div|dl|dt|fieldset|figcaption|"
			"figure|footer|form|h[1-6]|header|hr|html|legend|li|main|menu|nav|"
			"ol|p|section|summary|table|tbody|td|tfoot|th|thead|tr|ul)(\\s|/?>|$)"),
		QRegularExpression::CaseInsensitiveOption);
	static const QRegularExpression fence(QStringLiteral("^ {0,3}(```|~~~)"));

	QTextCursor cursor(&document);
	bool first = true;
	const auto insert = [&](const QStringList& lines, bool html) {
		if (lines.join(QString()).trimmed().isEmpty()) {
			return;
		}
		if (!first) {
			// A fresh, plain block: not the end of a list or a heading.
			cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
		}
		first = false;
		if (html) {
			cursor.insertHtml(withoutEmbeds(lines.join(QLatin1Char('\n'))));
			return;
		}
		// Prose is protected a stretch at a time (a tag may span lines);
		// fenced code goes in as written.
		QStringList markdown;
		QStringList prose;
		bool inCode = false;
		const auto flushProse = [&]() {
			if (!prose.isEmpty()) {
				markdown.append(protectHtml(prose.join(QLatin1Char('\n')), images));
				prose.clear();
			}
		};
		for (const QString& line : lines) {
			const bool isFence = fence.match(line).hasMatch();
			if (inCode || isFence) {
				flushProse();
				markdown.append(line);
				inCode = isFence ? !inCode : inCode;
			} else {
				prose.append(line);
			}
		}
		flushProse();
		cursor.insertMarkdown(markdown.join(QLatin1Char('\n')),
							  QTextDocument::MarkdownDialectGitHub);
	};

	QStringList run;
	bool runIsHtml = false;
	bool inFence = false;
	for (const QString& line : body.split(QLatin1Char('\n'))) {
		if (inFence || fence.match(line).hasMatch()) {
			if (fence.match(line).hasMatch()) {
				inFence = !inFence;
			}
			if (runIsHtml) {
				insert(run, true);
				run.clear();
				runIsHtml = false;
			}
			run.append(line);
			continue;
		}
		if (runIsHtml && line.trimmed().isEmpty()) {
			insert(run, true);
			run.clear();
			runIsHtml = false;
		} else if (!runIsHtml && htmlBlockStart.match(line).hasMatch()) {
			insert(run, false);
			run.clear();
			runIsHtml = true;
		}
		run.append(line);
	}
	insert(run, runIsHtml);
}

/* One piece of a paragraph, in order: a run of formatted text, a line
 * break, a picture, or a horizontal rule. */
struct Piece {
	enum Kind { Text, Break, Image, Rule } kind = Text;
	QString html;
	// Text only: whether it shows anything at all.
	bool blank = true;
};

/* A horizontal rule. Not <hr>: Qt Quick's TextEdit draws that in the
 * body text's colour, far louder than a divider should be. A full-width
 * cell one pixel of type high takes its colour from the stylesheet
 * (`td.rule`) instead. */
QString ruleHtml()
{
	return QStringLiteral("<table class=\"rule\" width=\"100%\" cellspacing=\"0\" "
						  "cellpadding=\"0\" border=\"0\"><tr><td class=\"rule\">"
						  "&nbsp;</td></tr></table>");
}

class Writer
{
  public:
	explicit Writer(const QList<ImageRef>& images) : m_images(images) {}

	QString out;

	void writeFrame(QTextFrame* frame)
	{
		for (QTextFrame::iterator it = frame->begin(); !it.atEnd(); ++it) {
			if (QTextFrame* child = it.currentFrame()) {
				closeOpenBlocks();
				if (auto* table = qobject_cast<QTextTable*>(child)) {
					writeTable(table);
				} else {
					writeFrame(child);
				}
			} else if (it.currentBlock().isValid()) {
				writeBlock(it.currentBlock());
			}
		}
		closeOpenBlocks();
	}

  private:
	const QList<ImageRef>& m_images;
	QList<QTextList*> m_lists;
	QStringList m_codeLines;

	void closeOpenBlocks()
	{
		closeLists(nullptr);
		flushCode();
	}

	static bool isOrdered(const QTextList* list)
	{
		return list->format().style() <= QTextListFormat::ListDecimal;
	}

	// Closes every open list deeper than (or beside) @p keep; all of them
	// for nullptr.
	void closeLists(QTextList* keep)
	{
		while (!m_lists.isEmpty() && m_lists.last() != keep &&
			   (!keep ||
				m_lists.last()->format().indent() >= keep->format().indent())) {
			out += isOrdered(m_lists.takeLast()) ? QStringLiteral("</ol>")
												 : QStringLiteral("</ul>");
		}
	}

	void openList(QTextList* list)
	{
		closeLists(list);
		if (!m_lists.isEmpty() && m_lists.last() == list) {
			return;
		}
		m_lists.append(list);
		out += isOrdered(list) ? QStringLiteral("<ol>") : QStringLiteral("<ul>");
	}

	void flushCode()
	{
		if (m_codeLines.isEmpty()) {
			return;
		}
		out += QStringLiteral("<pre>") + m_codeLines.join(QLatin1Char('\n')) +
			   QStringLiteral("</pre>");
		m_codeLines.clear();
	}

	static QString imageHtml(const ImageRef& image)
	{
		QString html = QStringLiteral("<img src=\"%1\" style=\"max-width: 100%\"")
						   .arg(attr(image.src));
		// The author's width, if any, still applies below the cap; the
		// height follows from the picture's own aspect ratio.
		if (image.width > 0) {
			html += QStringLiteral(" width=\"%1\"").arg(image.width);
		}
		html += QLatin1Char('>');
		if (isSafeLink(image.href)) {
			html = QStringLiteral("<a href=\"%1\">%2</a>").arg(attr(image.href), html);
		}
		return html;
	}

	static QString styledText(const QString& text, const QTextCharFormat& format,
							  bool heading)
	{
		QString html = text.toHtmlEscaped();
		if (format.fontFixedPitch()) {
			html = QStringLiteral("<code>") + html + QStringLiteral("</code>");
		}
		if (format.fontStrikeOut()) {
			html = QStringLiteral("<s>") + html + QStringLiteral("</s>");
		}
		if (format.fontItalic()) {
			html = QStringLiteral("<i>") + html + QStringLiteral("</i>");
		}
		// A heading is bold already; saying so again changes nothing.
		if (!heading && format.fontWeight() >= QFont::DemiBold) {
			html = QStringLiteral("<b>") + html + QStringLiteral("</b>");
		}
		const QString href = format.anchorHref();
		if (format.isAnchor() && isSafeLink(href)) {
			html = QStringLiteral("<a href=\"%1\">%2</a>").arg(attr(href), html);
		}
		return html;
	}

	// @p splitImages: pictures become Piece::Image; otherwise (a list item,
	// a table cell) they stay inline, as icons always do.
	void appendImage(QList<Piece>& result, ImageRef image,
					 const QTextCharFormat& format, bool splitImages) const
	{
		if (!isSafeImage(image.src)) {
			return;
		}
		if (image.href.isEmpty() && format.isAnchor()) {
			image.href = format.anchorHref();
		}
		const bool inlineImage = !splitImages || image.isIcon();
		result.append({inlineImage ? Piece::Text : Piece::Image,
					   imageHtml(image), false});
	}

	// The text of one fragment, cut at line breaks and at the tokens
	// protectHtml() left in it.
	void appendText(QList<Piece>& result, const QString& text,
					const QTextCharFormat& format, bool heading,
					bool splitImages) const
	{
		QString run;
		const auto flushRun = [&]() {
			if (!run.isEmpty()) {
				result.append({Piece::Text, styledText(run, format, heading),
							   run.trimmed().isEmpty()});
				run.clear();
			}
		};
		for (qsizetype i = 0; i < text.size(); ++i) {
			const char16_t c = text.at(i).unicode();
			if (c == kLineSeparator || c == kBreakToken) {
				flushRun();
				result.append({Piece::Break, QString(), true});
			} else if (c == kRuleToken) {
				flushRun();
				result.append({Piece::Rule, QString(), true});
			} else if (c == kImageOpen) {
				const qsizetype close = text.indexOf(QChar(kImageClose), i);
				if (close < 0) {
					continue;
				}
				flushRun();
				const int index = text.mid(i + 1, close - i - 1).toInt();
				if (index >= 0 && index < m_images.size()) {
					appendImage(result, m_images.at(index), format, splitImages);
				}
				i = close;
			} else {
				run += text.at(i);
			}
		}
		flushRun();
	}

	QList<Piece> pieces(const QTextBlock& block, bool heading,
						bool splitImages) const
	{
		QList<Piece> result;
		for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
			const QTextFragment fragment = it.fragment();
			if (!fragment.isValid()) {
				continue;
			}
			const QTextCharFormat format = fragment.charFormat();
			if (!format.isImageFormat()) {
				appendText(result, fragment.text(), format, heading, splitImages);
				continue;
			}
			const QTextImageFormat image = format.toImageFormat();
			const ImageRef ref{
				image.name(), QString(),
				image.hasProperty(QTextFormat::ImageWidth) ? qRound(image.width()) : 0,
				image.hasProperty(QTextFormat::ImageHeight) ? qRound(image.height())
															: 0};
			// One object character per picture: a fragment can hold
			// several copies of the same one.
			for (int i = 0; i < fragment.length(); ++i) {
				appendImage(result, ref, format, splitImages);
			}
		}
		return result;
	}

	// The pieces of one paragraph, as paragraphs of text and rows of
	// pictures. One line break stays a <br>; two or more in a row (an
	// author's spacer) end the paragraph instead.
	void writeParagraphs(const QList<Piece>& all, const QString& open,
						 const QString& close)
	{
		QStringList text;
		bool textShows = false;
		int pendingBreaks = 0;
		QStringList images;

		const auto flushText = [&]() {
			if (textShows) {
				out += open + text.join(QString()) + close;
			}
			text.clear();
			textShows = false;
			pendingBreaks = 0;
		};
		const auto flushImages = [&]() {
			if (images.isEmpty()) {
				return;
			}
			out += QStringLiteral(
					   "<table class=\"figure\" align=\"center\" "
					   "cellpadding=\"0\" cellspacing=\"0\" border=\"0\"><tr><td>") +
				   images.join(QLatin1Char(' ')) +
				   QStringLiteral("</td></tr></table>");
			images.clear();
		};

		for (const Piece& piece : all) {
			switch (piece.kind) {
				case Piece::Image:
					flushText();
					images.append(piece.html);
					break;
				case Piece::Rule:
					flushText();
					flushImages();
					out += ruleHtml();
					break;
				case Piece::Break:
					if (textShows) {
						++pendingBreaks;
					}
					break;
				case Piece::Text:
					if (piece.blank && !textShows) {
						// Space before anything shows, or between pictures.
						break;
					}
					flushImages();
					if (pendingBreaks > 1) {
						flushText();
					} else if (pendingBreaks == 1) {
						text.append(QStringLiteral("<br>"));
					}
					pendingBreaks = 0;
					// A paragraph starts on its first word.
					text.append(textShows ? piece.html
										  : QString(piece.html).remove(kLeadingSpace));
					textShows = textShows || !piece.blank;
					break;
			}
		}
		flushText();
		flushImages();
	}

	static QString inlineHtml(const QList<Piece>& all)
	{
		QStringList html;
		for (const Piece& piece : all) {
			html.append(piece.kind == Piece::Text ? piece.html
												  : QStringLiteral("<br>"));
		}
		// No break at either end of a list item or a cell.
		while (!html.isEmpty() && html.first() == QStringLiteral("<br>")) {
			html.removeFirst();
		}
		while (!html.isEmpty() && html.last() == QStringLiteral("<br>")) {
			html.removeLast();
		}
		return html.join(QString());
	}

	static bool showsAnything(const QList<Piece>& all)
	{
		for (const Piece& piece : all) {
			if (piece.kind == Piece::Image || piece.kind == Piece::Rule ||
				(piece.kind == Piece::Text && !piece.blank)) {
				return true;
			}
		}
		return false;
	}

	void writeBlock(const QTextBlock& block)
	{
		const QTextBlockFormat format = block.blockFormat();

		if (format.hasProperty(QTextFormat::BlockTrailingHorizontalRulerWidth)) {
			closeOpenBlocks();
			out += ruleHtml();
			return;
		}
		if (format.hasProperty(QTextFormat::BlockCodeFence) ||
			format.hasProperty(QTextFormat::BlockCodeLanguage) ||
			format.nonBreakableLines()) {
			closeLists(nullptr);
			m_codeLines.append(block.text()
								   .replace(QChar(kLineSeparator), QLatin1Char('\n'))
								   .toHtmlEscaped());
			return;
		}
		flushCode();

		const int level = qBound(0, format.headingLevel(), 6);
		QTextList* list = block.textList();
		const QList<Piece> all = pieces(block, level > 0, !list);
		if (!showsAnything(all)) {
			return;
		}

		if (list) {
			openList(list);
			out += QStringLiteral("<li>") + inlineHtml(all) +
				   QStringLiteral("</li>");
			return;
		}
		closeLists(nullptr);

		const QString tag = level > 0 ? QStringLiteral("h%1").arg(level)
									  : QStringLiteral("p");
		const Qt::Alignment alignment = format.alignment();
		const QString align = alignment & Qt::AlignHCenter
								  ? QStringLiteral(" align=\"center\"")
							  : alignment & Qt::AlignRight
								  ? QStringLiteral(" align=\"right\"")
								  : QString();
		const bool quote = format.intProperty(QTextFormat::BlockQuoteLevel) > 0;

		if (quote) {
			out += QStringLiteral("<blockquote>");
		}
		writeParagraphs(all, QStringLiteral("<%1%2>").arg(tag, align),
						QStringLiteral("</%1>").arg(tag));
		if (quote) {
			out += QStringLiteral("</blockquote>");
		}
	}

	QString cellHtml(const QTextTableCell& cell) const
	{
		QStringList lines;
		for (QTextFrame::iterator it = cell.begin(); !it.atEnd(); ++it) {
			if (it.currentFrame() || !it.currentBlock().isValid()) {
				continue;
			}
			const QList<Piece> all = pieces(it.currentBlock(), false, false);
			if (showsAnything(all)) {
				lines.append(inlineHtml(all));
			}
		}
		return lines.join(QStringLiteral("<br>"));
	}

	void writeTable(QTextTable* table)
	{
		const int headerRows = table->format().headerRowCount();
		QString rowsHtml;
		bool showsAnything = false;
		for (int row = 0; row < table->rows(); ++row) {
			rowsHtml += QStringLiteral("<tr>");
			for (int column = 0; column < table->columns(); ++column) {
				const QTextTableCell cell = table->cellAt(row, column);
				// A spanned cell is written once, at its top-left corner.
				if (!cell.isValid() || cell.row() != row ||
					cell.column() != column) {
					continue;
				}
				const QString tag = row < headerRows ? QStringLiteral("th")
													 : QStringLiteral("td");
				QString spans;
				if (cell.rowSpan() > 1) {
					spans += QStringLiteral(" rowspan=\"%1\"").arg(cell.rowSpan());
				}
				if (cell.columnSpan() > 1) {
					spans += QStringLiteral(" colspan=\"%1\"").arg(cell.columnSpan());
				}
				const QString content = cellHtml(cell);
				showsAnything = showsAnything || !content.isEmpty();
				rowsHtml += QStringLiteral("<%1%2>%3</%1>").arg(tag, spans, content);
			}
			rowsHtml += QStringLiteral("</tr>");
		}
		// HTML pages lay pictures out with empty tables; nothing to show.
		if (showsAnything) {
			out += QStringLiteral("<table class=\"grid\" border=\"1\" cellspacing=\"0\" "
								  "cellpadding=\"6\">") +
				   rowsHtml + QStringLiteral("</table>");
		}
	}
};
} // namespace

namespace RichBody
{
QString toArticleHtml(const QString& body, Format format)
{
	if (body.trimmed().isEmpty()) {
		return QString();
	}
	QList<ImageRef> images;
	QTextDocument document;
	if (format == Format::Markdown) {
		readMarkdown(document, body, images);
	} else {
		document.setHtml(withoutEmbeds(body));
	}
	Writer writer(images);
	writer.writeFrame(document.rootFrame());
	return writer.out;
}
} // namespace RichBody
