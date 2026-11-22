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
	/* Every motion token resolves from the style: playful is the springy
	 * default, calm is the earlier quiet set with no overshoot, off zeroes
	 * every duration. An unknown style reads as playful. */
	void motionTokensFollowTheStyle()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(QByteArrayLiteral(
							   "import QtQuick\n"
							   "import Kite.Theme\n"
							   "QtObject {\n"
							   "    property QtObject themeRef: Theme\n"
							   "    property QtObject motion: Theme.motion\n"
							   "    property int outCubic: Easing.OutCubic\n"
							   "    property int outBack: Easing.OutBack\n"
							   "    function tame(v, target, travel) { return Theme.motion.tame(v, target, travel) }\n"
							   "}\n"),
						   QUrl(QStringLiteral("inline")));
		std::unique_ptr<QObject> root(component.create());
		QVERIFY2(root != nullptr, qPrintable(component.errorString()));
		QObject* theme = root->property("themeRef").value<QObject*>();
		QObject* motion = root->property("motion").value<QObject*>();
		QVERIFY(theme != nullptr && motion != nullptr);
		const int outCubic = root->property("outCubic").toInt();
		const int outBack = root->property("outBack").toInt();
		const auto integer = [motion](const char* name) { return motion->property(name).toInt(); };
		const auto real = [motion](const char* name) { return motion->property(name).toDouble(); };

		QCOMPARE(theme->property("motionStyle").toString(), QStringLiteral("playful"));
		QCOMPARE(motion->property("style").toString(), QStringLiteral("playful"));
		QCOMPARE(motion->property("reduced").toBool(), false);
		QCOMPARE(integer("fast"), 140);
		QCOMPARE(integer("normal"), 240);
		QCOMPARE(integer("slow"), 400);
		QCOMPARE(integer("press"), 100);
		QCOMPARE(integer("pop"), 240);
		QCOMPARE(integer("stagger"), 45);
		QCOMPARE(integer("staggerRows"), 12);
		QCOMPARE(integer("shift"), 18);
		QCOMPARE(integer("rise"), 14);
		QCOMPARE(integer("dialogRise"), 16);
		QCOMPARE(integer("lift"), 6);
		QCOMPARE(real("pressScale"), 0.93);
		QCOMPARE(real("pressScaleLarge"), 0.95);
		QCOMPARE(real("pressScaleLight"), 0.97);
		QCOMPARE(real("cardPressScale"), 0.985);
		QCOMPARE(real("popScale"), 0.88);
		QCOMPARE(real("appearScale"), 0.7);
		QCOMPARE(real("tickScale"), 0.5);
		// Opacity and colour never overshoot; transforms do.
		QCOMPARE(integer("easing"), outCubic);
		QCOMPARE(integer("popEasing"), outBack);
		QCOMPARE(real("popOvershoot"), 1.7);
		QCOMPARE(real("knobOvershoot"), 2.0);

		// A marker that came 300 px passes its row by slideOvershootMax at
		// most; on the way there, and over a short hop, it is left alone.
		const auto tame = [&root](double value, double target, double travel) {
			QVariant result;
			QMetaObject::invokeMethod(root.get(), "tame", Q_RETURN_ARG(QVariant, result),
									  Q_ARG(QVariant, value), Q_ARG(QVariant, target), Q_ARG(QVariant, travel));
			return result.toDouble();
		};
		const double cap = integer("slideOvershootMax");
		QCOMPARE(tame(250, 300, 300), 250.0);
		QVERIFY(tame(329, 300, 300) > 300.0);
		QVERIFY(tame(329, 300, 300) <= 300.0 + cap);
		QVERIFY(tame(-29, 0, -300) >= -cap);
		QCOMPARE(tame(42, 40, 20), 42.0);

		QVERIFY(theme->setProperty("motionStyle", QStringLiteral("calm")));
		QCOMPARE(motion->property("style").toString(), QStringLiteral("calm"));
		QCOMPARE(motion->property("reduced").toBool(), false);
		QCOMPARE(integer("fast"), 120);
		QCOMPARE(integer("normal"), 180);
		QCOMPARE(integer("slow"), 260);
		QCOMPARE(integer("press"), 80);
		QCOMPARE(integer("pop"), 120);
		QCOMPARE(integer("stagger"), 30);
		QCOMPARE(integer("shift"), 10);
		QCOMPARE(integer("rise"), 8);
		QCOMPARE(integer("dialogRise"), 10);
		QCOMPARE(integer("lift"), 3);
		QCOMPARE(real("pressScale"), 0.97);
		QCOMPARE(real("pressScaleLarge"), 0.97);
		QCOMPARE(real("cardPressScale"), 1.0);
		QCOMPARE(real("popScale"), 0.96);
		QCOMPARE(real("appearScale"), 0.85);
		QCOMPARE(real("tickScale"), 0.85);
		QCOMPARE(integer("easing"), outCubic);
		QCOMPARE(integer("popEasing"), outCubic);
		QCOMPARE(real("popOvershoot"), 0.0);
		QCOMPARE(real("knobOvershoot"), 0.0);
		// Nothing overshoots, so nothing is tamed.
		QCOMPARE(tame(330, 300, 300), 330.0);

		QVERIFY(theme->setProperty("motionStyle", QStringLiteral("off")));
		QCOMPARE(motion->property("reduced").toBool(), true);
		for (const char* duration : {"fast", "normal", "slow", "press", "pop", "stagger", "pulse"})
			QCOMPARE(integer(duration), 0);

		QVERIFY(theme->setProperty("motionStyle", QStringLiteral("nonsense")));
		QCOMPARE(motion->property("style").toString(), QStringLiteral("playful"));
		QCOMPARE(motion->property("reduced").toBool(), false);
		QCOMPARE(integer("slow"), 400);
	}

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

		// Motion starts in the playful style and runs at its token durations
		// until the Motion setting (Theme.motionStyle, set by SettingsStore)
		// is "off", which zeroes them all, live.
		QCOMPARE(root->property("motionFast").toInt(), 140);
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

		QVERIFY(themeRef->setProperty("motionStyle", QStringLiteral("off")));
		QCOMPARE(root->property("motionFast").toInt(), 0);
		QCOMPARE(root->property("motionPulse").toInt(), 0);
		QCOMPARE(root->property("motionReduced").toBool(), true);
		QVERIFY(themeRef->setProperty("motionStyle", QStringLiteral("playful")));
		QCOMPARE(root->property("motionFast").toInt(), 140);

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
