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

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QTreeWidget>
#include <QVector>

#include "plugin/PluginUiRenderer.h"

namespace
{
	/* A doc exercising every one of the 14 node types, nested so
	 * containers (column/row/section) get covered too. */
	const char* kFullDoc =
		"{\n"
		"  \"type\": \"mmco-ui/1\",\n"
		"  \"root\": {\n"
		"    \"type\": \"column\", \"id\": \"root\",\n"
		"    \"children\": [\n"
		"      { \"type\": \"heading\", \"id\": \"h1\", \"props\": { \"text\": \"Heading\" } },\n"
		"      { \"type\": \"text\", \"id\": \"t1\", \"props\": { \"text\": \"Plain text\" } },\n"
		"      { \"type\": \"text\", \"id\": \"t2\",\n"
		"        \"props\": { \"text\": \"See [link](wiki:slug)\", \"format\": \"markdown\" } },\n"
		"      { \"type\": \"separator\", \"id\": \"sep1\" },\n"
		"      { \"type\": \"progress\", \"id\": \"p1\", \"props\": { \"value\": 42 } },\n"
		"      { \"type\": \"button\", \"id\": \"btn1\",\n"
		"        \"props\": { \"label\": \"Click Me\", \"enabled\": true } },\n"
		"      { \"type\": \"toggle\", \"id\": \"tg1\",\n"
		"        \"props\": { \"label\": \"Enable thing\", \"value\": false } },\n"
		"      { \"type\": \"text_field\", \"id\": \"tf1\",\n"
		"        \"props\": { \"label\": \"Name\", \"value\": \"abc\" } },\n"
		"      { \"type\": \"number_field\", \"id\": \"nf1\",\n"
		"        \"props\": { \"label\": \"Count\", \"value\": 3, \"min\": 0, \"max\": 10, \"decimals\": 0 } },\n"
		"      { \"type\": \"choice\", \"id\": \"ch1\",\n"
		"        \"props\": { \"label\": \"Pick\", \"options\": [\"a\", \"b\", \"c\"], \"value\": \"b\" } },\n"
		"      { \"type\": \"list\", \"id\": \"lst1\",\n"
		"        \"props\": { \"columns\": [\"Col1\", \"Col2\"],\n"
		"                   \"rows\": [ { \"id\": \"r1\", \"cells\": [\"A\", \"B\"] } ] } },\n"
		"      { \"type\": \"link\", \"id\": \"lnk1\",\n"
		"        \"props\": { \"text\": \"Open\", \"href\": \"https://example.com\" } },\n"
		"      { \"type\": \"section\", \"id\": \"sec1\", \"props\": { \"title\": \"Group\" },\n"
		"        \"children\": [\n"
		"          { \"type\": \"row\", \"id\": \"row1\",\n"
		"            \"children\": [\n"
		"              { \"type\": \"button\", \"id\": \"btn2\", \"props\": { \"label\": \"Inner\" } }\n"
		"            ] }\n"
		"        ] }\n"
		"    ]\n"
		"  }\n"
		"}\n";

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
	/* Every node type produces the expected concrete widget class, with
	 * props applied (label text, checked state, value, enabled, list
	 * columns/rows). */
	void test_rendersEveryNodeType()
	{
		QVector<Event> events;
		auto sink = [&events](const QString& id, const QString& ev,
							  const QString& val) { events.append({id, ev, val}); };

		auto surface = PluginUiRenderer::build(QString::fromUtf8(kFullDoc), sink);
		QVERIFY(surface != nullptr);
		QWidget* root = surface->rootWidget();
		QVERIFY(root != nullptr);

		auto* heading = root->findChild<QLabel*>("h1");
		QVERIFY(heading);
		QCOMPARE(heading->text(), QStringLiteral("Heading"));

		auto* text1 = root->findChild<QLabel*>("t1");
		QVERIFY(text1);
		QCOMPARE(text1->textFormat(), Qt::PlainText);

		auto* text2 = root->findChild<QLabel*>("t2");
		QVERIFY(text2);
		QCOMPARE(text2->textFormat(), Qt::MarkdownText);

		QVERIFY(root->findChild<QFrame*>("sep1"));

		auto* progress = root->findChild<QProgressBar*>("p1");
		QVERIFY(progress);
		QCOMPARE(progress->value(), 42);

		auto* button = root->findChild<QPushButton*>("btn1");
		QVERIFY(button);
		QCOMPARE(button->text(), QStringLiteral("Click Me"));
		QVERIFY(button->isEnabled());

		auto* toggle = root->findChild<QCheckBox*>("tg1");
		QVERIFY(toggle);
		QCOMPARE(toggle->text(), QStringLiteral("Enable thing"));
		QVERIFY(!toggle->isChecked());

		auto* textField = root->findChild<QLineEdit*>("tf1");
		QVERIFY(textField);
		QCOMPARE(textField->text(), QStringLiteral("abc"));

		auto* numberField = root->findChild<QDoubleSpinBox*>("nf1");
		QVERIFY(numberField);
		QCOMPARE(numberField->value(), 3.0);

		auto* choice = root->findChild<QComboBox*>("ch1");
		QVERIFY(choice);
		QCOMPARE(choice->currentData().toString(), QStringLiteral("b"));

		auto* list = root->findChild<QTreeWidget*>("lst1");
		QVERIFY(list);
		QCOMPARE(list->topLevelItemCount(), 1);
		QCOMPARE(list->topLevelItem(0)->text(0), QStringLiteral("A"));
		QCOMPARE(list->topLevelItem(0)->text(1), QStringLiteral("B"));

		QVERIFY(root->findChild<QLabel*>("lnk1"));
		QVERIFY(root->findChild<QGroupBox*>("sec1"));
		QVERIFY(root->findChild<QPushButton*>("btn2"));

		QCOMPARE(surface->nodeType("tg1"), QStringLiteral("toggle"));
		QCOMPARE(surface->nodeType("nosuchnode"), QString());
	}

