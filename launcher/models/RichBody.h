/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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
 * A project's long description (Markdown from Modrinth, FTB and ATLauncher,
 * often with raw HTML mixed in; HTML from CurseForge and Technic) rewritten as
 * one plain HTML article for QML rich text (RemoteRichText's `article` mode).
 * Parsed with QTextDocument and written back with semantic tags only (headings,
 * paragraphs, lists, quotes, code, rules, tables, links, inline bold, italic
 * and code); authors' fonts, sizes and colours are dropped.
 *
 * Images become their own blocks, centred in a row per paragraph and capped at
 * the text width (max-width: 100%). Centring uses a one-cell table: Qt Quick's
 * TextEdit draws an image alone in a centred paragraph at the left edge. Only an
 * image sized as an icon (kInlineImageMaxSize or smaller) stays inline.
 *
 * Links keep http, https and mailto targets only; images load over http and
 * https only, since the page opens links with Qt.openUrlExternally().
 */
namespace RichBody
{
enum class Format { Markdown, Html };

QString toArticleHtml(const QString& body, Format format);
} // namespace RichBody
