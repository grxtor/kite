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

#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>
#include <QVariantMap>

#include "FileSystem.h"
#include "InstanceShortcuts.h"
#include "NullInstance.h"
#include "core/LauncherContext.h"
#include "icons/IconList.h"
#include "models/InstanceShortcutController.h"
#include "settings/INISettingsObject.h"

namespace
{
	SettingsObjectPtr makeGlobalSettings(const QString& dir)
	{
		auto settings = std::make_shared<INISettingsObject>(
			FS::PathCombine(dir, "global.ini"));
		settings->registerSetting("PreLaunchCommand", "");
		settings->registerSetting("WrapperCommand", "");
		settings->registerSetting("PostExitCommand", "");
		settings->registerSetting("ShowConsole", true);
		settings->registerSetting("AutoCloseConsole", false);
		settings->registerSetting("ShowConsoleOnError", true);
		settings->registerSetting("LogPrePostOutput", true);
		settings->registerSetting("ConsoleMaxLines", 100000);
		settings->registerSetting("ConsoleOverflowStop", true);
		return settings;
	}

	/* Just enough launcher for a shortcut: an icon list. Everything else
	 * is null, which nothing on this path asks for. */
	class IconsOnlyLauncher : public LauncherContext
	{
	  public:
		explicit IconsOnlyLauncher(std::shared_ptr<IconList> icons)
			: m_icons(std::move(icons))
		{
			setInstance(this);
		}
		~IconsOnlyLauncher() override
		{
			setInstance(nullptr);
		}

		std::shared_ptr<SettingsObject> settings() const override
		{
			return {};
		}
		std::shared_ptr<InstanceList> instances() const override
		{
			return {};
		}
		std::shared_ptr<IconList> icons() const override
		{
			return m_icons;
		}
		shared_qobject_ptr<AccountList> accounts() const override
		{
			return {};
		}
		std::shared_ptr<TranslationsModel> translations() override
		{
			return {};
		}
		std::shared_ptr<JavaInstallList> javalist() override
		{
			return {};
		}
		shared_qobject_ptr<QNetworkAccessManager> network() override
		{
			return {};
		}
		shared_qobject_ptr<HttpMetaCache> metacache() override
		{
			return {};
		}
		shared_qobject_ptr<Meta::Index> metadataIndex() override
		{
			return {};
		}
		QIcon getThemedIcon(const QString&) override
		{
			return QIcon();
		}
		QString getJarsPath() override
		{
			return QString();
		}
		QString msaClientId() const override
		{
			return QString();
		}
		void updateProxySettings(QString, QString, int, QString,
								 QString) override
		{
		}
		AuthRequestDecorator* authRequestDecorator() const override
		{
			return nullptr;
		}
		UiHost* uiHost() const override
		{
			return nullptr;
		}

	  private:
		std::shared_ptr<IconList> m_icons;
	};
} // namespace

/*
 * The QML shortcut controller over a NullInstance in a temporary folder.
 * Shortcuts are written to a temporary "Other" location only, never to the
 * real desktop or applications folder.
 */
class InstanceShortcutControllerTest : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;

	InstancePtr makeInstance(const QString& name)
	{
		const QString root = m_dir.filePath(name);
		QDir().mkpath(root);
		auto settings = std::make_shared<INISettingsObject>(
			FS::PathCombine(root, "instance.cfg"));
		auto instance = std::make_shared<NullInstance>(
			makeGlobalSettings(m_dir.path()), settings, root);
		instance->setName(name);
		return instance;
	}

  private slots:
	void initTestCase()
	{
		QVERIFY(m_dir.isValid());
	}

	void offersLocationsAndNames()
	{
		InstanceShortcutController controller(makeInstance("Survival"));
		const QVariantList locations = controller.locations();
		QVERIFY(!locations.isEmpty());
		// Asking with a save dialog is always possible, and always last.
		QCOMPARE(locations.last().toMap().value("value").toString(),
				 QStringLiteral("other"));
		QCOMPARE(controller.suggestedName(QString()), QStringLiteral("Survival"));
		QCOMPARE(controller.suggestedName("  mc.example.org "),
				 QStringLiteral("Survival (mc.example.org)"));
		QVERIFY(controller.suggestedFileUrl("Survival").startsWith("file://"));
		// No launcher, no accounts.
		QVERIFY(controller.accounts().isEmpty());
	}

	void failsWithoutAnIcon()
	{
		auto instance = makeInstance("NoIcon");
		InstanceShortcutController controller(instance);

		QVERIFY(!controller.create("x", "other", QString(), QString()));
		QVERIFY(!controller.error().isEmpty());

		// No launcher here, so no icon list and nothing to render.
		QVERIFY(!controller.create("x", m_dir.filePath("NoIcon shortcut"),
								   QString(), QString()));
		QVERIFY(controller.error().contains("icon"));
		QVERIFY(controller.lastPath().isEmpty());
		QVERIFY(instance->shortcuts().isEmpty());
	}

	void writesAShortcut()
	{
#if !defined(Q_OS_MACOS) && !defined(Q_OS_LINUX) && !defined(Q_OS_WIN)
		QSKIP("Shortcuts are not supported on this platform.");
#endif
		const QString iconDir = m_dir.filePath("icons");
		QVERIFY(QDir().mkpath(iconDir));
		QImage image(64, 64, QImage::Format_ARGB32);
		image.fill(Qt::darkGreen);
		QVERIFY(image.save(FS::PathCombine(iconDir, "grass.png")));
		IconsOnlyLauncher launcher(
			std::make_shared<IconList>(QStringList(), iconDir));

		auto instance = makeInstance("Skyblock");
		InstanceShortcutController controller(instance);

		// Typed with the suffix the shortcut gets anyway: not doubled.
#if defined(Q_OS_MACOS)
		const QString suffix = QStringLiteral(".app");
#else
		const QString suffix = InstanceShortcuts::fileSuffix();
#endif
		const QString chosen = m_dir.filePath("Skyblock shortcut") + suffix;
		QVERIFY2(controller.create(QString(),
								   QUrl::fromLocalFile(chosen).toString(),
								   "mc.example.org", QString()),
				 qPrintable(controller.error()));
		QVERIFY(controller.error().isEmpty());
		QCOMPARE(QDir::cleanPath(controller.lastPath()), QDir::cleanPath(chosen));
		QVERIFY(QFileInfo::exists(controller.lastPath()));

		const auto shortcuts = instance->shortcuts();
		QCOMPARE(shortcuts.size(), 1);
		QCOMPARE(shortcuts.first().name,
				 QStringLiteral("Skyblock (mc.example.org)"));
		QVERIFY(shortcuts.first().target == ShortcutTarget::Other);
	}
};

QTEST_MAIN(InstanceShortcutControllerTest)

#include "InstanceShortcutController_test.moc"
