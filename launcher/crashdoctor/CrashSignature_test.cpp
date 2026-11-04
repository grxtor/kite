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

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTest>

#include "crashdoctor/CrashScrubber.h"
#include "crashdoctor/CrashSignature.h"

using namespace CrashDoctor;

/*
 * The lines a crash report sends: the same crash on two computers gives the
 * same lines and signature, and nothing personal survives. The real crashes of
 * testdata/real are read as they are; the template below is filled with two
 * different users' details.
 */
namespace
{
	// What the server refuses (server/friends/src/crashReports.ts), so a line
	// that passes here passes there. Plain literals: moc misreads raw ones.
	const QList<QRegularExpression>& serverChecks()
	{
		static const QList<QRegularExpression> checks{
			QRegularExpression(QStringLiteral("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}"),
							   QRegularExpression::CaseInsensitiveOption),
			QRegularExpression(QStringLiteral("(?<![\\w.])\\d{1,3}(?:\\.\\d{1,3}){3}(?![\\w.])")),
			QRegularExpression(QStringLiteral("\\b(?:[0-9a-f]{1,4}:){5,7}[0-9a-f]{1,4}\\b"),
							   QRegularExpression::CaseInsensitiveOption),
			QRegularExpression(QStringLiteral("(?:^|[\\s\"'(=:[/])(?:/Users/|/home/|[A-Za-z]:[\\\\/](?:Users|Documents and Settings)[\\\\/])"),
							   QRegularExpression::CaseInsensitiveOption),
			QRegularExpression(QStringLiteral("[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\\.[A-Za-z0-9-]+)*\\.[A-Za-z]{2,}(?![\\w/(.-])")),
			QRegularExpression(QStringLiteral("[\\x{0}-\\x{1f}\\x{7f}-\\x{9f}\\x{200e}\\x{200f}\\x{202a}-\\x{202e}\\x{2066}-\\x{2069}]")),
		};
		return checks;
	}

	struct User {
		QString home;
		QString name;
		QString player;
		QString uuid;
		QString ip;
		QString time;
		QString lambda;
		QString hex;
		int line;
	};

	const User kAlice{QStringLiteral("/Users/alice"), QStringLiteral("alice"), QStringLiteral("AliceMC"),
					  QStringLiteral("069a79f4-44e9-4726-a5be-fca90e38aaf5"), QStringLiteral("192.168.1.20:25565"),
					  QStringLiteral("2026-11-04 12:34:56"), QStringLiteral("12"), QStringLiteral("0x00000008000c3e440"), 101};
	const User kBob{QStringLiteral("C:\\Users\\Bob Smith"), QStringLiteral("Bob Smith"), QStringLiteral("bobby_2010"),
					QStringLiteral("853C80EF-3C37-49FD-AA49-938B674ADAE6"), QStringLiteral("10.0.0.7:25570"),
					QStringLiteral("2026-10-28T08:01:02.123Z"), QStringLiteral("7"), QStringLiteral("0x1f2a"), 245};

	QString crashFor(const User& user)
	{
		const QString sep = user.home.contains(QLatin1Char('\\')) ? QStringLiteral("\\") : QStringLiteral("/");
		return QStringLiteral(
				   "---- Minecraft Crash Report ----\n"
				   "Time: %6\n"
				   "Description: Ticking entity\n\n"
				   "java.lang.IllegalStateException: Player %3 (%4) at %5 could not load %1%2Library%2kite%2instances%2My World%2config%2create-common.toml\n"
				   "\tat java.base/java.lang.Thread.run(Thread.java:%9)\n"
				   "Caused by: java.io.FileNotFoundException: %1%2.kite%2%3.json (No such file) at %6\n"
				   "\tat TRANSFORMER/create@0.5.1.j/com.simibubi.create.Foo.lambda$tick$%7(Foo.java:%9) ~[create-1.20.1-0.5.1.j.jar%23301!/:0.5.1.j] {re:mixin}\n"
				   "\tat net.minecraft.world.level.Level$$Lambda$%7/%8.accept(Unknown Source) ~[?:?]\n"
				   "\tat jdk.internal.reflect.GeneratedMethodAccessor%7.invoke(Unknown Source)\n"
				   "\tat com.simibubi.create.Bar.read(Bar.java:%9)\n"
				   "\t... 4 more\n\n"
				   "A detailed walkthrough of the error\n")
			.arg(user.home, sep, user.player, user.uuid, user.ip, user.time, user.lambda, user.hex,
				 QString::number(user.line));
	}

	Personal personalFor(const User& user)
	{
		return Personal({user.home}, {user.name, user.player});
	}

	QString readFile(const QString& path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
	}

	void verifyClean(const QStringList& lines, const QStringList& secrets)
	{
		QVERIFY(!lines.isEmpty());
		for (const QString& line : lines) {
			QVERIFY2(!line.isEmpty() && line.size() <= kMaxReportLineLength, qPrintable(line));
			for (const QRegularExpression& check : serverChecks()) {
				QVERIFY2(!check.match(line).hasMatch(), qPrintable(check.pattern() + QStringLiteral(" in ") + line));
			}
			for (const QString& secret : secrets) {
				QVERIFY2(!line.contains(secret, Qt::CaseInsensitive), qPrintable(secret + QStringLiteral(" in ") + line));
			}
		}
	}
} // namespace

