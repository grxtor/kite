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

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "icons/GameTextureFixture.h"
#include "icons/IconList.h"

using GameTextureFixture::filled;

/*
 * Covers IconList::roleNames() -- the QML-facing addition an icon picker
 * grid binds `key`/`name`/`isBuiltin` against (see
 * qml/Components/IconPickerDialog.qml) -- without needing the compiled
 * "multimc" icon theme resource InstanceIconProvider_test.cpp needs for real
 * pixmaps: addThemeIcon()/addIcon() only touch the model's bookkeeping, never
 * render anything. And the game-texture layer over built-in keys, whose
 * pixels come from client jars the test builds itself.
 */
class IconListTest : public QObject
{
	Q_OBJECT

  private slots:
	void init()
	{
		m_dir = std::make_unique<QTemporaryDir>();
		QVERIFY(m_dir->isValid());
		m_icons = std::make_unique<IconList>(QStringList(), m_dir->path());
	}

	void cleanup()
	{
		m_icons.reset();
		m_dir.reset();
	}

	void roleNamesExposeKeyNameAndIsBuiltin()
	{
		const auto roles = m_icons->roleNames();
		QCOMPARE(roles.value(Qt::UserRole), QByteArray("key"));
		QCOMPARE(roles.value(Qt::DisplayRole), QByteArray("name"));
		QCOMPARE(roles.value(IconList::IsBuiltinRole),
				 QByteArray("isBuiltin"));
	}

	void builtinIconReportsIsBuiltinRole()
	{
		QVERIFY(m_icons->addThemeIcon(QStringLiteral("grass")));

		const int row = m_icons->getIconIndex(QStringLiteral("grass"));
		QVERIFY(row != -1);
		const QModelIndex index = m_icons->index(row);

		QCOMPARE(m_icons->data(index, Qt::UserRole).toString(),
				 QStringLiteral("grass"));
		QVERIFY(m_icons->data(index, IconList::IsBuiltinRole).toBool());
	}

	void fileBasedIconReportsNotBuiltin()
	{
		// A 1x1 PNG is enough for QIcon to accept the file -- nothing here
		// looks at the pixels.
		const QString path = m_dir->filePath(QStringLiteral("custom.png"));
		QImage image(1, 1, QImage::Format_RGB32);
		image.fill(Qt::black);
		QVERIFY(image.save(path, "PNG"));

		QVERIFY(m_icons->addIcon(QStringLiteral("custom"),
								 QStringLiteral("Custom"), path,
								 IconType::FileBased));

		const int row = m_icons->getIconIndex(QStringLiteral("custom"));
		QVERIFY(row != -1);
		const QModelIndex index = m_icons->index(row);

		QCOMPARE(m_icons->data(index, Qt::DisplayRole).toString(),
				 QStringLiteral("Custom"));
		QVERIFY(!m_icons->data(index, IconList::IsBuiltinRole).toBool());
	}

	/* The game-texture layer (GameTextureIcons), over client jars built
	 * from generated PNGs (GameTextureFixture). */
	void gameTextureReplacesBuiltinIcon()
	{
		const QImage grass = filled(16, 16, qRgba(40, 160, 40, 255));
		writeJar(QStringLiteral("1.21.4"),
				 {{QStringLiteral("block/grass_block_side.png"), grass}});
		QVERIFY(m_icons->addThemeIcon(QStringLiteral("grass")));
		QSignalSpy updated(m_icons.get(), &IconList::iconUpdated);

		m_icons->loadGameTextures(libraries(), cache());

		QCOMPARE(updated.count(), 1);
		QCOMPARE(updated.first().first().toString(), QStringLiteral("grass"));
		QCOMPARE(pixelOf(QStringLiteral("grass")), grass.pixel(0, 0));
		// Unknown keys fall back to grass, texture and all.
		QCOMPARE(pixelOf(QStringLiteral("no-such-key")), grass.pixel(0, 0));
		// Still a built-in icon, so exporting an instance never copies the
		// texture into the pack.
		const QModelIndex row =
			m_icons->index(m_icons->getIconIndex(QStringLiteral("grass")));
		QVERIFY(m_icons->data(row, IconList::IsBuiltinRole).toBool());
		QVERIFY(m_icons->icon(QStringLiteral("grass"))->isBuiltIn());

		// The jar gone, the built-in icon comes back.
		QVERIFY(QDir(libraries()).removeRecursively());
		m_icons->loadGameTextures(libraries(), cache());
		QCOMPARE(updated.count(), 2);
		QVERIFY(pixelOf(QStringLiteral("grass")) != grass.pixel(0, 0));
	}

