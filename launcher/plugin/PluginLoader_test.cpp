/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include "plugin/PluginLoader.h"
#include "plugin/PluginMetadata.h"

/* loadModule() dlopen()s a real .mmco and no such fixture exists in this tree, so the testable
 * piece is classifyAbiMismatch(): given a name, a path and three ABI numbers, does a rejected
 * module keep its identity and get a reason that says why. */
class PluginLoaderTest : public QObject
{
	Q_OBJECT

  private slots:
	/* The message carries the module name, the ABI it was built for and the host's supported range. */
	void test_tooOldCarriesNameAndRange()
	{
		QString detail;
		const PluginDisableReason reason = PluginLoader::classifyAbiMismatch(
			QStringLiteral("OldPlugin"), QStringLiteral("/plugins/OldPlugin.mmco"),
			/*builtForAbi=*/1, /*abiMin=*/5, /*abiMax=*/5, detail);

		QCOMPARE(reason, PluginDisableReason::AbiTooOld);
		QVERIFY(detail.contains(QStringLiteral("OldPlugin")));
		QVERIFY(detail.contains(QStringLiteral("1")));
		QVERIFY(detail.contains(QStringLiteral("5")));
	}

	/* Built for an ABI above what this launcher knows: AbiTooNew, not lumped in with too old. */
	void test_tooNewIsDistinguishedFromTooOld()
	{
		QString detail;
		const PluginDisableReason reason = PluginLoader::classifyAbiMismatch(
			QStringLiteral("FuturePlugin"),
			QStringLiteral("/plugins/FuturePlugin.mmco"),
			/*builtForAbi=*/9, /*abiMin=*/5, /*abiMax=*/5, detail);

		QCOMPARE(reason, PluginDisableReason::AbiTooNew);
		QVERIFY(detail.contains(QStringLiteral("FuturePlugin")));
		QVERIFY(detail.contains(QStringLiteral("9")));
	}

	/* A null or blank mmco_module_info::name reaches here as an empty moduleName; the message falls
	 * back to the file path. */
	void test_emptyNameFallsBackToPath()
	{
		QString detail;
		PluginLoader::classifyAbiMismatch(QString(),
										  QStringLiteral("/plugins/Nameless.mmco"),
										  1, 5, 5, detail);

		QVERIFY(detail.contains(QStringLiteral("/plugins/Nameless.mmco")));
	}
};

QTEST_GUILESS_MAIN(PluginLoaderTest)

#include "PluginLoader_test.moc"
