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

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "models/SettingsAdapter.h"
#include "settings/INISettingsObject.h"

namespace
{
	/// A fresh INISettingsObject backed by a file in a scratch temp
	/// directory, with a handful of settings of different types
	/// registered -- enough to exercise conversion on setValue().
	SettingsObjectPtr makeSettings(QTemporaryDir& dir)
	{
		auto settings = std::make_shared<INISettingsObject>(
			dir.filePath("settings.ini"));
		settings->registerSetting("IntSetting", 512);
		settings->registerSetting("BoolSetting", false);
		settings->registerSetting("StringSetting", QString("default"));
		return settings;
	}
} // namespace

class SettingsAdapterTest : public QObject
{
	Q_OBJECT

  private slots:
	void test_value_fallsBackToDefault_whenNeverSet()
	{
		QTemporaryDir dir;
		SettingsAdapter adapter(makeSettings(dir));

		QCOMPARE(adapter.value("IntSetting"), QVariant(512));
		QCOMPARE(adapter.defaultValue("IntSetting"), QVariant(512));
	}

	void test_value_unknownId_returnsInvalid()
	{
		QTemporaryDir dir;
		SettingsAdapter adapter(makeSettings(dir));

		QVERIFY(!adapter.value("NoSuchSetting").isValid());
		QVERIFY(!adapter.defaultValue("NoSuchSetting").isValid());
	}

	void test_contains_reflectsRegisteredSettings()
	{
		QTemporaryDir dir;
		SettingsAdapter adapter(makeSettings(dir));

		QVERIFY(adapter.contains("IntSetting"));
		QVERIFY(!adapter.contains("NoSuchSetting"));
	}

	/// The bug setValue() exists to prevent: QML hands an int setting a JS
	/// number, which arrives as a double, and it must not be stored as
	/// "4096.0".
	void test_setValue_convertsDoubleToInt()
	{
		QTemporaryDir dir;
		auto settings = makeSettings(dir);
		SettingsAdapter adapter(settings);

		adapter.setValue("IntSetting", QVariant(4096.0));

		QVariant stored = adapter.value("IntSetting");
		QCOMPARE(stored.typeId(), int(QMetaType::Int));
		QCOMPARE(stored.toInt(), 4096);
		// Not just the adapter's own read path -- the underlying setting
		// really holds an int, not a double.
		QCOMPARE(settings->get("IntSetting").typeId(), int(QMetaType::Int));
	}

	void test_setValue_bool_andString()
	{
		QTemporaryDir dir;
		SettingsAdapter adapter(makeSettings(dir));

		adapter.setValue("BoolSetting", QVariant(true));
		QCOMPARE(adapter.value("BoolSetting"), QVariant(true));

		adapter.setValue("StringSetting", QVariant("hello"));
		QCOMPARE(adapter.value("StringSetting"), QVariant(QString("hello")));
	}

	void test_setValue_unknownId_isNoOp()
	{
		QTemporaryDir dir;
		auto settings = makeSettings(dir);
		SettingsAdapter adapter(settings);

		adapter.setValue("NoSuchSetting", QVariant(1));

		QVERIFY(!settings->contains("NoSuchSetting"));
	}

	void test_reset_revertsToDefault()
	{
		QTemporaryDir dir;
		SettingsAdapter adapter(makeSettings(dir));
		adapter.setValue("IntSetting", QVariant(1024.0));
		QCOMPARE(adapter.value("IntSetting"), QVariant(1024));

		adapter.reset("IntSetting");

		QCOMPARE(adapter.value("IntSetting"), QVariant(512));
	}

	void test_valueChanged_emittedOnSetValue()
	{
		QTemporaryDir dir;
		SettingsAdapter adapter(makeSettings(dir));
		QSignalSpy spy(&adapter, &SettingsAdapter::valueChanged);

		adapter.setValue("IntSetting", QVariant(2048.0));

		QCOMPARE(spy.count(), 1);
		QCOMPARE(spy.first().at(0).toString(), QString("IntSetting"));
		QCOMPARE(spy.first().at(1).toInt(), 2048);
	}

	void test_valueChanged_emittedOnReset()
	{
		QTemporaryDir dir;
		SettingsAdapter adapter(makeSettings(dir));
		adapter.setValue("IntSetting", QVariant(2048.0));

		QSignalSpy spy(&adapter, &SettingsAdapter::valueChanged);
		adapter.reset("IntSetting");

		QCOMPARE(spy.count(), 1);
		QCOMPARE(spy.first().at(0).toString(), QString("IntSetting"));
		QCOMPARE(spy.first().at(1).toInt(), 512);
	}

	// No LauncherContext exists in this guiless test (see
	// AccountsController_test.cpp's own comment on the same limitation) --
	// applyProxySettings() must not crash without one, it just has nothing
	// to apply to.
	void test_applyProxySettings_withoutLauncherContext_doesNotCrash()
	{
		QTemporaryDir dir;
		SettingsAdapter adapter(makeSettings(dir));

		adapter.applyProxySettings("SOCKS5", "127.0.0.1", 1080, "user", "pass");
	}

	void test_checkExternalTool_unknownTool_reportsAnError()
	{
		QTemporaryDir dir;
		SettingsAdapter adapter(makeSettings(dir));

		QVERIFY(!adapter.checkExternalTool("not-a-tool", "/some/path").isEmpty());
	}

	void test_checkExternalTool_emptyPath_reportsAnErrorPerTool()
	{
		QTemporaryDir dir;
		SettingsAdapter adapter(makeSettings(dir));

		QVERIFY(!adapter.checkExternalTool("jprofiler", "").isEmpty());
		QVERIFY(!adapter.checkExternalTool("jvisualvm", "").isEmpty());
		QVERIFY(!adapter.checkExternalTool("mcedit", "").isEmpty());
	}

	void test_checkExternalTool_mcedit_rejectsFolderWithoutMCEdit()
	{
		QTemporaryDir dir;
		SettingsAdapter adapter(makeSettings(dir));

		QVERIFY(!adapter.checkExternalTool("mcedit", dir.path()).isEmpty());
	}
};

QTEST_GUILESS_MAIN(SettingsAdapterTest)

#include "SettingsAdapter_test.moc"