	/* ui_surface_set (PluginUiRenderer::setNodeProps) patches a single
	 * node's props in place without disturbing the rest of the tree. */
	void test_setNodePropsPatchesInPlace()
	{
		auto surface = PluginUiRenderer::build(QString::fromUtf8(kFullDoc),
											   PluginUiRenderer::EventSink());
		auto* toggle = surface->rootWidget()->findChild<QCheckBox*>("tg1");
		QVERIFY(toggle);
		QVERIFY(!toggle->isChecked());

		QJsonObject patch;
		patch["value"] = true;
		QVERIFY(surface->setNodeProps("tg1", patch));
		QVERIFY(toggle->isChecked());

		/* The button next to it is untouched. */
		auto* button = surface->rootWidget()->findChild<QPushButton*>("btn1");
		QCOMPARE(button->text(), QStringLiteral("Click Me"));

		QJsonObject btnPatch;
		btnPatch["enabled"] = false;
		QVERIFY(surface->setNodeProps("btn1", btnPatch));
		QVERIFY(!button->isEnabled());

		/* Unknown node id fails cleanly. */
		QVERIFY(!surface->setNodeProps("does-not-exist", patch));
	}

	/* ui_surface_set_rows (PluginUiRenderer::setRows) replaces a list's
	 * rows without touching its columns. */
	void test_setRowsReplacesListContent()
	{
		auto surface = PluginUiRenderer::build(QString::fromUtf8(kFullDoc),
											   PluginUiRenderer::EventSink());
		auto* list = surface->rootWidget()->findChild<QTreeWidget*>("lst1");
		QVERIFY(list);
		QCOMPARE(list->topLevelItemCount(), 1);
		QCOMPARE(list->headerItem()->text(0), QStringLiteral("Col1"));

		QJsonArray rows;
		QJsonObject r1;
		r1["id"] = "x";
		r1["cells"] = QJsonArray{"X1", "X2"};
		QJsonObject r2;
		r2["id"] = "y";
		r2["cells"] = QJsonArray{"Y1", "Y2"};
		rows.append(r1);
		rows.append(r2);

		QVERIFY(surface->setRows("lst1", rows));
		QCOMPARE(list->topLevelItemCount(), 2);
		QCOMPARE(list->topLevelItem(1)->text(0), QStringLiteral("Y1"));
		/* Columns untouched by setRows(). */
		QCOMPARE(list->headerItem()->text(0), QStringLiteral("Col1"));

		/* Wrong node type refuses. */
		QVERIFY(!surface->setRows("btn1", rows));
	}

