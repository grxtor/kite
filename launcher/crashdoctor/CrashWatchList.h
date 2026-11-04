/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include <QDateTime>
#include <QList>
#include <QString>

#include <functional>

#include "CrashCollector.h"

/*
 * The crashes Kite could not explain, kept on this computer for a while
 * (<data>/crash-reports/unresolved.json) so that newer crash rules can read
 * them again: which instance, which files, when, and the signature the files
 * gave. Nothing here leaves the computer. Pure, apart from load() and save().
 */
namespace CrashWatch
{
	struct Entry {
		QString instanceId;
		QString signature;
		QDateTime crashedAt;
		CrashDoctor::RunFiles files;
		/// The crash rules it was last read with.
		int catalogVersion = 0;
		/// The rule that knew it once newer rules came, "" while none did.
		QString solvedBy;
	};

	QList<Entry> load(const QString& path);
	bool save(const QString& path, const QList<Entry>& entries);

	/// @p entries with @p entry in it: replaces the one of the same instance
	/// and signature, and keeps at most kMaxPerInstance per instance (the
	/// newest) and kMaxEntries in all.
	QList<Entry> added(QList<Entry> entries, const Entry& entry);
	/// Without the entries older than kKeepDays and those whose files are all
	/// gone (@p exists says whether a file is still there).
	QList<Entry> pruned(QList<Entry> entries, const QDateTime& now,
						const std::function<bool(const QString&)>& exists);
	/// Without @p instanceId's entries; only its solved ones with @p solvedOnly.
	QList<Entry> without(QList<Entry> entries, const QString& instanceId, bool solvedOnly);

	inline constexpr int kKeepDays = 30;
	inline constexpr int kMaxPerInstance = 3;
	inline constexpr int kMaxEntries = 20;
} // namespace CrashWatch