class CrashSignatureTest : public QObject
{
	Q_OBJECT

  private slots:
	void sameCrashSameSignature()
	{
		const QSet<QString> installed{QStringLiteral("create")};
		const ReportLines alice = reportLines({QString(), crashFor(kAlice), QString()}, personalFor(kAlice), {}, installed);
		const ReportLines bob = reportLines({QString(), crashFor(kBob), QString()}, personalFor(kBob), {}, installed);
		QCOMPARE(alice.lines, bob.lines);
		QCOMPARE(signatureOf(alice.lines), signatureOf(bob.lines));
		QCOMPARE(alice.culprit, QStringLiteral("create"));
		QCOMPARE(alice.lines.value(0),
				 QStringLiteral("java.lang.IllegalStateException: Player <user> (<uuid>) at <ip> could not load <path>/create-common.toml"));
		QCOMPARE(alice.lines.value(1), QStringLiteral("java.io.FileNotFoundException: <path>/<user>.json (No such file) at <time>"));
		QCOMPARE(alice.lines.mid(2), (QStringList{
										 QStringLiteral("at com.simibubi.create.Foo.lambda$tick$<n>(Foo.java)"),
										 QStringLiteral("at net.minecraft.world.level.Level$$Lambda.accept(Unknown Source)"),
										 QStringLiteral("at jdk.internal.reflect.GeneratedMethodAccessor<n>.invoke(Unknown Source)"),
										 QStringLiteral("at com.simibubi.create.Bar.read(Bar.java)"),
									 }));
	}

	void nothingPersonalSurvives()
	{
		for (const User& user : {kAlice, kBob}) {
			const ReportLines out = reportLines({QString(), crashFor(user), QString()}, personalFor(user), {}, {});
			verifyClean(out.lines, {user.name, QStringLiteral("Smith"), user.player, user.uuid, user.ip.section(':', 0, 0),
									user.time, user.hex, QStringLiteral("My World"), QStringLiteral("Library")});
		}
	}

	void signatureIsSha256OfLines()
	{
		// printf 'a\nb' | shasum -a 256, the same as the server's crashSignature().
		QCOMPARE(signatureOf({QStringLiteral("a"), QStringLiteral("b")}),
				 QStringLiteral("7e18f737311b2dc3b2f269dd78396b0351f14fb66efa879f768cb23181883c78"));
		QCOMPARE(signatureOf({QString::fromUtf8("ç")}),
				 QStringLiteral("8bfa829b8119a6f39b91fd8decec63830b556e4d88a9da29334d7b0558829f2d"));
	}

	void jvmCrash()
	{
		const QString hsErr = QStringLiteral(
			"#\n# A fatal error has been detected by the Java Runtime Environment:\n#\n"
			"#  SIGSEGV (0xb) at pc=0x000000019a3b4c2d, pid=4242, tid=259\n#\n"
			"# Problematic frame:\n# C  [libglfw.dylib+0x1234]  _glfwPlatformPollEvents+0x40\n#\n"
			"# Core dump will be written. Default location: /Users/alice/core.4242\n\n"
			"Native frames: (J=compiled Java code, j=interpreted, Vv=VM code, C=native code)\n"
			"C  [libglfw.dylib+0x1234]  _glfwPlatformPollEvents+0x40\n"
			"j  org.lwjgl.glfw.GLFW.glfwPollEvents()V+0\n\n"
			"Java frames:\n");
		const ReportLines out = reportLines({QString(), QString(), hsErr}, Personal({QStringLiteral("/Users/alice")}, {}), {}, {});
		QCOMPARE(out.lines, (QStringList{QStringLiteral("SIGSEGV"),
										 QStringLiteral("C [libglfw.dylib+<hex>] _glfwPlatformPollEvents+<hex>"),
										 QStringLiteral("C [libglfw.dylib+<hex>] _glfwPlatformPollEvents+<hex>"),
										 QStringLiteral("j org.lwjgl.glfw.GLFW.glfwPollEvents()V+<n>")}));
	}

	void logWithoutTrace()
	{
		const QString log = QStringLiteral(
			"Java is version 21.0.4, using 64 (aarch64) architecture, from Eclipse Adoptium.\n"
			"Error occurred during initialization of VM\n"
			"Error: Could not create the Java Virtual Machine.\n"
			"Process exited with code 1.\n");
		const ReportLines out = reportLines({log, QString(), QString()}, Personal(), {}, {});
		QCOMPARE(out.lines, (QStringList{QStringLiteral("Error occurred during initialization of VM"),
										 QStringLiteral("Error: Could not create the Java Virtual Machine.")}));
		QVERIFY(reportLines({QStringLiteral("All fine\nProcess exited with code 0.\n"), {}, {}}, Personal(), {}, {}).lines.isEmpty());
	}

