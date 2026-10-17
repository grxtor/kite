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

#pragma once

#include <QString>

/*
 * A project's long description -- Markdown from Modrinth, FTB and
 * ATLauncher (often with raw HTML mixed in), HTML from CurseForge and
 * Technic -- rewritten as one plain, predictable HTML article, so QML's
 * rich text (RemoteRichText's `article` mode) can lay it out the way the
 * project's own web page does.
 *
 * The body is parsed with QTextDocument (setMarkdown() / setHtml(), the
 * same parsers rich text itself would use) and written back out with only
 * semantic tags: headings, paragraphs, lists, quotes, code, rules, tables,
 * links and inline bold/italic/code. Authors' fonts, sizes and colours are
 * dropped -- they were picked for a white web page, and the article's
 * stylesheet sets the type instead.
 *
 * Images become their own blocks: an image never shares a line with the
 * text around it (a banner with "Welcome to" beside it), and the images of
 * one paragraph sit together in a centred row. Each one is capped at the
 * text width (max-width: 100%, so it scales down with its aspect ratio
 * and is never scaled up). Centring goes through a one-cell table aligned
 * to the centre: Qt Quick's TextEdit draws an image that is alone in a
 * centred paragraph at the paragraph's left edge. Only an image the
 * author sized as an icon (kInlineImageMaxSize or smaller) stays in its
 * line of text.
 *
 * Links keep http, https and mailto targets only; images load over http
 * and https only. Anything else loses its link or is dropped, since the
 * page opens a link with Qt.openUrlExternally().
 */
namespace RichBody
{
enum class Format { Markdown, Html };

QString toArticleHtml(const QString& body, Format format);
} // namespace RichBody
