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

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSet>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/skins/DefaultSkins.h"
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

	static QByteArray pngBytes(int w, int h)
	{
		QImage image(w, h, QImage::Format_ARGB32);
		image.fill(QColor(40, 80, 120));
		QByteArray bytes;
		QBuffer buffer(&bytes);
		buffer.open(QIODevice::WriteOnly);
		image.save(&buffer, "PNG");
		return bytes;
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

	// The arm width picked for the skin the account wears stays picked:
	// the folder rescan that the library's own index write sets off, and a
	// fresh library over the same folder (the dialog opened again), used to
	// put the profile's width back. A change made elsewhere -- in game, on
	// another machine -- still comes through.
	void slimStaysOnTheWornSkin()
	{
		QTemporaryDir library;
		QVERIFY(library.isValid());
		auto account = MinecraftAccount::createBlankMSA();
		auto& profile = account->accountData()->minecraftProfile;
		profile.name = "Hero";
		profile.skin.url = "http://textures.minecraft.net/texture/hero";
		profile.skin.variant = "CLASSIC";
		profile.skin.data = pngBytes(64, 64);

		{
			SkinsController skins(account, library.path());
			const QVariantMap hero = find(skins.skins(), "Hero");
			QVERIFY(hero.value("worn").toBool());
			QVERIFY(!hero.value("slim").toBool());

			skins.setSlim("Hero", true);
			QVERIFY(find(skins.skins(), "Hero").value("slim").toBool());
			// The index write is seen by the folder watcher, which rescans.
			QSignalSpy rescanned(&skins, &SkinsController::skinsChanged);
			QTRY_VERIFY(rescanned.count() >= 1);
			QVERIFY(find(skins.skins(), "Hero").value("slim").toBool());
		}

		{
			SkinsController reopened(account, library.path());
			const QVariantMap hero = find(reopened.skins(), "Hero");
			QVERIFY(hero.value("worn").toBool());
			QVERIFY(hero.value("slim").toBool());
		}

		// Worn with slim arms, then switched back to classic elsewhere.
		profile.skin.variant = "SLIM";
		{
			SkinsController reopened(account, library.path());
			QVERIFY(find(reopened.skins(), "Hero").value("slim").toBool());
		}
		profile.skin.variant = "CLASSIC";
		{
			SkinsController reopened(account, library.path());
			QVERIFY(!find(reopened.skins(), "Hero").value("slim").toBool());
		}
	}

	// An unworn skin keeps its width through a fresh library too.
	void slimSurvivesAReload()
	{
		QTemporaryDir library;
		QTemporaryDir sources;
		QVERIFY(library.isValid() && sources.isValid());
		{
			SkinsController skins(nullptr, library.path());
			QCOMPARE(skins.importFile(writePng(sources.path(), "Hero.png", 64, 64)), QString());
			QTRY_VERIFY(!find(skins.skins(), "Hero").isEmpty());
			skins.setSlim("Hero", true);
		}
		SkinsController reopened(nullptr, library.path());
		QVERIFY(find(reopened.skins(), "Hero").value("slim").toBool());
	}

	// The game's nine default skins, apart from the user's own: each in
	// both arm widths, ready once its texture is in the cache (here put
	// there by hand, as a finished download would), and marked worn when
	// the account wears that texture.
	void defaultSkinsComeFromTheCache()
	{
		QTemporaryDir data;
		QTemporaryDir library;
		QVERIFY(data.isValid() && library.isValid());
		const QString previous = QDir::currentPath();
		QVERIFY(QDir::setCurrent(data.path()));

		const QString alexSlim = DefaultSkins::galleryPath("Alex", true);
		QVERIFY(QDir().mkpath(QFileInfo(alexSlim).absolutePath()));
		QFile file(alexSlim);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write(pngBytes(64, 64));
		file.close();

		auto account = MinecraftAccount::createBlankMSA();
		account->accountData()->minecraftProfile.skin.url =
			QString(DefaultSkins::galleryUrl("Alex", true))
				.replace("https://", "http://");
		SkinsController skins(account, library.path());
		const QVariantList defaults = skins.defaultSkins();
		QDir::setCurrent(previous);

		QStringList names;
		for (const QVariant& d : defaults)
			names << d.toMap().value("name").toString();
		QCOMPARE(names, QStringList({ "Steve", "Alex", "Ari", "Efe", "Kai", "Makena",
									  "Noor", "Sunny", "Zuri" }));

		const QVariantMap alex = defaults.at(1).toMap();
		QVERIFY(alex.value("slim").toBool());
		const QVariantList variants = alex.value("variants").toList();
		QCOMPARE(variants.size(), 2);
		const QVariantMap classic = variants.at(0).toMap();
		const QVariantMap slim = variants.at(1).toMap();
		QVERIFY(!classic.value("slim").toBool());
		QVERIFY(!classic.value("ready").toBool());
		QVERIFY(classic.value("thumbnailUrl").toString().isEmpty());
		QVERIFY(!classic.value("worn").toBool());
		QVERIFY(slim.value("slim").toBool());
		QVERIFY(slim.value("ready").toBool());
		QVERIFY(slim.value("worn").toBool());
		QVERIFY(slim.value("thumbnailUrl").toString().contains("/skinthumbslim/"));
		QCOMPARE(slim.value("path").toString(), alexSlim);
		QVERIFY(!defaults.at(0).toMap().value("slim").toBool()); // Steve
	}

	// One table: eighteen distinct textures.minecraft.net addresses, the
	// skinless fallback kept apart, and nothing for an unknown name.
	void defaultSkinTable()
	{
		QSet<QString> urls;
		for (const DefaultSkins::Character& c : DefaultSkins::characters()) {
			for (const bool slim : { false, true }) {
				const QString url = DefaultSkins::galleryUrl(c.name, slim);
				QVERIFY(url.startsWith("https://textures.minecraft.net/texture/"));
				QCOMPARE(QUrl(url).fileName().size(), 64);
				urls.insert(url);
			}
		}
		QCOMPARE(urls.size(), 18);
		QCOMPARE(DefaultSkins::galleryUrl("Steve", false),
				 QString("https://textures.minecraft.net/texture/"
						 "31f477eb1a7beee631c2ca64d06f8f68fa93a3386d04452ab27f43acdf1b60cb"));
		QCOMPARE(DefaultSkins::fallbackUrl(false),
				 QString("https://textures.minecraft.net/texture/"
						 "1a4af718455d4aab528e7a61f86fa25e6a369d1768dcb13f7df319a713eb810b"));
		QVERIFY(!urls.contains(DefaultSkins::fallbackUrl(true)));
		// Noor is drawn slim by default; its classic texture is this one
		// (checked against the client jar's wide/noor.png).
		QVERIFY(DefaultSkins::galleryUrl("Noor", false).endsWith(
			"90e75cd429ba6331cd210b9bd19399527ee3bab467b5a9f61cb8a27b177f6789"));
		// Kept under the texture's own hash.
		QCOMPARE(QFileInfo(DefaultSkins::galleryPath("Steve", false)).fileName(),
				 QString("31f477eb1a7beee631c2ca64d06f8f68fa93a3386d04452ab27f43acdf1b60cb.png"));
		QVERIFY(DefaultSkins::galleryUrl("Herobrine", false).isEmpty());
		QVERIFY(DefaultSkins::galleryPath("Herobrine", false).isEmpty());
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
