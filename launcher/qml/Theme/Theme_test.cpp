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
 * Smoke test for Kite.Theme, as QmlModule_test.cpp is for Kite: the module
 * resolves at its resource prefix, the singleton instantiates and its tokens
 * are the declared ones. The component comes from setData() because it only
 * needs to use the module, not live inside it.
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
							   "    property int motionFast: Theme.motion.fast\n"
							   "    property int motionPulse: Theme.motion.pulse\n"
							   "    property bool motionReduced: Theme.motion.reduced\n"
							   "    property color dangerInk: Theme.tone(\"danger\").ink\n"
							   "    property color dangerSoft: Theme.tone(\"danger\").soft\n"
							   "    property color unknownInk: Theme.tone(\"nonsense\").ink\n"
							   "    property int iconMid: Theme.icon.mid\n"
							   "    property int heroPixelSize: Theme.type.hero.pixelSize\n"
							   "    property color canvasColor: Theme.palette.canvas\n"
							   "    // Reaches the singletons from C++; singletonInstance() needs Qt 6.5.\n"
							   "    property QtObject themeRef: Theme\n"
							   "    property QtObject serviceRef: ThemeService\n"
							   "}\n"),
						   QUrl(QStringLiteral("inline")));

		QVERIFY2(!component.isError(), qPrintable(component.errorString()));

		std::unique_ptr<QObject> root(component.create());
		QVERIFY2(root != nullptr, qPrintable(component.errorString()));

		// The plain constants: exactly the values Theme.qml declares.
		QCOMPARE(root->property("spaceMd").toInt(), 12);
		// Body 15, subheading 20, heading 24, nothing a user reads under 13;
		// radii md 12 / lg 16; controls sm 32 / md 40 / lg 48.
		QCOMPARE(root->property("bodyPixelSize").toInt(), 15);
		QCOMPARE(root->property("headingPixelSize").toInt(), 24);
		QCOMPARE(root->property("subheadingPixelSize").toInt(), 20);
		QCOMPARE(root->property("captionPixelSize").toInt(), 13);
		QCOMPARE(root->property("radiusMd").toInt(), 12);
		QCOMPARE(root->property("radiusLg").toInt(), 16);
		QCOMPARE(root->property("controlHeightSm").toInt(), 32);
		QCOMPARE(root->property("controlHeight").toInt(), 40);
		QCOMPARE(root->property("controlHeightLg").toInt(), 48);

		// Motion runs at its token durations until the Reduce motion setting
		// (Theme.reduceMotion, set by SettingsStore) zeroes them all, live.
		QCOMPARE(root->property("motionFast").toInt(), 120);
		QCOMPARE(root->property("motionPulse").toInt(), 900);
		QCOMPARE(root->property("motionReduced").toBool(), false);
		QCOMPARE(root->property("iconMid").toInt(), 20);
		QCOMPARE(root->property("heroPixelSize").toInt(), 48);

		// The colour tokens come from ThemePalette for the active theme. The
		// launcher starts dark whatever the OS prefers, so this holds on a
		// light runner too. The scheme is named through forScheme() so a
		// changed default fails here instead of silently checking the wrong
		// palette.
		QCOMPARE(root->property("dark").toBool(), true);
		QCOMPARE(root->property("accent").value<QColor>(),
				 ThemePalette::forScheme(ThemePalette::Scheme::Amethyst, true).accent);
		QCOMPARE(root->property("surfaceHover").value<QColor>(),
				 ThemePalette::forScheme(ThemePalette::Scheme::Amethyst, true).surfaceHover);

		auto* service = qobject_cast<ThemeService*>(
			root->property("serviceRef").value<QObject*>());
		QVERIFY(service != nullptr);
		QObject* themeRef = root->property("themeRef").value<QObject*>();
		QVERIFY(themeRef != nullptr);

		const ThemePalette dark = ThemePalette::forScheme(ThemePalette::Scheme::Amethyst, true);
		QCOMPARE(root->property("dangerInk").value<QColor>(), dark.danger);
		QCOMPARE(root->property("dangerSoft").value<QColor>(), dark.dangerSubtle);
		QCOMPARE(root->property("unknownInk").value<QColor>(), dark.textSecondary);

		QVERIFY(themeRef->setProperty("reduceMotion", true));
		QCOMPARE(root->property("motionFast").toInt(), 0);
		QCOMPARE(root->property("motionPulse").toInt(), 0);
		QCOMPARE(root->property("motionReduced").toBool(), true);
		QVERIFY(themeRef->setProperty("reduceMotion", false));
		QCOMPARE(root->property("motionFast").toInt(), 120);

		// Switching mode replaces the whole theme as one unit: `changed` fires
		// once per switch, not once per property.
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

		// The scheme swaps independently of the mode, also as one unit; an
		// unknown name changes nothing.
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
	/* Qt Quick needs a QGuiApplication, and ThemeService reads
	 * QGuiApplication::palette(), so QTEST_GUILESS_MAIN is not an option.
	 * Offscreen keeps it runnable on a headless runner. */
	qputenv("QT_QPA_PLATFORM", "offscreen");

	QGuiApplication app(argc, argv);
	ThemeTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "Theme_test.moc"
