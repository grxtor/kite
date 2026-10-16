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

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "models/ComponentTools.h"

class ComponentToolsTest : public QObject
{
	Q_OBJECT

  private slots:
	void suggestsAUidFromTheName_data();
	void suggestsAUidFromTheName();
	void refusesAMissingName();
	void refusesAUidTheInstanceAlreadyHas();
	void takesASuggestedUidWhenNoneIsGiven();
	void keepsOnlyExistingFiles();
	void doesNothingWithoutAnInstance();
};

void ComponentToolsTest::suggestsAUidFromTheName_data()
{
	QTest::addColumn<QString>("name");
	QTest::addColumn<QString>("uid");
	QTest::newRow("plain") << "Sodium" << "org.projecttick.custom.sodium";
	QTest::newRow("mixed") << "My Mod 2!" << "org.projecttick.custom.mymod";
	QTest::newRow("no letters") << "1234 !!" << "";
	QTest::newRow("empty") << "" << "";
}

void ComponentToolsTest::suggestsAUidFromTheName()
{
	QFETCH(QString, name);
	QFETCH(QString, uid);
	QCOMPARE(ComponentTools::suggestUid(name), uid);
}

void ComponentToolsTest::refusesAMissingName()
{
	QVERIFY(!ComponentTools::validateNewComponent("", "org.x.y", {}).isEmpty());
	QVERIFY(!ComponentTools::validateNewComponent("   ", "org.x.y", {}).isEmpty());
}

void ComponentToolsTest::refusesAUidTheInstanceAlreadyHas()
{
	const QStringList existing = {"net.minecraft", "net.fabricmc.fabric-loader"};
	QVERIFY(!ComponentTools::validateNewComponent("Fabric", "net.minecraft", existing)
				 .isEmpty());
	QVERIFY(ComponentTools::validateNewComponent("Mine", "org.example.mine", existing)
				.isEmpty());
}

void ComponentToolsTest::takesASuggestedUidWhenNoneIsGiven()
{
	// Fine while the suggestion is free ...
	QVERIFY(ComponentTools::validateNewComponent("Sodium", "", {}).isEmpty());
	// ... refused when it is not, or when a name gives nothing to suggest.
	QVERIFY(!ComponentTools::validateNewComponent(
				 "Sodium", "", {"org.projecttick.custom.sodium"})
				 .isEmpty());
	QVERIFY(!ComponentTools::validateNewComponent("123", "", {}).isEmpty());
}

void ComponentToolsTest::keepsOnlyExistingFiles()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	QFile file(dir.filePath("mod.jar"));
	QVERIFY(file.open(QIODevice::WriteOnly));
	file.close();
	const auto paths = ComponentTools::existingFiles(
		{QUrl::fromLocalFile(file.fileName()),
		 QUrl::fromLocalFile(dir.filePath("missing.jar")),
		 QUrl::fromLocalFile(dir.path()), QUrl("https://example.com/a.jar")});
	QCOMPARE(paths, QStringList{file.fileName()});
}

void ComponentToolsTest::doesNothingWithoutAnInstance()
{
	ComponentTools tools(nullptr);
	QVERIFY(tools.componentUids().isEmpty());
	QVERIFY(!tools.addEmpty("Sodium", "").isEmpty());
	QVERIFY(!tools.addJarMods({QUrl::fromLocalFile("/nope.jar")}));
	QVERIFY(!tools.error().isEmpty());
	QVERIFY(!tools.replaceMainJar(QUrl::fromLocalFile("/nope.jar")));
	QVERIFY(tools.downloadAll() == nullptr);
	QVERIFY(!tools.hasJsonFile(0));
	QVERIFY(!tools.editComponentJson(0));
}

QTEST_GUILESS_MAIN(ComponentToolsTest)
#include "ComponentTools_test.moc"
