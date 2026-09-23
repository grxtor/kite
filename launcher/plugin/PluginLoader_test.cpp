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

#include "plugin/PluginLoader.h"
#include "plugin/PluginMetadata.h"

/*
 * PluginLoader::loadModule() dlopen()s a real shared-library file and reads
 * its mmco_module_info symbol -- there is no way to drive it from a unit
 * test without a genuine compiled .mmco fixture, and building one is a
 * CMake/toolchain concern that does not belong in a QtTest binary (no such
 * fixture exists anywhere in this tree today).
 *
 * What loadModule() delegates to for an ABI-mismatched module --
 * PluginLoader::classifyAbiMismatch() -- takes only plain values (the name
 * already captured from the module, the file path, and three ABI numbers)
 * and is exercised directly here instead. It is the piece the design review
 * cared about: given a name, does the rejected module keep its identity and
 * get a reason/message that says why.
 */
class PluginLoaderTest : public QObject
{
	Q_OBJECT

  private slots:
	/* A module built for an ABI below the host's floor is AbiTooOld, and
	 * its name, the ABI it was built for, and the host's supported range
	 * all end up in the message so the user (or a bug report) has
	 * everything needed without digging through logs. */
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

	/* Symmetric case: built for an ABI above what this launcher knows
	 * (a downgrade, or a module built for a future release) comes back
	 * as AbiTooNew rather than being lumped in with "too old". */
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

	/* mmco_module_info::name may be null/blank; loadModule() then passes
	 * an empty QString as moduleName. The message must still identify
	 * the module somehow, so it falls back to the file path. */
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
