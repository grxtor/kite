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

#include "CrashStreak.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace CrashDoctor
{
	namespace
	{
		const QString kCrashes = QStringLiteral("crashes");
		const QString kBeforeStart = QStringLiteral("beforeStart");
		const QString kStartedAt = QStringLiteral("startedAt");
	} // namespace

	NativeCrashStreak NativeCrashStreak::afterCrash(bool native) const
	{
		NativeCrashStreak next = *this;
		next.crashes = native ? crashes + 1 : 0;
		return next;
	}

	NativeCrashStreak NativeCrashStreak::afterCleanRun(const QDateTime& now) const
	{
		if (crashes == 0) {
			return *this;
		}
		NativeCrashStreak next;
		next.crashesBeforeStart = crashes;
		next.startedAt = now;
		return next;
	}

	int NativeCrashStreak::retried(const QDateTime& now) const
	{
		const bool recent = startedAt.isValid() && startedAt.daysTo(now) <= kRetriedCrashesDays;
		return recent && crashesBeforeStart >= kMinRetriedCrashes ? crashesBeforeStart : 0;
	}

	QString NativeCrashStreak::toJson() const
	{
		QJsonObject json{{kCrashes, crashes}, {kBeforeStart, crashesBeforeStart}};
		if (startedAt.isValid()) {
			json.insert(kStartedAt, startedAt.toUTC().toString(Qt::ISODate));
		}
		return QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Compact));
	}

	NativeCrashStreak NativeCrashStreak::fromJson(const QString& json)
	{
		const QJsonObject object = QJsonDocument::fromJson(json.toUtf8()).object();
		NativeCrashStreak streak;
		streak.crashes = std::max(0, object.value(kCrashes).toInt());
		streak.crashesBeforeStart = std::max(0, object.value(kBeforeStart).toInt());
		streak.startedAt = QDateTime::fromString(object.value(kStartedAt).toString(), Qt::ISODate);
		return streak;
	}
} // namespace CrashDoctor
