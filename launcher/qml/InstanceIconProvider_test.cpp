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

#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include <icons/IconList.h>

#include "InstanceIconProvider.h"

namespace
{
/* The same four directories Application.cpp (and IconTheme_test.cpp) pass to
 * IconList. All four are needed for "grass" specifically: of the built-in
 * instance icons, only scalable/instances/ ships an unsuffixed "grass.svg"
 * -- the raster sizes only have "grass_legacy.png" -- so leaving scalable
 * out would make every lookup below fall through to the fallback path and
 * prove nothing. */
const QStringList kBuiltinIconDirs = {
	QStringLiteral(":/icons/multimc/32x32/instances/"),
	QStringLiteral(":/icons/multimc/50x50/instances/"),
	QStringLiteral(":/icons/multimc/128x128/instances/"),
	QStringLiteral(":/icons/multimc/scalable/instances/"),
};

// Mirrors InstanceIconProvider's own fallback extent.
constexpr int kDefaultIconExtent = 64;
} // namespace

/*
 * Unit test for InstanceIconProvider.
 *
 * Runs against a real IconList rather than a mock: the point of this
 * provider is the "<key>?rev=N" bridge and the unknown-key/invalid-size
 * fallbacks, and a mock would only prove it calls a method, not that the
 * right pixels come back. Getting a real "grass" builtin icon to resolve
 * here needs the same two things application start-up does, and both are
 * easy to skip silently:
 *
 *  - Q_INIT_RESOURCE(multimc) in main(), because the compiled "multimc"
 *    resource lives inside the MeshMC_logic static library and nothing else
 *    in this test binary forces the linker to keep those object files.
 *  - QIcon::setThemeName("multimc") over a search path that includes
 *    ":/icons", because IconList's Builtin icons resolve through
 *    QIcon::fromTheme() rather than loading the file directly -- see
 *    MMCIcon::icon() -- which is exactly what ThemeManager::setIconTheme()
 *    sets up for the real application.
 *
 * Skip either one and every lookup below quietly takes the "unknown key"
 * path instead, which would pass without testing the thing it claims to.
 */
class InstanceIconProviderTest : public QObject
{
	Q_OBJECT

  private slots:
	void initTestCase()
	{
		// Same as ThemeManager::setIconTheme(), restated rather than called
		// so this measures IconList/Qt, not that wrapper. ":/icons" goes
		// first so the bundled theme wins over a same-named one on the host.
		QStringList searchPaths = QIcon::themeSearchPaths();
		searchPaths.prepend(QStringLiteral(":/icons"));
		QIcon::setThemeSearchPaths(searchPaths);
		QIcon::setThemeName(QStringLiteral("multimc"));
	}

	void cleanupTestCase()
	{
		QIcon::setThemeName(QString());
	}

	void init()
	{
		m_tempDir = std::make_unique<QTemporaryDir>();
		QVERIFY(m_tempDir->isValid());
		m_icons = std::make_shared<IconList>(kBuiltinIconDirs, m_tempDir->path());
	}

	void cleanup()
	{
		m_icons.reset();
		m_tempDir.reset();
	}

	void knownBuiltinKeyReturnsRequestedSize()
	{
		InstanceIconProvider provider(m_icons);

		QSize size;
		const QPixmap pixmap = provider.requestPixmap(
			QStringLiteral("grass"), &size, QSize(64, 64));

		QVERIFY(!pixmap.isNull());
		QCOMPARE(size, QSize(64, 64));
		QCOMPARE(pixmap.size(), QSize(64, 64));
	}

	void revisionQueryIsIgnored()
	{
		InstanceIconProvider provider(m_icons);

		QSize plainSize;
		QSize revisionedSize;
		const QPixmap plain = provider.requestPixmap(
			QStringLiteral("grass"), &plainSize, QSize(48, 48));
		const QPixmap revisioned = provider.requestPixmap(
			QStringLiteral("grass?rev=7"), &revisionedSize, QSize(48, 48));

		QCOMPARE(revisionedSize, plainSize);
		QCOMPARE(revisioned.toImage(), plain.toImage());
	}

	void unknownKeyFallsBackToNonNullPixmap()
	{
		InstanceIconProvider provider(m_icons);

		QSize size;
		const QPixmap pixmap = provider.requestPixmap(
			QStringLiteral("this-key-does-not-exist"), &size, QSize(32, 32));

		QVERIFY(!pixmap.isNull());
		QCOMPARE(size, QSize(32, 32));
	}

	void invalidRequestedSizeUsesDefault()
	{
		InstanceIconProvider provider(m_icons);

		QSize size;
		const QPixmap pixmap = provider.requestPixmap(
			QStringLiteral("grass"), &size, QSize());

		QVERIFY(!pixmap.isNull());
		QCOMPARE(size, QSize(kDefaultIconExtent, kDefaultIconExtent));
	}

  private:
	std::unique_ptr<QTemporaryDir> m_tempDir;
	std::shared_ptr<IconList> m_icons;
};

int main(int argc, char* argv[])
{
	/* Same reasoning as QmlModule_test.cpp: a Pixmap-type QQuickImageProvider
	 * needs a QGuiApplication, and forcing offscreen keeps this runnable on a
	 * headless runner without depending on the harness to set
	 * QT_QPA_PLATFORM for us. */
	qputenv("QT_QPA_PLATFORM", "offscreen");

	QGuiApplication app(argc, argv);

	/* The compiled "multimc" resource sits in MeshMC_logic, a static
	 * library; without a symbol reference into it, the linker drops the
	 * object that registers it, and every builtinPaths lookup below would
	 * silently come back empty. main.cpp does the same for the launcher
	 * itself. */
	Q_INIT_RESOURCE(multimc);

	InstanceIconProviderTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "InstanceIconProvider_test.moc"
