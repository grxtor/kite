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

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

#include "modplatform/PlatformCover.h"

using namespace PlatformCover;

namespace
{
QJsonArray arrayOf(const QByteArray& json)
{
	return QJsonDocument::fromJson(json).array();
}

const QString kModrinthCdn = QStringLiteral("https://cdn.modrinth.com/data/abc/images/");
const QStringList kPngJpeg{QStringLiteral("png"), QStringLiteral("jpeg")};
const QStringList kWithWebp{QStringLiteral("png"), QStringLiteral("jpeg"),
							QStringLiteral("webp")};
} // namespace

class PlatformCoverTest : public QObject
{
	Q_OBJECT

  private slots:

	void slugComesFromAModrinthIconKey()
	{
		QCOMPARE(slugFromIconKey("modrinth_fresh-smooth"),
				 QStringLiteral("fresh-smooth"));
		QCOMPARE(slugFromIconKey(" modrinth_cobblemon "),
				 QStringLiteral("cobblemon"));
		QCOMPARE(slugFromIconKey("modrinth"), QString());
		QCOMPARE(slugFromIconKey("flame"), QString());
		QCOMPARE(slugFromIconKey("default"), QString());
		QCOMPARE(slugFromIconKey(QString()), QString());
	}

	void anIdIsOneLookup()
	{
		PackFacts kite;
		kite.packProvider = "modrinth";
		kite.packId = "ICQPO4fX";
		kite.packSlug = "fresh-smooth";
		kite.iconKey = "modrinth_fresh-smooth";
		kite.instanceName = "Fresh & Smooth";
		QCOMPARE(lookupsFor(kite),
				 (QList<Lookup>{{Provider::Modrinth, "ICQPO4fX", QString()}}));

		// Another launcher's keys.
		PackFacts imported;
		imported.legacyProvider = "flame";
		imported.legacyId = "925200";
		QCOMPARE(lookupsFor(imported),
				 (QList<Lookup>{{Provider::CurseForge, "925200", QString()}}));

		// Modrinth takes the slug where there is no id; so does the icon key.
		PackFacts slug;
		slug.packProvider = "modrinth";
		slug.packSlug = "fabulously-optimized";
		QCOMPARE(lookupsFor(slug),
				 (QList<Lookup>{{Provider::Modrinth, "fabulously-optimized",
								 QString()}}));
		PackFacts icon;
		icon.iconKey = "modrinth_fresh-smooth";
		QCOMPARE(lookupsFor(icon),
				 (QList<Lookup>{{Provider::Modrinth, "fresh-smooth",
								 QString()}}));
	}

	void anIdThatCannotSitInAUrlPathIsNotUsed()
	{
		PackFacts facts;
		facts.packProvider = "modrinth";
		facts.packId = "../users/x?y";
		QVERIFY(lookupsFor(facts).isEmpty());

		PackFacts flame;
		flame.legacyProvider = "flame";
		flame.legacyId = "12abc";
		QVERIFY(lookupsFor(flame).isEmpty());

		// "." and ".." would walk the request up a path.
		PackFacts dots;
		dots.packProvider = "modrinth";
		dots.packId = "..";
		dots.packSlug = ".";
		QVERIFY(lookupsFor(dots).isEmpty());
	}

	void withoutAnIdThePackIsSearchedByTitle()
	{
		// Other launchers' CurseForge packs: a type and a name, no id.
		PackFacts flame;
		flame.legacyProvider = "flame";
		flame.packTitles = {"All the Mods 10 Aeronautics",
							"ATM10-Aeronautics-0.5.1"};
		flame.instanceName = "ATM10-Aeronautics-0.5.1";
		flame.iconKey = "flame";
		QCOMPARE(lookupsFor(flame),
				 (QList<Lookup>{
					 {Provider::CurseForge, QString(),
					  "All the Mods 10 Aeronautics"},
					 {Provider::CurseForge, QString(),
					  "ATM10-Aeronautics-0.5.1"}}));

		// The default Modrinth icon names the platform only.
		PackFacts modrinth;
		modrinth.iconKey = "modrinth";
		modrinth.instanceName = "Some Pack";
		QCOMPARE(lookupsFor(modrinth),
				 (QList<Lookup>{{Provider::Modrinth, QString(),
								 "Some Pack"}}));
	}

	void anInstanceThatPointsToNoPlatformIsNeverSearched()
	{
		PackFacts facts;
		facts.instanceName = "Cobblemon";
		facts.iconKey = "default";
		QVERIFY(lookupsFor(facts).isEmpty());
	}

	void titlesAreDeduplicatedAndCapped()
	{
		PackFacts facts;
		facts.packTitles = {"One", "one ", "Two", "Three"};
		facts.instanceName = "Four";
		facts.legacyProvider = "flame";
		const QList<Lookup> lookups = lookupsFor(facts);
		QCOMPARE(lookups.size(), 3);
		QCOMPARE(lookups.at(0).title, QStringLiteral("One"));
		QCOMPARE(lookups.at(1).title, QStringLiteral("Two"));
		QCOMPARE(lookups.at(2).title, QStringLiteral("Three"));
	}

