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

#include "CrashWatchList.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <algorithm>

namespace CrashWatch
{
	namespace
	{
		bool newerFirst(const Entry& a, const Entry& b)
		{
			return a.crashedAt > b.crashedAt;
		}
	} // namespace

	QList<Entry> load(const QString& path)
	{
		QFile file(path);
		QList<Entry> out;
		if (!file.open(QIODevice::ReadOnly)) {
			return out;
		}
		for (const QJsonValue& value : QJsonDocument::fromJson(file.readAll()).object().value("crashes").toArray()) {
			const QJsonObject item = value.toObject();
			Entry entry{item.value("instance").toString(),
						item.value("signature").toString(),
						QDateTime::fromString(item.value("crashedAt").toString(), Qt::ISODate),
						{item.value("crashReport").toString(), item.value("jvmError").toString(),
						 item.value("log").toString()},
						item.value("catalogVersion").toInt(),
						item.value("solvedBy").toString()};
			if (!entry.instanceId.isEmpty() && !entry.signature.isEmpty() && entry.crashedAt.isValid()) {
				out.append(entry);
			}
		}
		return out;
	}

	bool save(const QString& path, const QList<Entry>& entries)
	{
		QJsonArray crashes;
		for (const Entry& entry : entries) {
			crashes.append(QJsonObject{{"instance", entry.instanceId},
									   {"signature", entry.signature},
									   {"crashedAt", entry.crashedAt.toString(Qt::ISODate)},
									   {"crashReport", entry.files.crashReport},
									   {"jvmError", entry.files.jvmError},
									   {"log", entry.files.log},
									   {"catalogVersion", entry.catalogVersion},
									   {"solvedBy", entry.solvedBy}});
		}
		QDir().mkpath(QFileInfo(path).absolutePath());
		QSaveFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		file.write(QJsonDocument(QJsonObject{{"crashes", crashes}}).toJson(QJsonDocument::Compact));
		return file.commit();
	}

	QList<Entry> added(QList<Entry> entries, const Entry& entry)
	{
		entries.removeIf([&entry](const Entry& one) {
			return one.instanceId == entry.instanceId && one.signature == entry.signature;
		});
		entries.append(entry);
		std::stable_sort(entries.begin(), entries.end(), newerFirst);
		QList<Entry> kept;
		for (const Entry& one : entries) {
			const auto mine = std::count_if(kept.begin(), kept.end(),
											[&one](const Entry& other) { return other.instanceId == one.instanceId; });
			if (mine < kMaxPerInstance && kept.size() < kMaxEntries) {
				kept.append(one);
			}
		}
		return kept;
	}

	QList<Entry> pruned(QList<Entry> entries, const QDateTime& now,
						const std::function<bool(const QString&)>& exists)
	{
		entries.removeIf([&now, &exists](const Entry& entry) {
			const QStringList files{entry.files.crashReport, entry.files.jvmError, entry.files.log};
			const bool anyFile = std::any_of(files.begin(), files.end(), [&exists](const QString& file) {
				return !file.isEmpty() && exists(file);
			});
			return entry.crashedAt.daysTo(now) > kKeepDays || !anyFile;
		});
		return entries;
	}

	QList<Entry> without(QList<Entry> entries, const QString& instanceId, bool solvedOnly)
	{
		entries.removeIf([&](const Entry& entry) {
			return entry.instanceId == instanceId && (!solvedOnly || !entry.solvedBy.isEmpty());
		});
		return entries;
	}
} // namespace CrashWatch
