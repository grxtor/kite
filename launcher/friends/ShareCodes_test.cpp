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
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTest>

#include "ShareCodes.h"
#include "ShareContent.h"
#include "ShareManifest.h"
#include "minecraft/mod/ModMetadataIndex.h"

using namespace ShareCodes;

namespace
{
	QJsonObject json(const QByteArray& text)
	{
		return QJsonDocument::fromJson(text).object();
	}

	QJsonArray array(const QByteArray& text)
	{
		return QJsonDocument::fromJson(text).array();
	}

	void touch(const QString& path)
	{
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write("x");
	}

	ModMetadataIndex::Entry sidecar(const QString& fileName, const QString& platform,
									const QString& project, const QString& version)
	{
		ModMetadataIndex::Entry entry;
		entry.fileName = fileName;
		entry.platform = platform;
		entry.projectId = project;
		entry.versionId = version;
		entry.slug = fileName.section('.', 0, 0);
		entry.name = fileName.section('.', 0, 0);
		return entry;
	}

	/* Functions, not constants: Kind and Platform live in another translation
	 * unit, which may not be initialised before this one's constants. */
	Entry sodium()
	{
		return {Kind::Mod, Platform::Modrinth, "AANobbMI", "IZskON6d"};
	}
	Entry faithful()
	{
		return {Kind::ResourcePack, Platform::CurseForge, "238222", "5432100"};
	}
} // namespace

class ShareCodesTest : public QObject
{
	Q_OBJECT

  private slots:
	void codesAreNormalizedLikeTheServerDoes_data()
	{
		QTest::addColumn<QString>("typed");
		QTest::addColumn<QString>("stored");
		QTest::newRow("as shown") << "KITE-7F3K-9QXM" << "7F3K9QXM";
		QTest::newRow("lower case") << "kite-7f3k-9qxm" << "7F3K9QXM";
		QTest::newRow("bare") << "7F3K9QXM" << "7F3K9QXM";
		QTest::newRow("spaces") << " 7f3k 9qxm " << "7F3K9QXM";
		QTest::newRow("prefix no dash") << "KITE7F3K9QXM" << "7F3K9QXM";
		QTest::newRow("letter O") << "KITE-7F3K-9QXO" << "";
		QTest::newRow("short") << "KITE-7F3K" << "";
		QTest::newRow("long") << "7F3K9QXMM" << "";
		QTest::newRow("empty") << "" << "";
	}
	void codesAreNormalizedLikeTheServerDoes()
	{
		QFETCH(QString, typed);
		QFETCH(QString, stored);
		QCOMPARE(normalize(typed), stored);
		QCOMPARE(display(typed), stored.isEmpty() ? QString()
												  : "KITE-" + stored.left(4) + "-" + stored.mid(4));
	}

	void manifestBodyLeavesOutWhatTheServerRefuses()
	{
		Manifest manifest{"  Survival\u202e 1.21 ", "1.21.4", "fabric", "0.16.10",
						  {sodium(), sodium(), faithful(),
						   {Kind::Mod, Platform::Modrinth, "AANobbMI", "1.1.1+1.17"}}};
		const QJsonObject body = QJsonDocument::fromJson(manifestBody(manifest)).object();
		QCOMPARE(body.value("name").toString(), QStringLiteral("Survival 1.21"));
		QCOMPARE(body.value("loader").toObject().value("version").toString(),
				 QStringLiteral("0.16.10"));
		const QJsonArray content = body.value("content").toArray();
		QCOMPARE(content.size(), 2);
		QCOMPARE(content.at(1).toObject().value("fileId").toInteger(), 5432100);
		QVERIFY(content.at(1).toObject().value("projectId").isDouble());

		manifest.loader = "vanilla";
		const QJsonObject vanilla = QJsonDocument::fromJson(manifestBody(manifest)).object();
		QVERIFY(!vanilla.value("loader").toObject().contains("version"));
		QCOMPARE(vanilla.value("content").toArray().size(), 1);
	}

