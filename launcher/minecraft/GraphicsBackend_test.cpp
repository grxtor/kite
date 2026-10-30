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

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "minecraft/GraphicsBackend.h"

namespace
{
	const QString kKey = QStringLiteral("preferredGraphicsBackend");

	void writeFile(const QString& path, const QByteArray& content)
	{
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write(content);
	}

	QByteArray readFile(const QString& path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}
} // namespace

class GraphicsBackendTest : public QObject
{
	Q_OBJECT

  private slots:
	void supportsVersion_data()
	{
		QTest::addColumn<QString>("version");
		QTest::addColumn<bool>("supported");
		QTest::newRow("26.2") << "26.2" << true;
		QTest::newRow("26.3") << "26.3" << true;
		QTest::newRow("26.3.1") << "26.3.1" << true;
		QTest::newRow("26.2 snapshot") << "26.2-snapshot-1" << true;
		QTest::newRow("26.4 pre-release") << "26.4-pre1" << true;
		QTest::newRow("27.1") << "27.1" << true;
		QTest::newRow("26.1") << "26.1" << false;
		QTest::newRow("26.1.2") << "26.1.2" << false;
		QTest::newRow("1.21.11") << "1.21.11" << false;
		QTest::newRow("weekly snapshot") << "25w14a" << false;
		QTest::newRow("beta") << "b1.7.3" << false;
		QTest::newRow("empty") << "" << false;
	}
	void supportsVersion()
	{
		QFETCH(QString, version);
		QFETCH(bool, supported);
		QCOMPARE(GraphicsBackend::supportsVersion(version), supported);
	}

	void readsDefaultWithoutFile()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QCOMPARE(GraphicsBackend::read(dir.path()), QString("default"));
	}

	void readsDefaultWithoutKey()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		writeFile(dir.filePath("options.txt"), "version:4000\nmaxFps:120\n");
		QCOMPARE(GraphicsBackend::read(dir.path()), QString("default"));
	}

	void readsDefaultForGarbage()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		writeFile(dir.filePath("options.txt"),
				  "preferredGraphicsBackend:metal\n");
		QCOMPARE(GraphicsBackend::read(dir.path()), QString("default"));
	}

	void readsStoredBackend_data()
	{
		QTest::addColumn<QString>("backend");
		QTest::newRow("vulkan") << "vulkan";
		QTest::newRow("opengl") << "opengl";
		QTest::newRow("default") << "default";
	}
	void readsStoredBackend()
	{
		QFETCH(QString, backend);
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		writeFile(dir.filePath("options.txt"),
				  "maxFps:120\n" + kKey.toUtf8() + ':' + backend.toUtf8() +
					  "\n");
		QCOMPARE(GraphicsBackend::read(dir.path()), backend);
	}

	void createsOptionsFile()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		/* The game folder does not exist yet. */
		const QString root = dir.filePath("minecraft");
		QVERIFY(GraphicsBackend::write(root, "vulkan"));
		QCOMPARE(readFile(root + "/options.txt"),
				 QByteArray("preferredGraphicsBackend:vulkan\n"));
		QCOMPARE(GraphicsBackend::read(root), QString("vulkan"));
	}

	void replacesValueKeepingOtherLines()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = dir.filePath("options.txt");
		writeFile(path,
				  "version:4000\nmaxFps:120\npreferredGraphicsBackend:opengl\n"
				  "renderDistance:12\n");
		QVERIFY(GraphicsBackend::write(dir.path(), "vulkan"));
		QCOMPARE(readFile(path),
				 QByteArray("version:4000\nmaxFps:120\n"
							"preferredGraphicsBackend:vulkan\n"
							"renderDistance:12\n"));
	}

	void appendsMissingKey()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = dir.filePath("options.txt");
		writeFile(path, "version:4000\nmaxFps:120\n");
		QVERIFY(GraphicsBackend::write(dir.path(), "opengl"));
		QCOMPARE(readFile(path),
				 QByteArray("version:4000\nmaxFps:120\n"
							"preferredGraphicsBackend:opengl\n"));
	}

	void defaultWithoutFileWritesNothing()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QVERIFY(GraphicsBackend::write(dir.path(), "default"));
		QVERIFY(!QFile::exists(dir.filePath("options.txt")));
	}

	void unknownBackendChangesNothing()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = dir.filePath("options.txt");
		QVERIFY(!GraphicsBackend::write(dir.path(), "metal"));
		QVERIFY(!QFile::exists(path));

		const QByteArray before = "version:4000\nmaxFps:120\n";
		writeFile(path, before);
		QVERIFY(!GraphicsBackend::write(dir.path(), "metal"));
		QCOMPARE(readFile(path), before);
	}
};

QTEST_GUILESS_MAIN(GraphicsBackendTest)

#include "GraphicsBackend_test.moc"