	void gameTexturesReloadOnlyForANewJar()
	{
		const QImage oldGrass = filled(16, 16, qRgba(40, 160, 40, 255));
		writeJar(QStringLiteral("1.21.4"),
				 {{QStringLiteral("block/grass_block_side.png"), oldGrass}});
		QVERIFY(m_icons->addThemeIcon(QStringLiteral("grass")));
		m_icons->loadGameTextures(libraries(), cache());
		QSignalSpy updated(m_icons.get(), &IconList::iconUpdated);

		// Nothing new: nothing happens.
		m_icons->loadGameTextures(libraries(), cache());
		QCOMPARE(updated.count(), 0);

		// A newer jar still being written keeps what is shown.
		const QString newer = GameTextureFixture::jarPath(
			libraries(), QStringLiteral("26.3"));
		QVERIFY(FS::ensureFilePathExists(newer));
		QFile partial(newer);
		QVERIFY(partial.open(QIODevice::WriteOnly));
		partial.write("PK\x03\x04 half a jar");
		partial.close();
		m_icons->loadGameTextures(libraries(), cache());
		QCOMPARE(updated.count(), 0);
		QCOMPARE(pixelOf(QStringLiteral("grass")), oldGrass.pixel(0, 0));

		// Once it is whole, its textures take over.
		const QImage newGrass = filled(16, 16, qRgba(70, 190, 50, 255));
		writeJar(QStringLiteral("26.3"),
				 {{QStringLiteral("block/grass_block_side.png"), newGrass}});
		m_icons->loadGameTextures(libraries(), cache());
		QCOMPARE(updated.count(), 1);
		QCOMPARE(pixelOf(QStringLiteral("grass")), newGrass.pixel(0, 0));
	}

	void customIconUnderBuiltinKeyStillWins()
	{
		writeJar(QStringLiteral("1.21.4"),
				 {{QStringLiteral("block/grass_block_side.png"),
				   filled(16, 16, qRgba(40, 160, 40, 255))}});
		const QRgb mine = qRgba(1, 2, 3, 255);
		const QString path = m_dir->filePath(QStringLiteral("grass.png"));
		QVERIFY(filled(16, 16, mine).save(path, "PNG"));
		QVERIFY(m_icons->addThemeIcon(QStringLiteral("grass")));
		QVERIFY(m_icons->addIcon(QStringLiteral("grass"), QString(), path,
								 IconType::FileBased));

		m_icons->loadGameTextures(libraries(), cache());

		QCOMPARE(pixelOf(QStringLiteral("grass")), mine);
	}

  private:
	QString libraries() const
	{
		return m_dir->filePath(QStringLiteral("libraries"));
	}

	QString cache() const
	{
		return m_dir->filePath(QStringLiteral("cache/gametextures"));
	}

	void writeJar(const QString& version,
				  const QList<QPair<QString, QImage>>& textures) const
	{
		QVERIFY(GameTextureFixture::writeJar(libraries(), version, textures));
	}

	QRgb pixelOf(const QString& key) const
	{
		const QImage image =
			m_icons->getIcon(key).pixmap(QSize(16, 16), 1.0).toImage();
		return image.isNull() ? 0 : image.pixel(0, 0);
	}

	std::unique_ptr<QTemporaryDir> m_dir;
	std::unique_ptr<IconList> m_icons;
};

int main(int argc, char* argv[])
{
	/* QImage::save()/addIcon() go through QtGui; offscreen keeps this
	 * runnable on a headless runner without depending on the harness to
	 * set QT_QPA_PLATFORM for us -- same reasoning as
	 * InstanceIconProvider_test.cpp. */
	qputenv("QT_QPA_PLATFORM", "offscreen");

	QGuiApplication app(argc, argv);

	IconListTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "IconList_test.moc"
