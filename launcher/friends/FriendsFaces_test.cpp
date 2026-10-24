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

#include <QBuffer>
#include <QColor>
#include <QDateTime>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "FriendsFaces.h"

namespace
{
	const QString kUuid = "069a79f4-44e9-4726-a5be-fca90e38aaf5";
	const QString kSkinUrl =
		"http://textures.minecraft.net/texture/"
		"292009a4925b58f02c77dadc3ecef07ea4c7472f64e0fdc32ce5522489362680";

	/* A session server profile whose textures property names @p skinUrl. */
	QByteArray profile(const QString& skinUrl, const QString& id = "069a79f444e94726a5befca90e38aaf5")
	{
		const QJsonObject textures{
			{"textures", QJsonObject{{"SKIN", QJsonObject{{"url", skinUrl}}}}}};
		const QByteArray value =
			QJsonDocument(textures).toJson(QJsonDocument::Compact).toBase64();
		const QJsonObject root{
			{"id", id},
			{"name", "Notch"},
			{"properties",
			 QJsonArray{QJsonObject{{"name", "textures"},
									{"value", QString::fromLatin1(value)}}}}};
		return QJsonDocument(root).toJson(QJsonDocument::Compact);
	}

	/* A 64x64 skin: face region red, hat region blue with its left half left
	 * transparent, and a green pixel just outside both. */
	QByteArray skinPng(int height = 64)
	{
		QImage skin(64, height, QImage::Format_ARGB32);
		skin.fill(Qt::transparent);
		for (int y = 0; y < 8; ++y) {
			for (int x = 0; x < 8; ++x) {
				skin.setPixelColor(8 + x, 8 + y, Qt::red);
				if (x >= 4) {
					skin.setPixelColor(40 + x, 8 + y, Qt::blue);
				}
			}
		}
		skin.setPixelColor(16, 8, Qt::green);
		QByteArray png;
		QBuffer buffer(&png);
		buffer.open(QIODevice::WriteOnly);
		skin.save(&buffer, "PNG");
		return png;
	}
} // namespace

class FriendsFacesTest : public QObject
{
	Q_OBJECT

  private slots:
	void theProfileUrlHasNoDashes()
	{
		QCOMPARE(FriendsFaces::profileUrl("https://sessionserver.mojang.com/", kUuid)
					 .toString(),
				 QString("https://sessionserver.mojang.com/session/minecraft/"
						 "profile/069a79f444e94726a5befca90e38aaf5"));
	}

	void theSkinComesFromTexturesMinecraftNet()
	{
		const auto url =
			FriendsFaces::skinUrlFromProfile(profile(kSkinUrl), kUuid, false);
		QVERIFY(url.has_value());
		/* Mojang words it as http; it is fetched over https. */
		QCOMPARE(url->scheme(), QString("https"));
		QCOMPARE(url->host(), QString("textures.minecraft.net"));
		QCOMPARE(url->path(), QUrl(kSkinUrl).path());
	}

	void otherHostsAreRefused_data()
	{
		QTest::addColumn<QString>("url");
		QTest::newRow("another host")
			<< "https://evil.example.com/texture/abc";
		QTest::newRow("a lookalike") << "https://textures.minecraft.net.evil.com/a";
		QTest::newRow("a subdomain") << "https://x.textures.minecraft.net/a";
		QTest::newRow("a login") << "https://user@textures.minecraft.net/a";
		QTest::newRow("another port") << "https://textures.minecraft.net:8443/a";
		QTest::newRow("a file") << "file:///etc/passwd";
		QTest::newRow("no path") << "https://textures.minecraft.net";
		QTest::newRow("loopback by default") << "http://localhost:5545/skin.png";
		QTest::newRow("empty") << "";
	}
	void otherHostsAreRefused()
	{
		QFETCH(QString, url);
		QVERIFY(!FriendsFaces::skinUrlFromProfile(profile(url), kUuid, false));
	}

