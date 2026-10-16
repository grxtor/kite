/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 Project Tick
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

#include "core/InstancePathProblems.h"

class InstancePathProblemsTest : public QObject
{
	Q_OBJECT

  private slots:
	void aPlainFolderHasNoProblem()
	{
		QVERIFY(instancePathProblems({ QStringLiteral("/home/steve/MeshMC/instances") },
									 QStringLiteral("/tmp"))
					.isEmpty());
	}

	void anExclamationMarkIsAProblem()
	{
		const auto problems = instancePathProblems(
			{ QStringLiteral("/home/steve/Games!/instances") }, QStringLiteral("/tmp"));
		QCOMPARE(problems.size(), 1);
		QCOMPARE(problems.at(0).folder, QStringLiteral("/home/steve/Games!/instances"));
		QVERIFY(problems.at(0).text.contains(QStringLiteral("Games!")));
		QVERIFY(!problems.at(0).advice.isEmpty());
	}

	void aTemporaryFolderIsAProblem()
	{
		const auto problems = instancePathProblems(
			{ QStringLiteral("/tmp/MeshMC/instances") }, QStringLiteral("/tmp"));
		QCOMPARE(problems.size(), 1);
		QVERIFY(problems.at(0).text.contains(QStringLiteral("/tmp")));
	}

	void anUnextractedArchiveIsAProblem()
	{
		const auto problems = instancePathProblems(
			{ QStringLiteral("/home/steve/Rar$EXa0.123/instances") },
			QStringLiteral("/tmp"));
		QCOMPARE(problems.size(), 1);
		QVERIFY(problems.at(0).text.contains(QStringLiteral("Rar$")));
	}

	// Every folder is checked, and each problem names its own folder.
	void everyFolderIsChecked()
	{
		const auto problems = instancePathProblems(
			{ QStringLiteral("/home/steve/ok"), QStringLiteral("/home/steve/a!b"),
			  QStringLiteral("/tmp/x") },
			QStringLiteral("/tmp"));
		QCOMPARE(problems.size(), 2);
		QCOMPARE(problems.at(0).folder, QStringLiteral("/home/steve/a!b"));
		QCOMPARE(problems.at(1).folder, QStringLiteral("/tmp/x"));
	}
};

QTEST_GUILESS_MAIN(InstancePathProblemsTest)

#include "InstancePathProblems_test.moc"
