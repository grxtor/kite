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

#include <QList>
#include <QString>
#include <memory>

#include "modplatform/ModDownloadTypes.h"

class ModMetadataIndex;

/* Pure-function helper that classifies `DownloadItem`s against the
 * installed mods (`ModMetadataIndex`). Used by ModFolderPage and the
 * update flow to decide what to write to disk. */
class ModInstallConflictAnalyzer
{
  public:
	enum class Status {
		Fresh,			  /* Nothing comparable on disk; install normally */
		AlreadyInstalled, /* Same platform+projectId AND versionId present */
		UpdateAvailable,  /* Same platform+projectId, different versionId */
		NameConflict,	  /* Same normalized name from a different origin */
		FileNameClash	  /* Same target file name from a different source */
	};

	struct Decision {
		ModPlatform::DownloadItem item; /* Item, possibly with replaceExisting /
										   replacesFileName set */
		Status status = Status::Fresh;
		QString reason; /* Human-readable detail, surfaced in the UI */
	};

	/* Classifies each item; the result matches `items` in length and order.
	 * A null `index` makes every item Fresh. */
	static QList<Decision>
	analyze(const QList<ModPlatform::DownloadItem>& items,
			std::shared_ptr<ModMetadataIndex> index);

	/* Filters decisions down to what should be downloaded: Fresh,
	 * NameConflict and FileNameClash are kept as-is; UpdateAvailable is kept
	 * with `replaceExisting=true` and `replacesFileName` set; AlreadyInstalled
	 * is dropped. The caller asks the user about NameConflict / FileNameClash. */
	static QList<ModPlatform::DownloadItem>
	toDownloadPlan(const QList<Decision>& decisions);
};
