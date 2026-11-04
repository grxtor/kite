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

#include <QTemporaryDir>
#include <QTest>

#include "models/ModUpdateReminder.h"
#include "settings/INISettingsObject.h"

namespace
{
	constexpr qint64 kHourMs = qint64(60) * 60 * 1000;
	// 2026-11-04 12:00 UTC.
	constexpr qint64 kNow = qint64(1793793600) * 1000;
	const QString kStamp = QStringLiteral("1.21.1 fabric");

	ModUpdateCheckTask::UpdateInfo update(const QString& file)
	{
		ModUpdateCheckTask::UpdateInfo info;
		info.currentFileName = file;
		info.name = file;
		return info;
	}

	ModUpdateReminder::Record checked(qint64 at, int found, const QString& stamp = kStamp)
	{
		ModUpdateReminder::Record record;
		record.checkedAtMs = at;
		record.found = found;
		record.checkedFor = stamp;
		return record;
	}
} // namespace

class ModUpdateReminderTest : public QObject
{
	Q_OBJECT

  private slots:
	void aCheckIsForAVersionAndLoader()
	{
		QCOMPARE(ModUpdateReminder::stampFor("1.21.1", "fabric"), kStamp);
		QCOMPARE(ModUpdateReminder::stampFor("1.21.1", QString()), QString("1.21.1"));
		QCOMPARE(ModUpdateReminder::stampFor(QString(), "fabric"), QString());
	}

	void anInstanceIsCheckedAtMostOnceADay()
	{
		// Never checked.
		QVERIFY(ModUpdateReminder::checkDue({}, kStamp, kNow));
		// An hour ago, then just under a day: not again.
		QVERIFY(!ModUpdateReminder::checkDue(checked(kNow - kHourMs, 3), kStamp, kNow));
		QVERIFY(!ModUpdateReminder::checkDue(
			checked(kNow - ModUpdateReminder::kCheckIntervalMs + 1, 3), kStamp, kNow));
		// A day ago: again.
		QVERIFY(ModUpdateReminder::checkDue(
			checked(kNow - ModUpdateReminder::kCheckIntervalMs, 3), kStamp, kNow));
		// A new Minecraft or loader changes what the platforms answer: at once.
		QVERIFY(ModUpdateReminder::checkDue(checked(kNow - kHourMs, 3, "1.21.1 neoforge"), kStamp, kNow));
		QVERIFY(ModUpdateReminder::checkDue(checked(kNow - kHourMs, 3, "1.20.1 fabric"), kStamp, kNow));
		// A clock set back does not stop the checks for good.
		QVERIFY(ModUpdateReminder::checkDue(checked(kNow + 10 * kHourMs, 3), kStamp, kNow));
		// No Minecraft version: nothing to ask.
		QVERIFY(!ModUpdateReminder::checkDue({}, QString(), kNow));
	}

	void theCountIsForTheVersionItWasFoundFor()
	{
		QCOMPARE(ModUpdateReminder::shownCount(checked(kNow, 3), kStamp), 3);
		QCOMPARE(ModUpdateReminder::shownCount(checked(kNow, 3, "1.20.1 fabric"), kStamp), 0);
		QCOMPARE(ModUpdateReminder::shownCount({}, kStamp), 0);
		QCOMPARE(ModUpdateReminder::shownCount(checked(kNow, -2), kStamp), 0);
		QCOMPARE(ModUpdateReminder::shownCount(checked(kNow, 3), QString()), 0);
	}

	void eachInstalledFileCountsOnce()
	{
		QCOMPARE(ModUpdateReminder::countUpdates({}), 0);
		QCOMPARE(ModUpdateReminder::countUpdates({update("sodium.jar"), update("lithium.jar"),
												  update("iris.jar")}),
				 3);
		// The same file twice, once switched off, is still one.
		QCOMPARE(ModUpdateReminder::countUpdates({update("sodium.jar"), update("sodium.jar.disabled"),
												  update("lithium.jar")}),
				 2);
	}

	void aCheckWithoutAnyAnswerTellsNothing()
	{
		QVERIFY(ModUpdateReminder::answered(12, 0));
		// A mod gone from Modrinth fails on its own; the rest answered.
		QVERIFY(ModUpdateReminder::answered(12, 11));
		// Offline: every ask failed.
		QVERIFY(!ModUpdateReminder::answered(12, 12));
		// Nothing to ask is an answer: none have updates.
		QVERIFY(ModUpdateReminder::answered(0, 0));
	}

	void aDesktopNotificationIsOptInAndOnceADay()
	{
		QVERIFY(!ModUpdateReminder::notifyDue(false, 0, kNow));
		QVERIFY(ModUpdateReminder::notifyDue(true, 0, kNow));
		QVERIFY(!ModUpdateReminder::notifyDue(true, kNow - kHourMs, kNow));
		QVERIFY(!ModUpdateReminder::notifyDue(
			true, kNow - ModUpdateReminder::kNotifyIntervalMs + 1, kNow));
		QVERIFY(ModUpdateReminder::notifyDue(true, kNow - ModUpdateReminder::kNotifyIntervalMs, kNow));
		QVERIFY(!ModUpdateReminder::notifyDue(false, kNow - 30 * 24 * kHourMs, kNow));
	}

	void theRecordReadsFromTheInstanceSettings()
	{
		QTemporaryDir dir;
		INISettingsObject settings(dir.filePath("instance.cfg"));
		settings.registerSetting(ModUpdateReminder::kCheckedAtSetting, QVariant(qlonglong(0)));
		settings.registerSetting(ModUpdateReminder::kFoundSetting, 0);
		settings.registerSetting(ModUpdateReminder::kForSetting, QString());
		QCOMPARE(ModUpdateReminder::recordOf(settings).checkedAtMs, qint64(0));

		settings.set(ModUpdateReminder::kCheckedAtSetting, kNow);
		settings.set(ModUpdateReminder::kFoundSetting, 4);
		settings.set(ModUpdateReminder::kForSetting, kStamp);
		// As the next start reads the file.
		INISettingsObject reread(dir.filePath("instance.cfg"));
		reread.registerSetting(ModUpdateReminder::kCheckedAtSetting, QVariant(qlonglong(0)));
		reread.registerSetting(ModUpdateReminder::kFoundSetting, 0);
		reread.registerSetting(ModUpdateReminder::kForSetting, QString());
		const ModUpdateReminder::Record record = ModUpdateReminder::recordOf(reread);
		QCOMPARE(record.checkedAtMs, kNow);
		QCOMPARE(record.found, 4);
		QCOMPARE(record.checkedFor, kStamp);
		QVERIFY(!ModUpdateReminder::checkDue(record, kStamp, kNow + kHourMs));
		QCOMPARE(ModUpdateReminder::shownCount(record, kStamp), 4);
	}

	void withoutALauncherNothingCounts()
	{
		ModUpdateReminder reminder;
		QCOMPARE(reminder.count("anything"), 0);
		reminder.check("anything");
		QCOMPARE(reminder.revision(), 0);
	}
};

QTEST_GUILESS_MAIN(ModUpdateReminderTest)

#include "ModUpdateReminder_test.moc"
