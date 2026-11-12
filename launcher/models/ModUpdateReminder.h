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

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include "modplatform/ModUpdateCheckTask.h"

class MinecraftInstance;
class SettingsObject;

/*
 * The quiet "3 updates" of an instance's mods on Home's hero and in its Content
 * tab. check() asks Modrinth and CurseForge in the background with the Content
 * tab's own check (ModUpdateCheckTask), at most once a day per instance and
 * again when its Minecraft version or loader changes; what it found is kept in
 * the instance's settings, so the next start shows it at once.
 *
 * Nothing is ever installed here: the Content tab's review does that. A check
 * that got no answer at all (offline) says nothing and is tried again next time.
 * Instances whose mods a modpack manages are left out, as the pack's own update
 * brings theirs. The ModUpdateChecks setting turns it all off. A desktop
 * notification goes out only with ModUpdateNotifications on (off by default),
 * at most once a day for all instances together.
 */
class ModUpdateReminder : public QObject
{
	Q_OBJECT

	/// Moves whenever a count may have changed: count() bindings read it.
	Q_PROPERTY(int revision READ revision NOTIFY changed)

  public:
	/// Launcher settings.
	static constexpr const char* kEnabledSetting = "ModUpdateChecks";
	static constexpr const char* kNotifySetting = "ModUpdateNotifications";
	static constexpr const char* kNotifiedAtSetting = "ModUpdateNotifiedAt";
	/// Instance settings (MinecraftInstance registers them).
	static constexpr const char* kCheckedAtSetting = "ModUpdatesCheckedAt";
	static constexpr const char* kFoundSetting = "ModUpdatesFound";
	static constexpr const char* kForSetting = "ModUpdatesFor";

	static constexpr qint64 kCheckIntervalMs = qint64(24) * 60 * 60 * 1000;
	static constexpr qint64 kNotifyIntervalMs = qint64(24) * 60 * 60 * 1000;

	/// What the last check of an instance found, and for which version.
	struct Record
	{
		/// Milliseconds since the epoch; 0 for never.
		qint64 checkedAtMs = 0;
		int found = 0;
		/// stampFor() of the instance when it was checked.
		QString checkedFor;
	};

	/// What a check is for: the Minecraft version and loader, which change
	/// what the platforms answer ("1.21.1 fabric"). "" without a version.
	static QString stampFor(const QString& mcVersion, const QString& loader);
	/// Whether an instance last checked as @p record, now at @p stamp, is due
	/// for another check at @p nowMs: never checked, checked for another
	/// Minecraft or loader, or a day or more ago.
	static bool checkDue(const Record& record, const QString& stamp, qint64 nowMs);
	/// What the indicator says for @p record of an instance now at @p stamp:
	/// the count, or 0 when it was found for another version.
	static int shownCount(const Record& record, const QString& stamp);
	/// How many installed files @p updates would change.
	static int countUpdates(const QList<ModUpdateCheckTask::UpdateInfo>& updates);
	/// Whether a check that asked @p lookups mods, @p failed of them without
	/// an answer, tells anything: not when every ask failed.
	static bool answered(int lookups, int failed);
	/// Whether a desktop notification may go out at @p nowMs: opted in, and
	/// none in the last day.
	static bool notifyDue(bool optedIn, qint64 lastNotifiedMs, qint64 nowMs);

	static Record recordOf(const SettingsObject& settings);
	/// stampFor() of @p instance as it is now.
	static QString stampOf(MinecraftInstance& instance);

	/// The one the launcher uses; ContentUpdateController reports to it too.
	static ModUpdateReminder* instance();

	explicit ModUpdateReminder(QObject* parent = nullptr);
	~ModUpdateReminder() override;

	int revision() const
	{
		return m_revision;
	}

	/// Checks instance @p instanceId in the background if it is due; one
	/// check at a time, the rest wait their turn. Silent in every case.
	Q_INVOKABLE void check(const QString& instanceId);
	/// How many of its mods have a newer version, as last found; 0 while
	/// ModUpdateChecks is off or when it was found for another version.
	Q_INVOKABLE int count(const QString& instanceId) const;

	/// The Content tab checked all mods of @p instance (or updated some) and
	/// @p found are left: kept as if this had checked just now.
	void record(MinecraftInstance& instance, int found);
	/// Dev only (KITE_QML_ROUTE "modupdates=<id>,<count>"): as if a check of
	/// instance @p instanceId had just found @p found, for review snapshots.
	Q_INVOKABLE void debugRecord(const QString& instanceId, int found);

  signals:
	void changed();

  private:
	bool enabled() const;
	void startNext();
	void finish(const QString& instanceId, const QString& stamp, int lookups,
				int failed, int found);
	void notify(MinecraftInstance& instance, int found);
	void bump();

	QStringList m_queue;
	QString m_running;
	QPointer<ModUpdateCheckTask> m_task;
	int m_revision = 0;
};