	void modrinthListsTheFeaturedFirstAndOriginalsBeforeCopies()
	{
		const QByteArray cdn = kModrinthCdn.toUtf8();
		const QJsonArray gallery = arrayOf(
			"[{\"url\": \"" + cdn + "first.png\", \"raw_url\": \"" + cdn +
			"first_raw.png\", \"featured\": false},"
			" {\"url\": \"" + cdn + "hero.png\", \"raw_url\": \"" + cdn +
			"hero_raw.png\", \"featured\": true}]");
		QCOMPARE(modrinthImages(gallery, kPngJpeg),
				 (QStringList{kModrinthCdn + "hero_raw.png",
							  kModrinthCdn + "first_raw.png",
							  kModrinthCdn + "hero.png",
							  kModrinthCdn + "first.png"}));
	}

	void modrinthFallsBackToTheFirstThenToNothing()
	{
		const QJsonArray gallery = arrayOf(
			"[{\"url\": \"" + kModrinthCdn.toUtf8() + "first.png\"},"
			" {\"url\": \"" + kModrinthCdn.toUtf8() + "second.png\", \"featured\": false}]");
		QCOMPARE(modrinthImages(gallery, kPngJpeg).value(0),
				 kModrinthCdn + "first.png");

		QVERIFY(modrinthImages(QJsonArray(), kPngJpeg).isEmpty());
		QVERIFY(modrinthImages(arrayOf("[{\"title\": \"no url\"}]"), kPngJpeg)
					.isEmpty());
	}

	void modrinthCapsTheCandidates()
	{
		QByteArray json = "[";
		for (int i = 0; i < 10; ++i) {
			json += (i ? "," : "") + QByteArray("{\"raw_url\": \"") +
					kModrinthCdn.toUtf8() + "p" + QByteArray::number(i) +
					".png\"}";
		}
		json += "]";
		QCOMPARE(modrinthImages(arrayOf(json), kPngJpeg).size(),
				 kMaxCandidates);
	}

	void modrinthSkipsGifsAndFormatsQtCannotRead()
	{
		const QByteArray cdn = kModrinthCdn.toUtf8();
		const QJsonArray gallery = arrayOf(
			"[{\"featured\": true, \"url\": \"" + cdn +
			"hero.webp\", \"raw_url\": \"" + cdn + "hero.jpg\"}]");
		// jpg counts as jpeg; WebP only when it decodes.
		QCOMPARE(modrinthImages(gallery, kWithWebp),
				 (QStringList{kModrinthCdn + "hero.jpg",
							  kModrinthCdn + "hero.webp"}));
		QCOMPARE(modrinthImages(gallery, kPngJpeg),
				 (QStringList{kModrinthCdn + "hero.jpg"}));
		// A GIF is skipped for the next gallery entry.
		QCOMPARE(modrinthImages(
					 arrayOf("[{\"featured\": true, \"url\": \"" + cdn +
							 "a.gif\"}, {\"url\": \"" + cdn + "b.png\"}]"),
					 kPngJpeg),
				 (QStringList{kModrinthCdn + "b.png"}));
		// Only WebP and no plugin: nothing.
		QVERIFY(modrinthImages(arrayOf("[{\"url\": \"" + cdn + "only.webp\"}]"),
							   kPngJpeg)
					.isEmpty());
	}

	void onlyThePlatformsOwnImageHostIsFollowed()
	{
		const QJsonArray elsewhere = arrayOf(
			"[{\"featured\": true, \"url\": \"https://example.org/a.png\"},"
			" {\"url\": \"http://cdn.modrinth.com/data/abc/images/b.png\"}]");
		QVERIFY(modrinthImages(elsewhere, kPngJpeg).isEmpty());

		const QJsonArray flame = arrayOf(
			"[{\"url\": \"https://example.org/a.png\"},"
			" {\"url\": \"https://media.forgecdn.net/attachments/1/2/b.png\"}]");
		QCOMPARE(flameImages(flame, kPngJpeg),
				 (QStringList{"https://media.forgecdn.net/attachments/1/2/b.png"}));
	}

	void curseForgeListsTheScreenshotsInOrder()
	{
		const QJsonArray screenshots = arrayOf(
			"[{\"url\": \"https://media.forgecdn.net/attachments/1/1/one.png\","
			"  \"thumbnailUrl\": \"https://media.forgecdn.net/attachments/thumbnails/1/1/310/172/one.png\"},"
			" {\"url\": \"https://media.forgecdn.net/attachments/1/2/two.jpg\"}]");
		QCOMPARE(flameImages(screenshots, kPngJpeg),
				 (QStringList{"https://media.forgecdn.net/attachments/1/1/one.png",
							  "https://media.forgecdn.net/attachments/1/2/two.jpg"}));
		QVERIFY(flameImages(QJsonArray(), kPngJpeg).isEmpty());
	}

