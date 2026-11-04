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

// The last line a process prints without a newline still reaches the log, on
// the stream it came from: it is often the one that says why a game stopped.
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
};

QTEST_GUILESS_MAIN(LoggedProcessTest)

#include "LoggedProcess_test.moc"
