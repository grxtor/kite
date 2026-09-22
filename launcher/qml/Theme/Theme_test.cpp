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

#include "qml/Theme/ThemeService.h"
#include "theme/ThemePalette.h"

#include <QtTest>
#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QSignalSpy>
#include <QUrl>

#include <memory>

/*
 * Smoke test for MeshMC.Theme, the same job QmlModule_test.cpp does for the
 * MeshMC module: proves the module resolves at its resource prefix, the
 * singleton instantiates, and the tokens it hands out are the ones this
 * module declares -- not that any UI built on top of them looks right.
 *
 * The component is supplied via setData() rather than a loadFromModule()/qrc
 * URL, because it only needs to *use* the module, not live inside it; a
 * plain QUrl base is enough for relative resolution and needs no resource
 * entry of its own.
 */
class ThemeTest : public QObject
{
	Q_OBJECT

  private slots:
	void tokensAndModeSwitch()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));

		QQmlComponent component(&engine);
		component.setData(QByteArrayLiteral(
							   "import QtQuick\n"
							   "import MeshMC.Theme\n"
							   "QtObject {\n"
							   "    property color accent: Theme.palette.accent\n"
							   "    property int spaceMd: Theme.space.md\n"
							   "    property int bodyPixelSize: Theme.type.body.pixelSize\n"
							   "    property int radiusMd: Theme.radius.md\n"
							   "    property bool dark: Theme.dark\n"
							   "    property color canvasColor: Theme.palette.canvas\n"
							   "    // Exposed so the test can reach the singletons from C++\n"
							   "    // without QQmlEngine::singletonInstance(uri, typeName),\n"
							   "    // which is Qt 6.5+ and this project's floor is 6.4.\n"
							   "    property QtObject themeRef: Theme\n"
							   "    property QtObject serviceRef: ThemeService\n"
							   "}\n"),
						   QUrl(QStringLiteral("inline")));

		QVERIFY2(!component.isError(), qPrintable(component.errorString()));

		std::unique_ptr<QObject> root(component.create());
		QVERIFY2(root != nullptr, qPrintable(component.errorString()));

		// The plain constants: exactly the values Theme.qml declares.
		QCOMPARE(root->property("spaceMd").toInt(), 12);
		QCOMPARE(root->property("bodyPixelSize").toInt(), 14);
		QCOMPARE(root->property("radiusMd").toInt(), 8);

		// The colour tokens: not merely "some colour", but the one
		// ThemePalette hands out for whichever theme is active -- proving
		// the token came from ThemePalette rather than a value made up here.
		const bool systemDark = QGuiApplication::palette()
									 .color(QPalette::Window)
									 .lightnessF() < 0.5;
		const ThemePalette systemPalette =
			systemDark ? ThemePalette::meshDark() : ThemePalette::meshLight();
		QCOMPARE(root->property("dark").toBool(), systemDark);
		QCOMPARE(root->property("accent").value<QColor>(), systemPalette.accent);

		auto* service = qobject_cast<ThemeService*>(
			root->property("serviceRef").value<QObject*>());
		QVERIFY(service != nullptr);
		QObject* themeRef = root->property("themeRef").value<QObject*>();
		QVERIFY(themeRef != nullptr);

		// Switching mode replaces the whole theme as one unit: `dark` and
		// `palette` land on the new theme together, and `changed` fires
		// exactly once per switch, never once per property.
		QSignalSpy toLight(service, &ThemeService::changed);
		themeRef->setProperty("mode", QStringLiteral("light"));
		QCOMPARE(toLight.count(), 1);
		QCOMPARE(root->property("dark").toBool(), false);
		QCOMPARE(root->property("canvasColor").value<QColor>(),
				 ThemePalette::meshLight().canvas);

		QSignalSpy toDark(service, &ThemeService::changed);
		themeRef->setProperty("mode", QStringLiteral("dark"));
		QCOMPARE(toDark.count(), 1);
		QCOMPARE(root->property("dark").toBool(), true);
		QCOMPARE(root->property("canvasColor").value<QColor>(),
				 ThemePalette::meshDark().canvas);
	}
};

int main(int argc, char* argv[])
{
	/* Qt Quick types need a QGuiApplication, and ThemeService itself reads
	 * QGuiApplication::palette() -- QTEST_GUILESS_MAIN (QCoreApplication),
	 * what most other tests in this tree use, is not an option here. Forcing
	 * the offscreen platform keeps it runnable on a headless CI runner
	 * without depending on the harness to set QT_QPA_PLATFORM for us. */
	qputenv("QT_QPA_PLATFORM", "offscreen");

	QGuiApplication app(argc, argv);
	ThemeTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "Theme_test.moc"
