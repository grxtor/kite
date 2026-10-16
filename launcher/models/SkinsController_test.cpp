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

#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "models/SkinsController.h"

/*
 * The skin library over a temporary folder: adding a file, listing it,
 * renaming, arm width, deleting, and refusing what is not a skin.
 */
class SkinsControllerTest : public QObject
{
	Q_OBJECT

	static QString writePng(const QString& dir, const QString& name, int w, int h)
	{
		QImage image(w, h, QImage::Format_ARGB32);
		image.fill(QColor(120, 80, 40));
		const QString path = dir + QLatin1Char('/') + name;
		image.save(path, "PNG");
		return path;
	}

	static QVariantMap find(const QVariantList& skins, const QString& name)
	{
		for (const QVariant& skin : skins) {
			if (skin.toMap().value("name").toString() == name)
				return skin.toMap();
		}
		return {};
	}

  private slots:
	void importListRenameSlimDelete()
	{
		QTemporaryDir library;
		QTemporaryDir sources;
		QVERIFY(library.isValid() && sources.isValid());
		SkinsController skins(nullptr, library.path());
		QVERIFY(skins.skins().isEmpty());

		const QString source = writePng(sources.path(), "Hero.png", 64, 64);
		QCOMPARE(skins.importFile(QUrl::fromLocalFile(source).toString()), QString());
		// The folder watcher picks the copy up; wait for it.
		QTRY_VERIFY(!find(skins.skins(), "Hero").isEmpty());
		const QVariantMap hero = find(skins.skins(), "Hero");
		QVERIFY(hero.value("textureUrl").toString().startsWith("image://accountface/skinfile/"));
		QVERIFY(!hero.value("worn").toBool());
		QVERIFY(!hero.value("slim").toBool());

		const QString classicThumb = hero.value("thumbnailUrl").toString();
		skins.setSlim("Hero", true);
		QVERIFY(find(skins.skins(), "Hero").value("slim").toBool());
		// A new url, so the list's sprite is redrawn with slim arms.
		QVERIFY(find(skins.skins(), "Hero").value("thumbnailUrl").toString() != classicThumb);
		QVERIFY(find(skins.skins(), "Hero").value("thumbnailUrl").toString().contains("/skinthumbslim/"));

		QVERIFY(skins.rename("Hero", "Villain"));
		QTRY_VERIFY(!find(skins.skins(), "Villain").isEmpty());
		QVERIFY(find(skins.skins(), "Hero").isEmpty());
		QVERIFY(!skins.rename("Villain", "   "));

		QCOMPARE(skins.remove("Villain"), QString());
		QTRY_VERIFY(find(skins.skins(), "Villain").isEmpty());
		QVERIFY(!skins.remove("Villain").isEmpty());
	}

	void notASkinIsRefused()
	{
		QTemporaryDir library;
		QTemporaryDir sources;
		SkinsController skins(nullptr, library.path());
		const QString wrong = writePng(sources.path(), "Big.png", 128, 128);
		QVERIFY(!skins.importFile(wrong).isEmpty());
		QVERIFY(skins.skins().isEmpty());
	}

	// Only http(s) links and real names start a download.
	void importsCheckTheirInput()
	{
		QTemporaryDir library;
		SkinsController skins(nullptr, library.path());
		QCOMPARE(skins.importFromUrl("not a link at all"), nullptr);
		QCOMPARE(skins.importFromUrl("ftp://example.org/skin.png"), nullptr);
		QCOMPARE(skins.importFromUsername("   "), nullptr);
	}
};

QTEST_GUILESS_MAIN(SkinsControllerTest)

#include "SkinsController_test.moc"