	void realCrashes_data()
	{
		QTest::addColumn<QString>("path");
		const QDir dir(QFileInfo(QFINDTESTDATA("testdata/expected.json")).absolutePath() + QStringLiteral("/real"));
		for (const QFileInfo& file : dir.entryInfoList(QDir::Files)) {
			if (!file.fileName().startsWith(QLatin1String("NEGATIVE"))) {
				QTest::newRow(qPrintable(file.fileName())) << file.absoluteFilePath();
			}
		}
	}

	void realCrashes()
	{
		QFETCH(QString, path);
		const QString text = readFile(path);
		const bool crashReport = path.endsWith(QLatin1String(".crash.txt"));
		const Evidence evidence{crashReport ? QString() : text, crashReport ? text : QString(), QString()};
		const ReportLines out = reportLines(evidence, Personal(), {}, {});
		verifyClean(out.lines, {});
		QVERIFY(out.lines.size() <= 2 * (1 + kMaxMessageLines) + kMaxReportFrames);
		QCOMPARE(signatureOf(out.lines).size(), 64);
	}

	void forgeCrashBlamesTheSuspectedMod()
	{
		const QString text = readFile(QFINDTESTDATA("testdata/real/forge-1.20.1-abstractmethod-lib-mismatch.crash.txt"));
		const ReportLines out = reportLines({QString(), text, QString()}, Personal(), {}, {QStringLiteral("cataclysm")});
		QCOMPARE(out.culprit, QStringLiteral("cataclysm"));
		QVERIFY(out.lines.value(0).startsWith(QLatin1String("java.lang.AbstractMethodError: Receiver class com.github.L_Ender.cataclysm.")));
		QCOMPARE(out.lines.value(1),
				 QStringLiteral("at com.github.L_Ender.lionfishapi.client.model.tools.BasicEntityModel.m_7695_(BasicEntityModel.java)"));
		QCOMPARE(out.lines.size(), 1 + kMaxReportFrames);
		// Not installed, not blamed.
		QCOMPARE(reportLines({QString(), text, QString()}, Personal(), {}, {}).culprit, QString());
	}

	void fabricCrashTakesTheRootCause()
	{
		const QString text = readFile(QFINDTESTDATA("testdata/real/fabric-26.3-entrypoint-nosuchfield.crash.txt"));
		const ReportLines out = reportLines({QString(), text, QString()}, Personal(), {},
											{QStringLiteral("server_waypoint")});
		QVERIFY(out.lines.value(0).startsWith(QLatin1String("java.lang.RuntimeException: Could not execute entrypoint stage 'client'")));
		QVERIFY(out.lines.value(1).startsWith(QLatin1String("java.lang.NoSuchFieldError: Class com.mojang.blaze3d.platform.InputConstants$Type")));
		QCOMPARE(out.lines.value(2),
				 QStringLiteral("at _959.server_waypoint.fabric.ServerWaypointFabricClient.onInitializeClient(ServerWaypointFabricClient.java)"));
		QCOMPARE(out.lines.size(), 4);
	}

	void forgeModLoadingTakesTheModsMessages()
	{
		const QString text = readFile(QFINDTESTDATA("testdata/real/forge-1.20.1-mcreator-nosuchmethod-init.crash.txt"));
		const ReportLines out = reportLines({QString(), text, QString()}, Personal(), {}, {QStringLiteral("floating_cargo")});
		QCOMPARE(out.lines, (QStringList{
								QStringLiteral("java.lang.Exception: Mod Loading has failed"),
								QStringLiteral("Failure message: Floating Cargo (floating_cargo) has failed to load correctly"),
								QStringLiteral("Exception message: java.lang.NoSuchMethodException: net.mcreator.floatingcargo.FloatingCargoMod.<init>()"),
							}));
		QCOMPARE(out.culprit, QStringLiteral("floating_cargo"));
		// Another mod's failure is another crash.
		const QString other = readFile(QFINDTESTDATA("testdata/real/forge-1.20.1-requires-create-version.crash.txt"));
		const ReportLines requires = reportLines({QString(), other, QString()}, Personal(), {}, {});
		QCOMPARE(requires.lines.value(1), QStringLiteral("Failure message: Mod createcasing requires create 0.5.1.h or above"));
		QCOMPARE(requires.lines.size(), 1 + kMaxModSections);
		QVERIFY(signatureOf(requires.lines) != signatureOf(out.lines));
	}

	void classHintsBlameAMod()
	{
		const QString text = QStringLiteral(
			"Description: x\n\njava.lang.NullPointerException\n"
			"\tat net.minecraft.client.Foo.bar(Foo.java:1)\n"
			"\tat me.jellysquid.mods.sodium.client.Render.draw(Render.java:2)\n");
		const QList<ClassHint> hints{{QStringLiteral("me.jellysquid.mods.sodium"), QStringLiteral("sodium"), QStringLiteral("Sodium")},
									 {QStringLiteral("net.minecraft"), QString(), QString()}};
		QCOMPARE(reportLines({QString(), text, QString()}, Personal(), hints, {QStringLiteral("sodium")}).culprit,
				 QStringLiteral("sodium"));
	}
};

QTEST_GUILESS_MAIN(CrashSignatureTest)

#include "CrashSignature_test.moc"
