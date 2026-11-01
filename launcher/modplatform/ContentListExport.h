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

#include <QFlags>
#include <QList>
#include <QString>
#include <QStringList>

/* Renders a folder's contents as a pasteable list. Knows nothing about
 * Mod, the folder models or the sidecar index: the page collects what it
 * knows per file into an Item, so one code path serves every content type. */
namespace ContentListExport
{
	enum class Format { Html, Markdown, PlainText, Json, Csv, Custom };

	/* Which optional columns to include; the name is always there. */
	enum Field {
		NoFields = 0,
		Authors = 1 << 0,
		Url = 1 << 1,
		Version = 1 << 2,
		FileName = 1 << 3
	};
	Q_DECLARE_FLAGS(Fields, Field)

	struct Item {
		QString name;
		QString version;
		QString url;
		QString fileName;
		/* The mod id declared inside the archive, not the platform project id.
		 * Only reachable through the custom template. */
		QString modId;
		QStringList authors;
	};

	/* One of the built-in formats. `fields` is ignored by Custom; use the
	 * template overload. */
	QString render(const QList<Item>& items, Format format, Fields fields);

	/* One line per item with `{name}`, `{mod_id}`, `{url}`, `{version}`,
	 * `{authors}` and `{filename}` substituted. */
	QString render(const QList<Item>& items, const QString& lineTemplate);

	/* A format's own line, offered as the start of a custom template. */
	QString exampleLine(Format format);

	QString fileExtension(Format format);
}

Q_DECLARE_OPERATORS_FOR_FLAGS(ContentListExport::Fields)
