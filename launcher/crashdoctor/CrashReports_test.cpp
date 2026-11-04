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

#include <QJsonArray>
#include <QTemporaryDir>
#include <QTest>

#include "crashdoctor/CrashFollowUp.h"
#include "crashdoctor/CrashOutbox.h"
#include "crashdoctor/CrashPayload.h"
#include "crashdoctor/CrashWatchList.h"

using namespace CrashDoctor;

/*
 * A crash report from the launcher's side: the exact payload, the outbox that
 * keeps what could not be sent, the list of crashes kept to read again, and
 * the words of the "fix found" notification.
 */
namespace
{
	Report sampleReport()
	{
		Report report;
		report.instanceId = QStringLiteral("skies");
		report.facts.minecraft = QStringLiteral("1.20.1");
		report.facts.loader = QStringLiteral("forge");
		report.facts.os = QStringLiteral("macos");
		report.facts.arch = QStringLiteral("arm64");
		report.facts.javaNow = 17;
		report.loaderVersion = QStringLiteral("47.3.0");
		report.mods = {{QStringLiteral("create"), QStringLiteral("0.5.1.j")},
					   {QStringLiteral("create"), QStringLiteral("0.5.1.j")},
					   {QStringLiteral("Bad Id"), QStringLiteral("1")},
					   {QStringLiteral("jei"), QStringLiteral("15.2.0 (beta)")}};
		report.lines.lines = {QStringLiteral("java.lang.NullPointerException: x"),
							  QStringLiteral("at com.simibubi.create.Foo.bar(Foo.java)")};
		report.lines.culprit = QStringLiteral("create");
		report.signature = signatureOf(report.lines.lines);
		return report;
	}

	QJsonObject payloadWith(const QString& signature)
	{
		return QJsonObject{{"formatVersion", 1}, {"signature", signature}};
	}
} // namespace

class CrashReportsTest : public QObject
{
	Q_OBJECT

  private slots:
	void payloadHasExactlyTheFields()
	{
		const QJsonObject payload = CrashPayload::build(sampleReport(), QStringLiteral("1.2.0"));
		QStringList keys = payload.keys();
		keys.sort();
		QCOMPARE(keys, (QStringList{"arch", "culprit", "formatVersion", "java", "kite", "lines", "loader", "minecraft",
									"mods", "os", "rules", "signature"}));
		QCOMPARE(payload.value("formatVersion").toInt(), 1);
		QCOMPARE(payload.value("signature").toString(), signatureOf(sampleReport().lines.lines));
		QCOMPARE(payload.value("java").toInt(), 17);
		QCOMPARE(payload.value("loader").toObject(),
				 (QJsonObject{{"type", "forge"}, {"version", "47.3.0"}}));
		// Each id once, a bad id left out, a version cut to what the server takes.
		QCOMPARE(payload.value("mods").toArray(),
				 (QJsonArray{QJsonObject{{"id", "create"}, {"version", "0.5.1.j"}},
							 QJsonObject{{"id", "jei"}, {"version", "15.2.0beta"}}}));
		QCOMPARE(payload.value("rules").toArray(), QJsonArray());
	}

	void vanillaSendsNoModsAndUnknownJavaIsNull()
	{
		Report report = sampleReport();
		report.facts.loader = QStringLiteral("vanilla");
		report.facts.javaNow = 0;
		const QJsonObject payload = CrashPayload::build(report, QStringLiteral("1.2.0"));
		QCOMPARE(payload.value("loader").toObject(), (QJsonObject{{"type", "vanilla"}}));
		QCOMPARE(payload.value("mods").toArray(), QJsonArray());
		QVERIFY(payload.value("java").isNull());
		QVERIFY(!payload.contains("culprit"));
	}

