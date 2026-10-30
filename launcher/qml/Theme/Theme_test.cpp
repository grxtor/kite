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
 * Smoke test for Kite.Theme, the same job QmlModule_test.cpp does for the
 * Kite module: proves the module resolves at its resource prefix, the
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
							   "import Kite.Theme\n"
							   "QtObject {\n"
							   "    property color accent: Theme.palette.accent\n"
							   "    property int spaceMd: Theme.space.md\n"
							   "    property int bodyPixelSize: Theme.type.body.pixelSize\n"
							   "    property int headingPixelSize: Theme.type.heading.pixelSize\n"
							   "    property int subheadingPixelSize: Theme.type.subheading.pixelSize\n"
							   "    property int captionPixelSize: Theme.type.caption.pixelSize\n"
							   "    property int radiusMd: Theme.radius.md\n"
							   "    property int radiusLg: Theme.radius.lg\n"
							   "    property int controlHeightSm: Theme.control.heightSm\n"
							   "    property int controlHeight: Theme.control.height\n"
							   "    property int controlHeightLg: Theme.control.heightLg\n"
							   "    property color surfaceHover: Theme.palette.surfaceHover\n"
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
		// The phase-3 Modrinth scale (designs/phase3-scale.md): body 15,
		// subheading 20, heading 24, nothing a user reads under 13; radii sm 8 / md 12 /
		// lg 16 / xl 20, controls sm 32 / md 40 / lg 48.
		QCOMPARE(root->property("bodyPixelSize").toInt(), 15);
		QCOMPARE(root->property("headingPixelSize").toInt(), 24);
		QCOMPARE(root->property("subheadingPixelSize").toInt(), 20);
		QCOMPARE(root->property("captionPixelSize").toInt(), 13);
		QCOMPARE(root->property("radiusMd").toInt(), 12);
		QCOMPARE(root->property("radiusLg").toInt(), 16);
		QCOMPARE(root->property("controlHeightSm").toInt(), 32);
		QCOMPARE(root->property("controlHeight").toInt(), 40);
		QCOMPARE(root->property("controlHeightLg").toInt(), 48);

		// The colour tokens: not merely "some colour", but the one
		// ThemePalette hands out for whichever theme is active -- proving
		// the token came from ThemePalette rather than a value made up here.
		//
		// The launcher starts dark whatever the OS prefers -- that is the
		// documented default -- so this holds on a light test runner too.
		// A fresh ThemeService also starts on the Amethyst scheme (see its
		// own m_scheme default) -- named explicitly through forScheme()
		// rather than meshDark(), so a future default-scheme change fails
		// loudly here instead of this assertion silently checking the wrong
		// palette.
		QCOMPARE(root->property("dark").toBool(), true);
		QCOMPARE(root->property("accent").value<QColor>(),
				 ThemePalette::forScheme(ThemePalette::Scheme::Amethyst, true).accent);
		// surfaceHover is the newest palette role; it reaches QML too.
		QCOMPARE(root->property("surfaceHover").value<QColor>(),
				 ThemePalette::forScheme(ThemePalette::Scheme::Amethyst, true).surfaceHover);

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
				 ThemePalette::forScheme(ThemePalette::Scheme::Amethyst, false).canvas);

		QSignalSpy toDark(service, &ThemeService::changed);
		themeRef->setProperty("mode", QStringLiteral("dark"));
		QCOMPARE(toDark.count(), 1);
		QCOMPARE(root->property("dark").toBool(), true);
		QCOMPARE(root->property("canvasColor").value<QColor>(),
				 ThemePalette::forScheme(ThemePalette::Scheme::Amethyst, true).canvas);

		// The colour scheme swaps independently of the mode, also as one
		// announced unit; an unknown name changes nothing.
		QSignalSpy toEmber(service, &ThemeService::changed);
		themeRef->setProperty("scheme", QStringLiteral("ember"));
		QCOMPARE(toEmber.count(), 1);
		QCOMPARE(root->property("accent").value<QColor>(),
				 ThemePalette::forScheme(ThemePalette::Scheme::Ember, true).accent);
		QCOMPARE(root->property("dark").toBool(), true);

		QSignalSpy ignored(service, &ThemeService::changed);
		service->setScheme(QStringLiteral("nonsense"));
		QCOMPARE(ignored.count(), 0);
		QCOMPARE(service->scheme(), QStringLiteral("ember"));
		QCOMPARE(service->previewPalette(QStringLiteral("diamond")).accent,
				 ThemePalette::forScheme(ThemePalette::Scheme::Diamond, true).accent);

		// A look brings its own dark palette whatever the mode, and going
		// back to "classic" restores the scheme and mode it left; an unknown
		// look changes nothing.
		themeRef->setProperty("mode", QStringLiteral("light"));
		QSignalSpy toNight(service, &ThemeService::changed);
		themeRef->setProperty("look", QStringLiteral("night"));
		QCOMPARE(toNight.count(), 1);
		QCOMPARE(root->property("dark").toBool(), true);
		QCOMPARE(root->property("canvasColor").value<QColor>(),
				 ThemePalette::forScheme(ThemePalette::Scheme::Night, true).canvas);

		QSignalSpy unknownLook(service, &ThemeService::changed);
		service->setLook(QStringLiteral("nonsense"));
		QCOMPARE(unknownLook.count(), 0);
		QCOMPARE(service->look(), QStringLiteral("night"));

		themeRef->setProperty("look", QStringLiteral("classic"));
		QCOMPARE(root->property("dark").toBool(), false);
		QCOMPARE(root->property("accent").value<QColor>(),
				 ThemePalette::forScheme(ThemePalette::Scheme::Ember, false).accent);
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