	void onlyAWideLargePictureSuitsAHero()
	{
		QVERIFY(suitableAsCover(QSize(1920, 1080)));
		QVERIFY(suitableAsCover(QSize(1000, 700)));
		// The 350 px copy, a square logo and a tall poster.
		QVERIFY(!suitableAsCover(QSize(350, 197)));
		QVERIFY(!suitableAsCover(QSize(1200, 1200)));
		QVERIFY(!suitableAsCover(QSize(1000, 1500)));
		QVERIFY(!suitableAsCover(QSize()));
	}

	void aTitleMatchNeedsExactlyOneModpack()
	{
		const QJsonArray hits = arrayOf(
			"[{\"title\": \"Fresh & Smooth\", \"project_id\": \"ICQPO4fX\"},"
			" {\"title\": \"Fresh & Smooth Lite\", \"project_id\": \"zzz\"},"
			" {\"title\": \"Other\", \"project_id\": \"yyy\"}]");
		QCOMPARE(uniqueModrinthMatch(hits, "Fresh & Smooth"),
				 QStringLiteral("ICQPO4fX"));
		// Case and surrounding space do not matter.
		QCOMPARE(uniqueModrinthMatch(hits, "  fresh & SMOOTH "),
				 QStringLiteral("ICQPO4fX"));
		// A partial title is no match.
		QCOMPARE(uniqueModrinthMatch(hits, "Fresh"), QString());
		QCOMPARE(uniqueModrinthMatch(hits, "Nothing like it"), QString());
		QCOMPARE(uniqueModrinthMatch(hits, QString()), QString());
		QCOMPARE(uniqueModrinthMatch(QJsonArray(), "Other"), QString());

		const QJsonArray twins = arrayOf(
			"[{\"title\": \"Same\", \"project_id\": \"a\"},"
			" {\"title\": \"same\", \"project_id\": \"b\"}]");
		QCOMPARE(uniqueModrinthMatch(twins, "Same"), QString());

		// The same project listed twice is still one.
		const QJsonArray repeated = arrayOf(
			"[{\"title\": \"Same\", \"project_id\": \"a\"},"
			" {\"title\": \"Same\", \"project_id\": \"a\"}]");
		QCOMPARE(uniqueModrinthMatch(repeated, "Same"), QStringLiteral("a"));
	}

	void curseForgeMatchesOnNameAndNumericId()
	{
		const QJsonArray hits = arrayOf(
			"[{\"name\": \"All the Mods 10\", \"id\": 925200},"
			" {\"name\": \"All the Mods 10 - ATM10\", \"id\": 111}]");
		QCOMPARE(uniqueFlameMatch(hits, "all the mods 10"),
				 QStringLiteral("925200"));
		QCOMPARE(uniqueFlameMatch(hits, "All the Mods"), QString());

		const QJsonArray twins = arrayOf(
			"[{\"name\": \"Zombiemania\", \"id\": 1},"
			" {\"name\": \"Zombiemania\", \"id\": 2}]");
		QCOMPARE(uniqueFlameMatch(twins, "Zombiemania"), QString());
	}

	void theManifestNamesThePack()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QCOMPARE(flameManifestName(dir.path()), QString());

		QVERIFY(QDir(dir.path()).mkpath("flame"));
		QFile manifest(QDir(dir.path()).filePath("flame/manifest.json"));
		QVERIFY(manifest.open(QIODevice::WriteOnly));
		manifest.write("{\"name\": \" All the Mods 10 Aeronautics \", "
					   "\"version\": \"0.5.1\"}");
		manifest.close();
		QCOMPARE(flameManifestName(dir.path()),
				 QStringLiteral("All the Mods 10 Aeronautics"));

		QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Truncate));
		manifest.write("not json");
		manifest.close();
		QCOMPARE(flameManifestName(dir.path()), QString());
	}

	void aLookupThatFoundNothingWaitsAWeek()
	{
		const qint64 now = 2'000'000'000;
		QVERIFY(retryDue(0, now));
		QVERIFY(!retryDue(now - 60, now));
		QVERIFY(!retryDue(now - kRetryAfterSeconds + 1, now));
		QVERIFY(retryDue(now - kRetryAfterSeconds, now));
		// A clock that moved back must not block it for good.
		QVERIFY(retryDue(now + 3600, now));
	}

	void decodableFormatsListsPlainFormatsLowerCase()
	{
		const QStringList formats = decodableFormats();
		QVERIFY(formats.contains(QStringLiteral("png")));
		QVERIFY(!formats.contains(QStringLiteral("jpg")));
		for (const QString& format : formats) {
			QCOMPARE(format, format.toLower());
		}
		// Whether "webp" is listed depends on Qt's image plugins here; the
		// pick functions take the list so both cases are covered above.
	}
};

QTEST_GUILESS_MAIN(PlatformCoverTest)
#include "PlatformCover_test.moc"