	void onlyUnknownCrashesOrFixesThatDidNotHelpAreSent()
	{
		Report report = sampleReport();
		QVERIFY(CrashPayload::sendable(report));
		Cause cause;
		cause.ruleId = QStringLiteral("mods.crashed");
		report.causes = {cause};
		QVERIFY(!CrashPayload::sendable(report));
		report.fixDidNotHelp = true;
		QVERIFY(CrashPayload::sendable(report));
		QCOMPARE(CrashPayload::build(report, QStringLiteral("1.2.0")).value("rules").toArray(),
				 QJsonArray{QStringLiteral("mods.crashed")});
		report.lines = {};
		QVERIFY(!CrashPayload::sendable(report));
	}

	void readableSaysWhatGoes()
	{
		const QVariantMap shown = CrashPayload::readable(CrashPayload::build(sampleReport(), QStringLiteral("1.2.0")));
		QCOMPARE(shown.value("versions").toString(), QStringLiteral("Kite 1.2.0 · macOS arm64 · Java 17"));
		QCOMPARE(shown.value("game").toString(), QStringLiteral("Minecraft 1.20.1 · Forge 47.3.0"));
		QCOMPARE(shown.value("mods").toString(), QStringLiteral("create 0.5.1.j, jei 15.2.0beta"));
		QCOMPARE(shown.value("modCount").toInt(), 2);
		QCOMPARE(shown.value("culprit").toString(), QStringLiteral("create"));
		QCOMPARE(shown.value("lines").toStringList(), sampleReport().lines.lines);
	}

	void outboxOutcomes()
	{
		QCOMPARE(CrashOutbox::outcome(202), CrashOutbox::Outcome::Sent);
		QCOMPARE(CrashOutbox::outcome(400), CrashOutbox::Outcome::Drop);
		QCOMPARE(CrashOutbox::outcome(413), CrashOutbox::Outcome::Drop);
		QCOMPARE(CrashOutbox::outcome(429), CrashOutbox::Outcome::Retry);
		QCOMPARE(CrashOutbox::outcome(408), CrashOutbox::Outcome::Retry);
		QCOMPARE(CrashOutbox::outcome(503), CrashOutbox::Outcome::Retry);
		QCOMPARE(CrashOutbox::outcome(0), CrashOutbox::Outcome::Retry);
	}

	void outboxKeepsWhatCouldNotGo()
	{
		QTemporaryDir dir;
		{
			// No server: it waits, and outlives the launcher.
			CrashOutbox outbox(dir.path(), [] { return QString(); });
			outbox.send(payloadWith(QStringLiteral("a")));
			outbox.send(payloadWith(QStringLiteral("a")));
			QCOMPARE(outbox.waiting(), 1);
			QCOMPARE(outbox.state(QStringLiteral("a")), CrashOutbox::State::Queued);
		}
		CrashOutbox again(dir.path(), [] { return QStringLiteral("http://127.0.0.1:1"); });
		QCOMPARE(again.waiting(), 1);
		// No network here (no launcher): each try fails and counts, up to the cap.
		for (int i = 0; i < CrashOutbox::kMaxAttempts; ++i) {
			again.retry();
		}
		QCOMPARE(again.waiting(), 0);
		QCOMPARE(again.state(QStringLiteral("a")), CrashOutbox::State::None);
	}

	void outboxHoldsAFewAtMost()
	{
		QTemporaryDir dir;
		CrashOutbox outbox(dir.path(), [] { return QString(); });
		for (int i = 0; i < CrashOutbox::kMaxQueued + 5; ++i) {
			outbox.send(payloadWith(QString::number(i)));
		}
		QCOMPARE(outbox.waiting(), CrashOutbox::kMaxQueued);
		QCOMPARE(outbox.state(QStringLiteral("0")), CrashOutbox::State::None);
		QCOMPARE(outbox.state(QString::number(CrashOutbox::kMaxQueued + 4)), CrashOutbox::State::Queued);
	}

