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

#include <QList>
#include <QPair>
#include <QTest>

#include "LoggedProcess.h"
#include "MessageLevel.h"
#include "ProcessOutput.h"

// The last line a process prints without a newline still reaches the log, on
// the stream it came from: it is often the one that says why a game stopped.
// What it prints is read as UTF-8, the system code page only for a line that
// is not UTF-8 (ProcessOutput).
class LoggedProcessTest : public QObject
{
	Q_OBJECT

  private:
	static constexpr int kExitTimeoutMs = 10000;
	using Logged = QList<QPair<QStringList, MessageLevel::Enum>>;

	// Every line the process logged at @p level, in order. MessageLevel::Enum
	// is no registered meta type, so no QSignalSpy.
	static QStringList linesAt(const Logged& logged, MessageLevel::Enum level)
	{
		QStringList lines;
		for (const auto& [batch, batchLevel] : logged) {
			if (batchLevel == level) {
				lines << batch;
			}
		}
		return lines;
	}

	static Logged runShell(const QString& script)
	{
		LoggedProcess process;
		Logged logged;
		connect(&process, &LoggedProcess::log,
				[&logged](QStringList lines, MessageLevel::Enum level) {
					logged.append({lines, level});
				});
		process.start(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), script});
		[&] {
			QTRY_VERIFY_WITH_TIMEOUT(
				process.state() == LoggedProcess::Finished, kExitTimeoutMs);
		}();
		return logged;
	}

  private slots:
	void unfinishedStdoutLineIsKept()
	{
#ifdef Q_OS_WIN
		QSKIP("Needs a POSIX shell");
#endif
		const Logged logged = runShell(QStringLiteral("printf 'first\\nlast words'"));
		QCOMPARE(linesAt(logged, MessageLevel::StdOut),
				 QStringList({QStringLiteral("first"), QStringLiteral("last words")}));
	}

	void unfinishedStderrLineStaysOnStderr()
	{
#ifdef Q_OS_WIN
		QSKIP("Needs a POSIX shell");
#endif
		const Logged logged =
			runShell(QStringLiteral("printf 'out\\n'; printf 'err tail' >&2"));
		QCOMPARE(linesAt(logged, MessageLevel::StdErr),
				 QStringList({QStringLiteral("err tail")}));
		QCOMPARE(linesAt(logged, MessageLevel::StdOut),
				 QStringList({QStringLiteral("out")}));
	}

	// A resource pack name with formatting codes, as Minecraft logs it: "§"
	// is C2 A7, which a Windows code page used to turn into "Â§".
	void gameOutputIsUtf8()
	{
#ifdef Q_OS_WIN
		QSKIP("Needs a POSIX shell");
#endif
		const Logged logged = runShell(QStringLiteral("printf 'pack \\302\\247dDan\\302\\247bdy\\n'"));
		QCOMPARE(linesAt(logged, MessageLevel::StdOut),
				 QStringList({QStringLiteral("pack \u00a7dDan\u00a7bdy")}));
	}

	void utf8BytesMakeOneCharacter()
	{
		ProcessOutput::LineDecoder decoder;
		QCOMPARE(decoder.feed(QByteArray("'file/!     \xc2\xa7" "dDan\xc2\xa7" "bdy 26.3.zip'\r\n")),
				 QStringList({QStringLiteral("'file/!     \u00a7dDan\u00a7bdy 26.3.zip'")}));
		// "Ç" (C3 87) cut between two reads.
		QVERIFY(decoder.feed(QByteArray("\xc3")).isEmpty());
		QCOMPARE(decoder.feed(QByteArray("\x87" "a\n\nb")),
				 QStringList({QStringLiteral("\u00c7a"), QString()}));
		QCOMPARE(decoder.finish(), QStringLiteral("b"));
		QCOMPARE(decoder.finish(), QString());
	}

	// A line that is not UTF-8 is read in the fallback: the system code page
	// in Kite (cp1254 on a Turkish Windows); Latin-1 here, which agrees with
	// cp1254 on "Ç" (C7) and on "Ã" (C3).
	void codePageLineFallsBack()
	{
		ProcessOutput::LineDecoder decoder(QStringConverter::Latin1);
		QCOMPARE(decoder.feed(QByteArray("C:\\Users\\\xc7" "an\nabc\xc3\n\xc3\x87" "a\n")),
				 QStringList({QStringLiteral("C:\\Users\\\u00c7an"), QStringLiteral("abc\u00c3"),
							  QStringLiteral("\u00c7a")}));

		const QByteArray codePage("Kullan\xfd\xfe\n");
		ProcessOutput::LineDecoder system;
		QCOMPARE(system.feed(codePage), QStringList({QString::fromLocal8Bit(codePage.chopped(1))}));
	}

	// stdout.encoding is Java 18's; older Javas read sun.stdout.encoding.
	void jvmArgumentsAskForUtf8()
	{
		const QStringList modern{"-Dstdout.encoding=UTF-8", "-Dstderr.encoding=UTF-8"};
		const QStringList legacy{"-Dsun.stdout.encoding=UTF-8", "-Dsun.stderr.encoding=UTF-8"};
		QCOMPARE(ProcessOutput::jvmArguments(8), legacy);
		QCOMPARE(ProcessOutput::jvmArguments(17), legacy);
		QCOMPARE(ProcessOutput::jvmArguments(18), modern);
		QCOMPARE(ProcessOutput::jvmArguments(25), modern);
		QCOMPARE(ProcessOutput::jvmArguments(0), modern + legacy);
	}
};

QTEST_GUILESS_MAIN(LoggedProcessTest)

#include "LoggedProcess_test.moc"
