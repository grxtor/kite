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

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QVector>

#include "plugin/PluginUiRenderer.h"

namespace
{
	struct Event {
		QString nodeId;
		QString event;
		QString valueJson;
	};
} // namespace

class PluginUiRendererTest : public QObject
{
	Q_OBJECT

  private slots:
	void test_buildTrayMenu()
	{
		const char* menuDoc =
			"{\n"
			"  \"type\": \"mmco-tray-menu/1\",\n"
			"  \"items\": [\n"
			"    { \"type\": \"button\", \"id\": \"open\", \"props\": { \"label\": \"Open Kite\" } },\n"
			"    { \"type\": \"separator\" },\n"
			"    { \"type\": \"section\", \"id\": \"launch\", \"props\": { \"title\": \"Launch instance\" },\n"
			"      \"children\": [\n"
			"        { \"type\": \"button\", \"id\": \"inst-1\", \"props\": { \"label\": \"My Instance\" } }\n"
			"      ] },\n"
			"    { \"type\": \"separator\" },\n"
			"    { \"type\": \"button\", \"id\": \"quit\", \"props\": { \"label\": \"Quit Kite\" } }\n"
			"  ]\n"
			"}\n";
		QVector<Event> events;
		auto sink = [&events](const QString& id, const QString& ev,
							  const QString& val) { events.append({id, ev, val}); };

		QMenu menu;
		QVERIFY(PluginUiRenderer::buildTrayMenu(&menu, QString::fromUtf8(menuDoc), sink));

		/* open, separator, "Launch instance" submenu, separator, quit. */
		const auto actions = menu.actions();
		QCOMPARE(actions.size(), 5);
		QCOMPARE(actions.at(0)->text(), QStringLiteral("Open Kite"));
		QVERIFY(actions.at(1)->isSeparator());
		QVERIFY(actions.at(2)->menu() != nullptr);
		QCOMPARE(actions.at(2)->menu()->title(), QStringLiteral("Launch instance"));
		QVERIFY(actions.at(3)->isSeparator());
		QCOMPARE(actions.at(4)->text(), QStringLiteral("Quit Kite"));

		actions.at(0)->trigger();
		QCOMPARE(events.size(), 1);
		QCOMPARE(events.first().nodeId, QStringLiteral("open"));
		QCOMPARE(events.first().event, QStringLiteral("click"));

		actions.at(2)->menu()->actions().first()->trigger();
		QCOMPARE(events.size(), 2);
		QCOMPARE(events.last().nodeId, QStringLiteral("inst-1"));
	}
};

int main(int argc, char* argv[])
{
	/* Needs a full QApplication (real QWidgets); defaults to the offscreen platform so the binary
	 * also runs outside ctest. */
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));

	QApplication app(argc, argv);
	PluginUiRendererTest testCase;
	return QTest::qExec(&testCase, argc, argv);
}

#include "PluginUiRenderer_test.moc"