	void watchListKeepsAFewPerInstance()
	{
		const QDateTime now = QDateTime::currentDateTimeUtc();
		QList<CrashWatch::Entry> list;
		for (int i = 0; i < CrashWatch::kMaxPerInstance + 2; ++i) {
			list = CrashWatch::added(list, {QStringLiteral("skies"), QString::number(i), now.addSecs(i), {}, 1, {}});
		}
		list = CrashWatch::added(list, {QStringLiteral("atm"), QStringLiteral("x"), now, {}, 1, {}});
		QCOMPARE(list.size(), CrashWatch::kMaxPerInstance + 1);
		QCOMPARE(list.first().signature, QString::number(CrashWatch::kMaxPerInstance + 1));
		// The same crash again replaces itself.
		list = CrashWatch::added(list, {QStringLiteral("atm"), QStringLiteral("x"), now.addSecs(99), {}, 2, {}});
		QCOMPARE(list.size(), CrashWatch::kMaxPerInstance + 1);
		QCOMPARE(list.first().catalogVersion, 2);
	}

	void watchListForgetsOldAndGoneCrashes()
	{
		QTemporaryDir dir;
		const QString report = dir.filePath(QStringLiteral("crash.txt"));
		QFile file(report);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.close();
		const QDateTime now = QDateTime::currentDateTimeUtc();
		const QList<CrashWatch::Entry> list{
			{QStringLiteral("a"), QStringLiteral("1"), now, {report, {}, {}}, 1, {}},
			{QStringLiteral("b"), QStringLiteral("2"), now, {dir.filePath(QStringLiteral("gone.txt")), {}, {}}, 1, {}},
			{QStringLiteral("c"), QStringLiteral("3"), now.addDays(-CrashWatch::kKeepDays - 1), {report, {}, {}}, 1, {}},
		};
		const auto kept = CrashWatch::pruned(list, now, [](const QString& path) { return QFileInfo::exists(path); });
		QCOMPARE(kept.size(), 1);
		QCOMPARE(kept.first().instanceId, QStringLiteral("a"));

		const QString path = dir.filePath(QStringLiteral("unresolved.json"));
		QVERIFY(CrashWatch::save(path, list));
		const auto loaded = CrashWatch::load(path);
		QCOMPARE(loaded.size(), 3);
		QCOMPARE(loaded.first().files.crashReport, report);
		QCOMPARE(loaded.first().crashedAt.toString(Qt::ISODate), now.toString(Qt::ISODate));
	}

	void watchListDropsWhatIsDoneWith()
	{
		const QDateTime now = QDateTime::currentDateTimeUtc();
		const QList<CrashWatch::Entry> list{{QStringLiteral("a"), QStringLiteral("1"), now, {}, 1, {}},
											{QStringLiteral("a"), QStringLiteral("2"), now, {}, 1, QStringLiteral("mods.crashed")},
											{QStringLiteral("b"), QStringLiteral("3"), now, {}, 1, {}}};
		QCOMPARE(CrashWatch::without(list, QStringLiteral("a"), true).size(), 2);
		QCOMPARE(CrashWatch::without(list, QStringLiteral("a"), false).size(), 1);
	}

	void fixFoundSaysWhichDay()
	{
		const QDateTime now(QDate(2026, 11, 4), QTime(12, 0));
		const auto text = [&now](int daysAgo) {
			return CrashFollowUp::fixFoundText(QStringLiteral("Skies"), now.addDays(-daysAgo), now, QStringLiteral("en"));
		};
		QCOMPARE(text(0), QStringLiteral("Today's Skies crash has a fix now."));
		QCOMPARE(text(1), QStringLiteral("Yesterday's Skies crash has a fix now."));
		// 2026-11-03 is a Tuesday, 11-01 a Sunday.
		QCOMPARE(text(3), QStringLiteral("Last Sunday's Skies crash has a fix now."));
		QCOMPARE(text(9), QStringLiteral("The Skies crash of 26 October has a fix now."));
		QCOMPARE(CrashFollowUp::fixFoundText(QStringLiteral("Skies"), now.addDays(-2), now, QStringLiteral("tr")),
				 QStringLiteral("Last pazartesi's Skies crash has a fix now."));
	}
};

QTEST_GUILESS_MAIN(CrashReportsTest)

#include "CrashReports_test.moc"