	void aFetchedCodeIsReadStrictly()
	{
		const QByteArray body = R"({ "code": "KITE-7F3K-9QXM", "createdAt": "2026-10-24T18:00:00.000Z",
			"expiresAt": "2027-01-22T18:00:00.000Z",
			"manifest": { "name": "Survival 1.21", "minecraftVersion": "1.21.4",
				"loader": { "type": "fabric", "version": "0.16.10" },
				"content": [
					{ "kind": "mod", "platform": "modrinth", "projectId": "AANobbMI", "versionId": "IZskON6d" },
					{ "kind": "resourcepack", "platform": "curseforge", "projectId": 238222, "fileId": 5432100 } ] } })";
		const auto code = parseFetched(body);
		QVERIFY(code);
		QCOMPARE(code->code, QStringLiteral("7F3K9QXM"));
		QCOMPARE(code->manifest.content, (QList<Entry>{sodium(), faithful()}));
		const QVariantMap shown = toMap(*code);
		QCOMPARE(shown.value("code").toString(), QStringLiteral("KITE-7F3K-9QXM"));
		QCOMPARE(shown.value("mods").toInt(), 1);
		QCOMPARE(shown.value("resourcePacks").toInt(), 1);
		QCOMPARE(shown.value("shaderPacks").toInt(), 0);

		QVERIFY(!parseManifest(json(R"({ "name": "x", "minecraftVersion": "1.21",
			"loader": { "type": "vanilla" }, "content": [
			{ "kind": "mod", "platform": "modrinth", "projectId": "a", "versionId": "b" } ] })")));
		QVERIFY(!parseManifest(json(R"({ "name": "x", "minecraftVersion": "1.21",
			"loader": { "type": "fabric" }, "content": [] })")));
		QVERIFY(!parseFetched(R"({ "code": "nope", "manifest": {} })"));
	}

	void theListOfYourCodes()
	{
		const auto codes = parseList(R"({ "codes": [
			{ "code": "KITE-7F3K-9QXM", "name": "Survival 1.21", "minecraftVersion": "1.21.4",
			  "loader": { "type": "fabric", "version": "0.16.10" },
			  "createdAt": "2026-10-24T18:00:00.000Z", "expiresAt": "2027-01-22T18:00:00.000Z" },
			{ "code": "bad" } ] })");
		QVERIFY(codes);
		QCOMPARE(codes->size(), 1);
		QCOMPARE(codes->first().manifest.loader, QStringLiteral("fabric"));
		QVERIFY(!parseList("{}"));
		QCOMPARE(parseCreated(R"({ "code": "KITE-7F3K-9QXM" })")->code, QStringLiteral("7F3K9QXM"));
	}

	/* Files with ids go in; hand-added and turned-off ones are listed. */
	void theManifestOfAnInstance()
	{
		QTemporaryDir root;
		const QString mods = root.filePath("mods");
		const QString packs = root.filePath("resourcepacks");
		QDir().mkpath(mods);
		QDir().mkpath(packs);
		for (const char* name : {"sodium.jar", "handmade.jar", "lithium.jar.disabled", "mrpack.jar"}) {
			touch(QDir(mods).filePath(name));
		}
		touch(QDir(packs).filePath("faithful.zip"));
		QDir().mkpath(QDir(packs).filePath("Unpacked Pack"));
		ModMetadataIndex modIndex{QDir(mods)};
		QVERIFY(modIndex.put(sidecar("sodium.jar", "modrinth", "AANobbMI", "IZskON6d")));
		QVERIFY(modIndex.put(sidecar("lithium.jar", "modrinth", "gvQqBUqZ", "ZSNsJrPI")));
		auto fromPack = sidecar("mrpack.jar", "modrinth", "P7dR8mSH", "");
		fromPack.versionNumber = "0.92.2+1.21";
		fromPack.sha1 = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
		QVERIFY(modIndex.put(fromPack));
		ModMetadataIndex packIndex{QDir(packs)};
		QVERIFY(packIndex.put(sidecar("faithful.zip", "curseforge", "238222", "5432100")));

		ShareManifest::Source source{"Survival", "1.21.4", "fabric", "0.16.10", {},
									 {{Kind::Mod, mods}, {Kind::ResourcePack, packs}}};
		auto built = ShareManifest::build(source);
		QCOMPARE(built.manifest.content.size(), 2);
		QCOMPARE(built.included.size(), 2);
		QCOMPARE(built.skipped.size(), 4);
		const QVariantMap shown = ShareManifest::toMap(built);
		QCOMPARE(shown.value("mods").toInt(), 1);
		QCOMPARE(shown.value("resourcePacks").toInt(), 1);
		QStringList reasons;
		for (const QVariant& one : shown.value("skipped").toList()) {
			reasons.append(one.toMap().value("name").toString() + ":" +
						   one.toMap().value("reason").toString());
		}
		QCOMPARE(reasons, (QStringList{"handmade:noSource", "lithium:turnedOff",
									   "mrpack:noSource", "Unpacked Pack:noSource"}));

		/* The .mrpack's file is known to Modrinth by its hash. */
		QCOMPARE(ShareManifest::hashesToLookUp(built), QStringList{fromPack.sha1});
		ShareManifest::applyHashLookup(
			built, json(R"({ "da39a3ee5e6b4b0d3255bfef95601890afd80709":
				{ "id": "tFw0iWAk", "project_id": "P7dR8mSH" } })"));
		QCOMPARE(built.manifest.content.size(), 3);
		QCOMPARE(built.manifest.content.last(),
				 (Entry{Kind::Mod, Platform::Modrinth, "P7dR8mSH", "tFw0iWAk"}));
		QCOMPARE(built.skipped.size(), 3);

		source.loader = "vanilla";
		QCOMPARE(ShareManifest::build(source).manifest.content.size(), 1);
	}

	void modrinthAnswersBecomeDownloads()
	{
		const Entry gone{Kind::ShaderPack, Platform::Modrinth, "HVnmMxH1", "aaaaaaaa"};
		const Entry wrongProject{Kind::Mod, Platform::Modrinth, "XXXXXXXX", "bbbbbbbb"};
		ShareContent::Resolved resolved;
		ShareContent::addModrinth(
			resolved, {sodium(), gone, wrongProject},
			array(R"([ { "id": "IZskON6d", "project_id": "AANobbMI", "name": "Sodium 0.6",
				"files": [ { "primary": false, "url": "https:\/\/cdn.modrinth.com/x/other.jar", "filename": "other.jar" },
						   { "primary": true, "url": "https:\/\/cdn.modrinth.com/x/sodium.jar",
							 "filename": "sodium.jar", "size": 1234, "hashes": { "sha1": "abc" } } ] },
				{ "id": "bbbbbbbb", "project_id": "YYYYYYYY", "files": [
					{ "url": "https:\/\/cdn.modrinth.com/y.jar", "filename": "y.jar" } ] } ])"),
			array(R"([ { "id": "AANobbMI", "title": "Sodium", "slug": "sodium" },
					   { "id": "HVnmMxH1", "title": "Complementary", "slug": "complementary" } ])"));
		QCOMPARE(resolved.items.size(), 1);
		const auto& item = resolved.items.first();
		QCOMPARE(item.kind, Kind::Mod);
		QCOMPARE(item.download.fileName, QStringLiteral("sodium.jar"));
		QCOMPARE(item.download.name, QStringLiteral("Sodium"));
		QCOMPARE(item.download.slug, QStringLiteral("sodium"));
		QCOMPARE(item.download.versionId, QStringLiteral("IZskON6d"));
		QCOMPARE(item.download.sha1, QStringLiteral("abc"));
		QCOMPARE(resolved.missingNames, (QStringList{"Complementary", "XXXXXXXX"}));
	}

	void curseForgeAnswersBecomeDownloads()
	{
		const Entry optedOut{Kind::Mod, Platform::CurseForge, "306612", "4000000"};
		const Entry badName{Kind::Mod, Platform::CurseForge, "1", "2"};
		const Entry driveName{Kind::Mod, Platform::CurseForge, "3", "4"};
		ShareContent::Resolved resolved;
		ShareContent::addCurseForge(
			resolved, {faithful(), optedOut, badName, driveName},
			json(R"({ "data": [
				{ "id": 5432100, "modId": 238222, "fileName": "Faithful.zip", "fileLength": 99,
				  "downloadUrl": "https:\/\/edge.forgecdn.net/files/Faithful.zip",
				  "hashes": [ { "algo": 1, "value": "f00" } ] },
				{ "id": 4000000, "modId": 306612, "fileName": "fabric-api.jar", "downloadUrl": null },
				{ "id": 2, "modId": 1, "fileName": "../escape.jar" },
				{ "id": 4, "modId": 3, "fileName": "C:escape.jar" } ] })").value("data").toArray(),
			array(R"([ { "id": 238222, "name": "Faithful 32x", "slug": "faithful-32x" } ])"));
		QCOMPARE(resolved.items.size(), 2);
		QCOMPARE(resolved.items.at(0).download.sha1, QStringLiteral("f00"));
		QCOMPARE(resolved.items.at(0).download.name, QStringLiteral("Faithful 32x"));
		QVERIFY(resolved.items.at(1).download.browserDownloadOnly);
		QVERIFY(resolved.items.at(1).download.downloadUrl.contains("/306612/files/4000000/"));
		QCOMPARE(resolved.missingNames, (QStringList{"1", "3"}));
		QCOMPARE(ShareContent::curseForgeIdsBody("fileIds", {"5432100", "4000000"}),
				 QByteArray(R"({"fileIds":[5432100,4000000]})"));
	}

	/* The preview's list: by kind, then name; an unnamed project keeps its id. */
	void contentIsNamedForThePreview()
	{
		const Entry shaders{Kind::ShaderPack, Platform::Modrinth, "HVnmMxH1", "Bqen1mJX"};
		const Entry lithium{Kind::Mod, Platform::Modrinth, "gvQqBUqZ", "u8pHPXJl"};
		const QVariantList names = ShareContent::contentNames(
			{shaders, faithful(), sodium(), lithium},
			array(R"([ { "id": "AANobbMI", "title": "Sodium" }, { "id": "gvQqBUqZ", "title": "Lithium" },
					   { "id": "HVnmMxH1", "title": "Complementary Reimagined" } ])"),
			{});
		QStringList shown;
		for (const QVariant& one : names) {
			shown.append(one.toMap().value("kind").toString() + ":" + one.toMap().value("name").toString());
		}
		QCOMPARE(shown, (QStringList{"mod:Lithium", "mod:Sodium", "resourcepack:238222",
									 "shaderpack:Complementary Reimagined"}));
	}

	void contentIsAskedForInBatches()
	{
		QList<Entry> content;
		const int many = ShareContent::kBatchSize + 1;
		for (int i = 0; i < many; ++i) {
			content.append({Kind::Mod, Platform::Modrinth, QString("p%1").arg(i), QString("v%1").arg(i)});
		}
		content.append(faithful());
		const auto batches = ShareContent::batches(content, Platform::Modrinth);
		QCOMPARE(batches.size(), 2);
		QCOMPARE(batches.last().size(), 1);
		QCOMPARE(ShareContent::batches(content, Platform::CurseForge).size(), 1);
	}
};

QTEST_GUILESS_MAIN(ShareCodesTest)

#include "ShareCodes_test.moc"
