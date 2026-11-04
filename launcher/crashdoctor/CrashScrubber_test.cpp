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

#include <QTest>

#include "crashdoctor/CrashScrubber.h"

using namespace CrashDoctor;

/*
 * One line of a crash report at a time: frames lose what changes from run to
 * run, messages lose what is personal (CrashScrubber). Whole crashes are in
 * CrashSignature_test.
 */
class CrashScrubberTest : public QObject
{
	Q_OBJECT

  private slots:
	void framesLoseWhatChangesFromRunToRun()
	{
		QString module;
		const Personal none;
		QCOMPARE(scrubFrame(QStringLiteral("\tat TRANSFORMER/create@0.5.1.j/com.simibubi.create.Foo.lambda$tick$12(Foo.java:88) ~[create.jar%2399!/:0.5.1.j] {re:mixin,pl:mixin:A}"),
							none, &module),
				 QStringLiteral("at com.simibubi.create.Foo.lambda$tick$<n>(Foo.java)"));
		QCOMPARE(module, QStringLiteral("create"));
		QCOMPARE(scrubFrame(QStringLiteral("\tat knot//net.minecraft.client.Minecraft.<init>(Minecraft.java:469)"), none, &module),
				 QStringLiteral("at net.minecraft.client.Minecraft.<init>(Minecraft.java)"));
		QCOMPARE(module, QString());
		QCOMPARE(scrubFrame(QStringLiteral("at net.minecraft.Foo$$Lambda/0x000001b8a1c4f0.run(Unknown Source)"), none),
				 QStringLiteral("at net.minecraft.Foo$$Lambda.run(Unknown Source)"));
		QCOMPARE(scrubFrame(QStringLiteral("at net.minecraft.Foo.handler$zza000$sodium$onTick(Foo.java:5)"), none),
				 QStringLiteral("at net.minecraft.Foo.handler$sodium$onTick(Foo.java)"));
		QCOMPARE(scrubFrame(QStringLiteral("at net.minecraft.Foo.mixinextras$bridge$load$17(Foo.java:3)"), none),
				 QStringLiteral("at net.minecraft.Foo.mixinextras$bridge$load$<n>(Foo.java)"));
		QCOMPARE(scrubFrame(QStringLiteral("at jdk.internal.reflect.NativeMethodAccessorImpl.invoke0(Native Method) ~[?:?] {}"), none),
				 QStringLiteral("at jdk.internal.reflect.NativeMethodAccessorImpl.invoke0(Native Method)"));
		QCOMPARE(scrubFrame(QStringLiteral("at net.minecraftforge.eventbus.ASMEventHandler_1234_Foo_onTick_TickEvent.invoke(.dynamic)"), none),
				 QStringLiteral("at net.minecraftforge.eventbus.ASMEventHandler_<n>_Foo_onTick_TickEvent.invoke(.dynamic)"));
	}

	void messagesLoseWhatIsPersonal()
	{
		const Personal steve({QStringLiteral("/home/steve")}, {QStringLiteral("steve"), QStringLiteral("Notch")});
		const auto scrub = [&steve](const char* text) { return scrubMessage(QString::fromUtf8(text), steve); };
		QCOMPARE(scrub("Failed to read /home/steve/.local/share/kite/instances/x/mods/sodium-0.5.jar"),
				 QStringLiteral("Failed to read <path>/sodium-0.5.jar"));
		QCOMPARE(scrub("Notch joined from 2001:db8::8a2e:370:7334 and fe80::1%en0"),
				 QStringLiteral("<user> joined from <ip> and <ip>"));
		QCOMPARE(scrub("Couldn't connect to play.example-smp.net:25565 (https://example.com/x?token=abc)"),
				 QStringLiteral("Couldn't connect to <host> (<url>)"));
		QCOMPARE(scrub("Mail steve.jobs@example.com or join mc.hypixel.net"),
				 QStringLiteral("Mail <email> or join <host>"));
		QCOMPARE(scrub("Index 5 out of bounds for length 3 at x=-120, seed 4815162342"),
				 QStringLiteral("Index <n> out of bounds for length <n> at x=-<n>, seed <n>"));
		QCOMPARE(scrub("class file version 65.0, needs 1.20.1 and net.minecraft.world.level.Level@6d06d69c"),
				 QStringLiteral("class file version 65.0, needs 1.20.1 and net.minecraft.world.level.Level@<hex>"));
		QCOMPARE(scrub("[12:34:56] [Render thread/ERROR]: token eyJhbGciOi.eyJzdWIiOi.c2lnbmF0dXJl, 0x7ffee3b0"),
				 QStringLiteral("[<time>] [Render thread/ERROR]: token <token>, <hex>"));
		QCOMPARE(scrub("C:\\Users\\steve\\AppData\\Roaming\\kite\\options.txt and \\\\server\\share\\x"),
				 QStringLiteral("<path>/options.txt and <path>"));
		QCOMPARE(scrub("bidi \u202Eevil\u202C\tand\x01 control"), QStringLiteral("bidi evil and control"));
		QCOMPARE(scrub(QString(400, QLatin1Char('x')).toUtf8().constData()).size(), kMaxReportLineLength);
		// A surrogate pair is never cut in half.
		const QString emoji = QString(kMaxReportLineLength - 1, QLatin1Char('x')) + QString::fromUtf8("😀😀");
		QCOMPARE(scrubMessage(emoji, Personal()).size(), kMaxReportLineLength - 1);
	}

	void shortNamesAreLeftAlone()
	{
		const Personal ab({}, {QStringLiteral("ab"), QStringLiteral("mod")});
		QCOMPARE(scrubMessage(QStringLiteral("ab broke a mod"), ab), QStringLiteral("ab broke a <user>"));
	}
};

QTEST_GUILESS_MAIN(CrashScrubberTest)

#include "CrashScrubber_test.moc"
