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
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

#include "icons/IconList.h"

/*
 * Covers IconList::roleNames() -- the QML-facing addition an icon picker
 * grid binds `key`/`name`/`isBuiltin` against (see
 * qml/Components/IconPickerDialog.qml) -- without needing the compiled
 * "multimc" icon theme resource InstanceIconProvider_test.cpp needs for real
 * pixmaps: addThemeIcon()/addIcon() only touch the model's bookkeeping, never
 * render anything.
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

  private:
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