	void aLocalMockIsAcceptedOnlyWhenAsked()
	{
		const QString mock = "http://localhost:5545/skin.png";
		QVERIFY(!FriendsFaces::skinUrlFromProfile(profile(mock), kUuid, false));
		const auto url =
			FriendsFaces::skinUrlFromProfile(profile(mock), kUuid, true);
		QVERIFY(url.has_value());
		QCOMPARE(url->toString(), mock);
		/* Even then, only this machine. */
		QVERIFY(!FriendsFaces::skinUrlFromProfile(
			profile("http://192.168.1.5/skin.png"), kUuid, true));
	}

	void aProfileOfSomeoneElseIsRefused()
	{
		QVERIFY(!FriendsFaces::skinUrlFromProfile(
			profile(kSkinUrl, "853c80ef3c3749fdaa49938b674adae6"), kUuid, false));
	}

	void brokenProfilesAreRefused()
	{
		QVERIFY(!FriendsFaces::skinUrlFromProfile("", kUuid, false));
		QVERIFY(!FriendsFaces::skinUrlFromProfile("{ not json", kUuid, false));
		/* A player without a skin has a textures property with no SKIN. */
		const QByteArray none =
			QJsonDocument(QJsonObject{{"textures", QJsonObject()}})
				.toJson(QJsonDocument::Compact)
				.toBase64();
		const QJsonObject root{
			{"id", "069a79f444e94726a5befca90e38aaf5"},
			{"properties",
			 QJsonArray{QJsonObject{{"name", "textures"},
									{"value", QString::fromLatin1(none)}}}}};
		QVERIFY(!FriendsFaces::skinUrlFromProfile(QJsonDocument(root).toJson(), kUuid, false));
	}

	void theFaceIsCutWithTheHatOverIt()
	{
		const QImage face = FriendsFaces::faceFromSkinPng(skinPng());
		QCOMPARE(face.size(), QSize(8, 8));
		/* The face where the hat is transparent, the hat where it is not. */
		QCOMPARE(face.pixelColor(0, 0), QColor(Qt::red));
		QCOMPARE(face.pixelColor(3, 7), QColor(Qt::red));
		QCOMPARE(face.pixelColor(4, 0), QColor(Qt::blue));
		QCOMPARE(face.pixelColor(7, 7), QColor(Qt::blue));
		/* The pixel beside the face region is not in it. */
		QVERIFY(face.pixelColor(7, 0) != QColor(Qt::green));
	}

	void anOldSkinHasAFaceToo()
	{
		QCOMPARE(FriendsFaces::faceFromSkinPng(skinPng(32)).size(), QSize(8, 8));
	}

	void whatIsNotASkinHasNoFace()
	{
		QVERIFY(FriendsFaces::faceFromSkinPng("not a png").isNull());
		QImage wrong(128, 128, QImage::Format_ARGB32);
		wrong.fill(Qt::red);
		QByteArray png;
		QBuffer buffer(&png);
		buffer.open(QIODevice::WriteOnly);
		wrong.save(&buffer, "PNG");
		QVERIFY(FriendsFaces::faceFromSkinPng(png).isNull());
	}

	void aFaceIsStaleAfterADay()
	{
		const QDateTime now(QDate(2026, 10, 24), QTime(12, 0), Qt::UTC);
		QVERIFY(!FriendsFaces::isStale(now.addSecs(-3600), now));
		QVERIFY(!FriendsFaces::isStale(now.addSecs(-24 * 3600 + 1), now));
		QVERIFY(FriendsFaces::isStale(now.addSecs(-24 * 3600), now));
		QVERIFY(FriendsFaces::isStale(QDateTime(), now));
	}

	void loopbackIsThisMachineOnly()
	{
		QVERIFY(FriendsFaces::isLoopbackHost("localhost"));
		QVERIFY(FriendsFaces::isLoopbackHost("127.0.0.1"));
		QVERIFY(FriendsFaces::isLoopbackHost("::1"));
		QVERIFY(!FriendsFaces::isLoopbackHost("sessionserver.mojang.com"));
		QVERIFY(!FriendsFaces::isLoopbackHost("localhost.evil.com"));
	}
};

QTEST_GUILESS_MAIN(FriendsFacesTest)

#include "FriendsFaces_test.moc"
