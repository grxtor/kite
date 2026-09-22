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

#include <QtTest>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QStandardItemModel>
#include <QQmlEngine>
#include <QUrl>

#include <memory>

/*
 * Smoke test for the QML user interface module.
 *
 * This does not test any behaviour; it proves the toolchain is wired up: that
 * qt_add_qml_module ran, that the .qml files were compiled into the binary at
 * the resource prefix we expect, and that the engine can instantiate the root
 * component. Those are exactly the things that break silently -- a missing
 * module, a shifted resource prefix or a qmlcachegen failure all produce a
 * perfectly green build and an application that shows nothing.
 *
 * The URL is spelled out rather than using QQmlComponent::loadFromModule(),
 * which is Qt 6.5+; the project's floor is 6.4 (Debian 12 / Ubuntu 24.04 LTS).
 * qt_add_qml_module is given RESOURCE_PREFIX "/qt/qml" so this path is the
 * same on 6.4 as it is once QTP0001 is available.
 */
class QmlModuleTest : public QObject
{
	Q_OBJECT

  private slots:
	void rootComponentInstantiates()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));

		QQmlComponent component(
			&engine, QUrl(QStringLiteral("qrc:/qt/qml/MeshMC/Main.qml")));

		QVERIFY2(!component.isError(), qPrintable(component.errorString()));
		QVERIFY2(component.isReady(),
				 qPrintable(QStringLiteral("component not ready: %1")
								.arg(component.errorString())));

		/* The root requires the instance model; an empty stand-in is enough
		 * to prove the component loads, and a required property left unset
		 * would itself be a load error worth catching here. */
		QStandardItemModel instances;
		QObject selection;
		std::unique_ptr<QObject> root(component.createWithInitialProperties(
			{{QStringLiteral("instanceModel"),
			  QVariant::fromValue<QObject*>(&instances)},
			 {QStringLiteral("selection"),
			  QVariant::fromValue<QObject*>(&selection)}}));
		QVERIFY2(root != nullptr, qPrintable(component.errorString()));

		/* Guards against the component resolving to something default
		 * constructed: the property only exists on our Main.qml. */
		QCOMPARE(root->property("moduleName").toString(),
				 QStringLiteral("MeshMC"));
	}
};

int main(int argc, char* argv[])
{
	/* The test instantiates QML objects but never shows a window. Forcing the
	 * offscreen platform keeps it runnable on a headless CI runner without
	 * depending on the harness to set QT_QPA_PLATFORM for us. Qt Quick types
	 * need a QGuiApplication, so QTEST_GUILESS_MAIN (QCoreApplication) -- what
	 * the other tests in this tree use -- is not an option here. */
	qputenv("QT_QPA_PLATFORM", "offscreen");

	QGuiApplication app(argc, argv);
	QmlModuleTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "QmlModule_test.moc"
