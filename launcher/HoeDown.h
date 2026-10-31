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

#pragma once
#include <cmark.h>
#include <QString>
#include <QByteArray>

/** Markdown-to-HTML wrapper using cmark (CommonMark). */
class HoeDown
{
  public:
	QString process(QByteArray input)
	{
		/* CMARK_OPT_UNSAFE lets raw HTML through: CurseForge sends HTML only
		 * (it would render empty) and Modrinth markdown carries <details>,
		 * <img> and <center>. The output goes to a QTextBrowser, which runs no
		 * scripts, and the panes refuse non-http(s) links. CMARK_OPT_NOBREAKS
		 * renders a single newline as a space. */
		char* html = cmark_markdown_to_html(
			input.constData(), input.size(),
			CMARK_OPT_UNSAFE | CMARK_OPT_NOBREAKS);
		QString result = QString::fromUtf8(html);
		free(html);
		return result;
	}
};
