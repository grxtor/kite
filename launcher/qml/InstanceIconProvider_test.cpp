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
/* The directories Application.cpp passes to IconList. All four are needed:
 * only scalable/instances/ ships an unsuffixed "grass.svg", so without it
 * every lookup below would take the fallback path and prove nothing. */
const QStringList kBuiltinIconDirs = {
	QStringLiteral(":/icons/multimc/32x32/instances/"),
	QStringLiteral(":/icons/multimc/50x50/instances/"),
	QStringLiteral(":/icons/multimc/128x128/instances/"),
	QStringLiteral(":/icons/multimc/scalable/instances/"),
};

// InstanceIconProvider's fallback extent.
constexpr int kDefaultIconExtent = 64;
} // namespace

/*
 * Unit test for InstanceIconProvider, against a real IconList: the point is
 * the "<key>?rev=N" bridge and the unknown-key/invalid-size fallbacks, and a
 * mock would not prove the right pixels come back. Resolving the real "grass"
 * builtin needs two things application start-up also does:
 *
 *  - Q_INIT_RESOURCE(multimc) in main(), because the "multimc" resource lives
 *    in the Kite_logic static library and nothing else forces the linker to
 *    keep it.
 *  - QIcon::setThemeName("multimc") over a search path that includes
 *    ":/icons", because builtin icons resolve through QIcon::fromTheme().
 *
 * Without either, every lookup quietly takes the "unknown key" path.
 */
class InstanceIconProviderTest : public QObject
{
	Q_OBJECT

  private slots:
	void initTestCase()
	{
		// As ThemeManager::setIconTheme(), restated so this measures IconList
		// and Qt. ":/icons" goes first so the bundled theme wins over a
		// same-named one on the host.
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
	/* A Pixmap-type QQuickImageProvider needs a QGuiApplication; offscreen
	 * keeps this runnable on a headless runner. */
	qputenv("QT_QPA_PLATFORM", "offscreen");

	QGuiApplication app(argc, argv);

	/* The "multimc" resource sits in the Kite_logic static library; without a
	 * symbol reference the linker drops the object that registers it, and every
	 * builtinPaths lookup would come back empty. */
	Q_INIT_RESOURCE(multimc);

	InstanceIconProviderTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "InstanceIconProvider_test.moc"