	/* A button click and a toggle flip each fire exactly the event the
	 * ABI promises: event="click"/"change", node_id matching the node,
	 * value_json carrying the new value for "change". */
	void test_buttonAndToggleEmitExpectedEvents()
	{
		QVector<Event> events;
		auto sink = [&events](const QString& id, const QString& ev,
							  const QString& val) { events.append({id, ev, val}); };
		auto surface = PluginUiRenderer::build(QString::fromUtf8(kFullDoc), sink);

		auto* button = surface->rootWidget()->findChild<QPushButton*>("btn1");
		QVERIFY(button);
		button->click();

		QVERIFY(!events.isEmpty());
		QCOMPARE(events.last().nodeId, QStringLiteral("btn1"));
		QCOMPARE(events.last().event, QStringLiteral("click"));

		events.clear();
		auto* toggle = surface->rootWidget()->findChild<QCheckBox*>("tg1");
		QVERIFY(toggle);
		toggle->click(); /* flips false -> true and emits toggled(true) */

		QCOMPARE(events.size(), 1);
		QCOMPARE(events.first().nodeId, QStringLiteral("tg1"));
		QCOMPARE(events.first().event, QStringLiteral("change"));
		QCOMPARE(events.first().valueJson, QStringLiteral("true"));
	}

	/* The declarative tray-menu doc (button/separator/section) builds a
	 * real QMenu and wires clicks through the same event sink shape. */
	void test_buildTrayMenu()
	{
		const char* menuDoc =
			"{\n"
			"  \"type\": \"mmco-tray-menu/1\",\n"
			"  \"items\": [\n"
			"    { \"type\": \"button\", \"id\": \"open\", \"props\": { \"label\": \"Open MeshMC\" } },\n"
			"    { \"type\": \"separator\" },\n"
			"    { \"type\": \"section\", \"id\": \"launch\", \"props\": { \"title\": \"Launch instance\" },\n"
			"      \"children\": [\n"
			"        { \"type\": \"button\", \"id\": \"inst-1\", \"props\": { \"label\": \"My Instance\" } }\n"
			"      ] },\n"
			"    { \"type\": \"separator\" },\n"
			"    { \"type\": \"button\", \"id\": \"quit\", \"props\": { \"label\": \"Quit MeshMC\" } }\n"
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
		QCOMPARE(actions.at(0)->text(), QStringLiteral("Open MeshMC"));
		QVERIFY(actions.at(1)->isSeparator());
		QVERIFY(actions.at(2)->menu() != nullptr);
		QCOMPARE(actions.at(2)->menu()->title(), QStringLiteral("Launch instance"));
		QVERIFY(actions.at(3)->isSeparator());
		QCOMPARE(actions.at(4)->text(), QStringLiteral("Quit MeshMC"));

		actions.at(0)->trigger();
		QCOMPARE(events.size(), 1);
		QCOMPARE(events.first().nodeId, QStringLiteral("open"));
		QCOMPARE(events.first().event, QStringLiteral("click"));

		/* The nested submenu's own action fires with its own id. */
		actions.at(2)->menu()->actions().first()->trigger();
		QCOMPARE(events.size(), 2);
		QCOMPARE(events.last().nodeId, QStringLiteral("inst-1"));
	}
};

int main(int argc, char* argv[])
{
	/* PluginUiRenderer builds real QWidgets, so this test needs a full
	 * QApplication (not QGuiApplication). Default to the offscreen
	 * platform plugin when the environment hasn't already picked one,
	 * so the binary also runs standalone outside `ctest`. */
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));

	QApplication app(argc, argv);
	PluginUiRendererTest testCase;
	return QTest::qExec(&testCase, argc, argv);
}

#include "PluginUiRenderer_test.moc"
