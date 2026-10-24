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

#include <QTest>
#include "TestUtil.h"

#include "minecraft/MojangVersionFormat.h"
#include "minecraft/OneSixVersionFormat.h"
#include "minecraft/Library.h"
#include "minecraft/RuntimeContext.h"
#include "minecraft/Rule.h"
#include "minecraft/OpSys.h"

#include <QJsonArray>
#include <QSysInfo>
#include "net/HttpMetaCache.h"
#include "FileSystem.h"

class LibraryTest : public QObject
{
	Q_OBJECT
  private:
	LibraryPtr readMojangJson(const char* file)
	{
		auto path = QFINDTESTDATA(file);
		QFile jsonFile(path);
		if (!jsonFile.open(QIODevice::ReadOnly))
			return nullptr;
		auto data = jsonFile.readAll();
		jsonFile.close();
		ProblemContainer problems;
		return MojangVersionFormat::libraryFromJson(
			problems, QJsonDocument::fromJson(data).object(), file);
	}
	// get absolute path to expected storage, assuming default cache prefix
	QStringList getStorage(QString relative)
	{
		return {FS::PathCombine(cache->getBasePath("libraries"), relative)};
	}
	// @p system with a Java of @p architecture, in the meta's words
	static RuntimeContext context(const QString& system,
								  const QString& architecture = "x86_64")
	{
		RuntimeContext out;
		out.system = system;
		out.architecture = architecture;
		out.bitness =
			(architecture == "x86" || architecture == "arm32") ? "32" : "64";
		return out;
	}
	static LibraryPtr libraryFromJson(const char* json)
	{
		ProblemContainer problems;
		return MojangVersionFormat::libraryFromJson(
			problems, QJsonDocument::fromJson(json).object(), "test");
	}
  private slots:
	void initTestCase()
	{
		cache.reset(new HttpMetaCache());
		cache->addBase("libraries", QDir("libraries").absolutePath());
		dataDir = QDir("data").absolutePath();
	}
	void test_legacy()
	{
		Library test("test.package:testname:testversion");
		QCOMPARE(test.artifactPrefix(), QString("test.package:testname"));
		QCOMPARE(test.isNative(), false);

		QStringList jar, native, native32, native64;
		test.getApplicableFiles(context(OpSys_toString(currentSystem)), jar, native, native32, native64,
								QString());
		QCOMPARE(
			jar,
			getStorage(
				"test/package/testname/testversion/testname-testversion.jar"));
		QCOMPARE(native, {});
		QCOMPARE(native32, {});
		QCOMPARE(native64, {});
	}
	void test_legacy_url()
	{
		QStringList failedFiles;
		Library test("test.package:testname:testversion");
		test.setRepositoryURL("file://foo/bar");
		auto downloads = test.getDownloads(context(OpSys_toString(currentSystem)), cache.get(),
										   failedFiles, QString());
		QCOMPARE(downloads.size(), 1);
		QCOMPARE(failedFiles, {});
		NetAction::Ptr dl = downloads[0];
		QCOMPARE(dl->m_url, QUrl("file://foo/bar/test/package/testname/"
								 "testversion/testname-testversion.jar"));
	}
	void test_legacy_url_local_broken()
	{
		Library test("test.package:testname:testversion");
		QCOMPARE(test.isNative(), false);
		QStringList failedFiles;
		test.setHint("local");
		auto downloads = test.getDownloads(context(OpSys_toString(currentSystem)), cache.get(),
										   failedFiles, QString());
		QCOMPARE(downloads.size(), 0);
		QCOMPARE(failedFiles, {"testname-testversion.jar"});
	}
	void test_legacy_url_local_override()
	{
		Library test("com.paulscode:codecwav:20101023");
		QCOMPARE(test.isNative(), false);
		QStringList failedFiles;
		test.setHint("local");
		auto downloads = test.getDownloads(context(OpSys_toString(currentSystem)), cache.get(),
										   failedFiles, QString("data"));
		QCOMPARE(downloads.size(), 0);
		qDebug() << failedFiles;
		QCOMPARE(failedFiles.size(), 0);

		QStringList jar, native, native32, native64;
		test.getApplicableFiles(context(OpSys_toString(currentSystem)), jar, native, native32, native64,
								QString("data"));
		QCOMPARE(jar,
				 {QFileInfo("data/codecwav-20101023.jar").absoluteFilePath()});
		QCOMPARE(native, {});
		QCOMPARE(native32, {});
		QCOMPARE(native64, {});
	}
	void test_legacy_native()
	{
		Library test("test.package:testname:testversion");
		test.m_nativeClassifiers["linux"] = "linux";
		QCOMPARE(test.isNative(), true);
		test.setRepositoryURL("file://foo/bar");
		{
			QStringList jar, native, native32, native64;
			test.getApplicableFiles(context("linux"), jar, native, native32, native64,
									QString());
			QCOMPARE(jar, {});
			QCOMPARE(native, getStorage("test/package/testname/testversion/"
										"testname-testversion-linux.jar"));
			QCOMPARE(native32, {});
			QCOMPARE(native64, {});
			QStringList failedFiles;
			auto dls = test.getDownloads(context("linux"), cache.get(), failedFiles,
										 QString());
			QCOMPARE(dls.size(), 1);
			QCOMPARE(failedFiles, {});
			auto dl = dls[0];
			QCOMPARE(dl->m_url,
					 QUrl("file://foo/bar/test/package/testname/testversion/"
						  "testname-testversion-linux.jar"));
		}
	}
	void test_legacy_native_arch()
	{
		Library test("test.package:testname:testversion");
		test.m_nativeClassifiers["linux"] = "linux-${arch}";
		test.m_nativeClassifiers["osx"] = "osx-${arch}";
		test.m_nativeClassifiers["windows"] = "windows-${arch}";
		QCOMPARE(test.isNative(), true);
		test.setRepositoryURL("file://foo/bar");
		{
			QStringList jar, native, native32, native64;
			test.getApplicableFiles(context("linux"), jar, native, native32, native64,
									QString());
			QCOMPARE(jar, {});
			QCOMPARE(native, {});
			QCOMPARE(native32, getStorage("test/package/testname/testversion/"
										  "testname-testversion-linux-32.jar"));
			QCOMPARE(native64, getStorage("test/package/testname/testversion/"
										  "testname-testversion-linux-64.jar"));
			QStringList failedFiles;
			auto dls = test.getDownloads(context("linux"), cache.get(), failedFiles,
										 QString());
			QCOMPARE(dls.size(), 2);
			QCOMPARE(failedFiles, {});
			QCOMPARE(dls[0]->m_url,
					 QUrl("file://foo/bar/test/package/testname/testversion/"
						  "testname-testversion-linux-32.jar"));
			QCOMPARE(dls[1]->m_url,
					 QUrl("file://foo/bar/test/package/testname/testversion/"
						  "testname-testversion-linux-64.jar"));
		}
		{
			QStringList jar, native, native32, native64;
			test.getApplicableFiles(context("windows"), jar, native, native32, native64,
									QString());
			QCOMPARE(jar, {});
			QCOMPARE(native, {});
			QCOMPARE(native32,
					 getStorage("test/package/testname/testversion/"
								"testname-testversion-windows-32.jar"));
			QCOMPARE(native64,
					 getStorage("test/package/testname/testversion/"
								"testname-testversion-windows-64.jar"));
			QStringList failedFiles;
			auto dls = test.getDownloads(context("windows"), cache.get(), failedFiles,
										 QString());
			QCOMPARE(dls.size(), 2);
			QCOMPARE(failedFiles, {});
			QCOMPARE(dls[0]->m_url,
					 QUrl("file://foo/bar/test/package/testname/testversion/"
						  "testname-testversion-windows-32.jar"));
			QCOMPARE(dls[1]->m_url,
					 QUrl("file://foo/bar/test/package/testname/testversion/"
						  "testname-testversion-windows-64.jar"));
		}
		{
			QStringList jar, native, native32, native64;
			test.getApplicableFiles(context("osx"), jar, native, native32, native64,
									QString());
			QCOMPARE(jar, {});
			QCOMPARE(native, {});
			QCOMPARE(native32, getStorage("test/package/testname/testversion/"
										  "testname-testversion-osx-32.jar"));
			QCOMPARE(native64, getStorage("test/package/testname/testversion/"
										  "testname-testversion-osx-64.jar"));
			QStringList failedFiles;
			auto dls =
				test.getDownloads(context("osx"), cache.get(), failedFiles, QString());
			QCOMPARE(dls.size(), 2);
			QCOMPARE(failedFiles, {});
			QCOMPARE(dls[0]->m_url,
					 QUrl("file://foo/bar/test/package/testname/testversion/"
						  "testname-testversion-osx-32.jar"));
			QCOMPARE(dls[1]->m_url,
					 QUrl("file://foo/bar/test/package/testname/testversion/"
						  "testname-testversion-osx-64.jar"));
		}
	}
	void test_legacy_native_arch_local_override()
	{
		Library test("test.package:testname:testversion");
		test.m_nativeClassifiers["linux"] = "linux-${arch}";
		test.setHint("local");
		QCOMPARE(test.isNative(), true);
		test.setRepositoryURL("file://foo/bar");
		{
			QStringList jar, native, native32, native64;
			test.getApplicableFiles(context("linux"), jar, native, native32, native64,
									QString("data"));
			QCOMPARE(jar, {});
			QCOMPARE(native, {});
			QCOMPARE(native32,
					 {QFileInfo("data/testname-testversion-linux-32.jar")
						  .absoluteFilePath()});
			QCOMPARE(native64,
					 {QFileInfo("data/testname-testversion-linux-64.jar")
						  .absoluteFilePath()});
			QStringList failedFiles;
			auto dls = test.getDownloads(context("linux"), cache.get(), failedFiles,
										 QString("data"));
			QCOMPARE(dls.size(), 0);
			QCOMPARE(failedFiles, {"data/testname-testversion-linux-64.jar"});
		}
	}
	void test_onenine()
	{
		auto test = readMojangJson("data/lib-simple.json");
		{
			QStringList jar, native, native32, native64;
			test->getApplicableFiles(context("osx"), jar, native, native32, native64,
									 QString());
			QCOMPARE(
				jar,
				getStorage(
					"com/paulscode/codecwav/20101023/codecwav-20101023.jar"));
			QCOMPARE(native, {});
			QCOMPARE(native32, {});
			QCOMPARE(native64, {});
		}
		{
			QStringList failedFiles;
			auto dls = test->getDownloads(context("linux"), cache.get(), failedFiles,
										  QString());
			QCOMPARE(dls.size(), 1);
			QCOMPARE(failedFiles, {});
			QCOMPARE(dls[0]->m_url,
					 QUrl("https://libraries.minecraft.net/com/paulscode/"
						  "codecwav/20101023/codecwav-20101023.jar"));
		}
		test->setHint("local");
		{
			QStringList jar, native, native32, native64;
			test->getApplicableFiles(context("osx"), jar, native, native32, native64,
									 QString("data"));
			QCOMPARE(
				jar,
				{QFileInfo("data/codecwav-20101023.jar").absoluteFilePath()});
			QCOMPARE(native, {});
			QCOMPARE(native32, {});
			QCOMPARE(native64, {});
		}
		{
			QStringList failedFiles;
			auto dls = test->getDownloads(context("linux"), cache.get(), failedFiles,
										  QString("data"));
			QCOMPARE(dls.size(), 0);
			QCOMPARE(failedFiles, {});
		}
	}
	void test_onenine_local_override()
	{
		auto test = readMojangJson("data/lib-simple.json");
		test->setHint("local");
		{
			QStringList jar, native, native32, native64;
			test->getApplicableFiles(context("osx"), jar, native, native32, native64,
									 QString("data"));
			QCOMPARE(
				jar,
				{QFileInfo("data/codecwav-20101023.jar").absoluteFilePath()});
			QCOMPARE(native, {});
			QCOMPARE(native32, {});
			QCOMPARE(native64, {});
		}
		{
			QStringList failedFiles;
			auto dls = test->getDownloads(context("linux"), cache.get(), failedFiles,
										  QString("data"));
			QCOMPARE(dls.size(), 0);
			QCOMPARE(failedFiles, {});
		}
	}
	void test_onenine_native()
	{
		auto test = readMojangJson("data/lib-native.json");
		QStringList jar, native, native32, native64;
		test->getApplicableFiles(context("osx"), jar, native, native32, native64,
								 QString());
		QCOMPARE(jar, QStringList());
		QCOMPARE(native,
				 getStorage(
					 "org/lwjgl/lwjgl/lwjgl-platform/2.9.4-nightly-20150209/"
					 "lwjgl-platform-2.9.4-nightly-20150209-natives-osx.jar"));
		QCOMPARE(native32, {});
		QCOMPARE(native64, {});
		QStringList failedFiles;
		auto dls =
			test->getDownloads(context("osx"), cache.get(), failedFiles, QString());
		QCOMPARE(dls.size(), 1);
		QCOMPARE(failedFiles, {});
		QCOMPARE(dls[0]->m_url,
				 QUrl("https://libraries.minecraft.net/org/lwjgl/lwjgl/"
					  "lwjgl-platform/2.9.4-nightly-20150209/"
					  "lwjgl-platform-2.9.4-nightly-20150209-natives-osx.jar"));
	}
	void test_onenine_native_arch()
	{
		auto test = readMojangJson("data/lib-native-arch.json");
		QStringList jar, native, native32, native64;
		test->getApplicableFiles(context("windows"), jar, native, native32, native64,
								 QString());
		QCOMPARE(jar, {});
		QCOMPARE(native, {});
		QCOMPARE(native32,
				 getStorage("tv/twitch/twitch-platform/5.16/"
							"twitch-platform-5.16-natives-windows-32.jar"));
		QCOMPARE(native64,
				 getStorage("tv/twitch/twitch-platform/5.16/"
							"twitch-platform-5.16-natives-windows-64.jar"));
		QStringList failedFiles;
		auto dls =
			test->getDownloads(context("windows"), cache.get(), failedFiles, QString());
		QCOMPARE(dls.size(), 2);
		QCOMPARE(failedFiles, {});
		QCOMPARE(
			dls[0]->m_url,
			QUrl("https://libraries.minecraft.net/tv/twitch/twitch-platform/"
				 "5.16/twitch-platform-5.16-natives-windows-32.jar"));
		QCOMPARE(
			dls[1]->m_url,
			QUrl("https://libraries.minecraft.net/tv/twitch/twitch-platform/"
				 "5.16/twitch-platform-5.16-natives-windows-64.jar"));
	}

	// Plain platform names are for x86 and x86_64 only; any other
	// architecture matches its own "<system>-<arch>" name.
	void test_runtime_context_matches()
	{
		QVERIFY(context("osx").matches("osx"));
		QVERIFY(context("osx").matches("osx-x86_64"));
		QVERIFY(!context("osx").matches("osx-arm64"));
		QVERIFY(context("osx", "arm64").matches("osx-arm64"));
		QVERIFY(!context("osx", "arm64").matches("osx"));
		QVERIFY(!context("osx", "arm64").matches("linux-arm64"));
		QVERIFY(context("windows", "x86").matches("windows"));
		QVERIFY(context("linux", "arm32").matches("linux-arm32"));
		QVERIFY(!context("linux", "arm32").matches("linux"));
	}
	void test_runtime_context_architecture_names()
	{
		QCOMPARE(RuntimeContext::normalizeArchitecture("amd64"), "x86_64");
		QCOMPARE(RuntimeContext::normalizeArchitecture("x86_64"), "x86_64");
		QCOMPARE(RuntimeContext::normalizeArchitecture("aarch64"), "arm64");
		QCOMPARE(RuntimeContext::normalizeArchitecture("arm64"), "arm64");
		QCOMPARE(RuntimeContext::normalizeArchitecture("i686"), "x86");
		QCOMPARE(RuntimeContext::normalizeArchitecture("i386"), "x86");
		QCOMPARE(RuntimeContext::normalizeArchitecture("armhf"), "arm32");
		QCOMPARE(RuntimeContext::normalizeArchitecture("riscv64"), "riscv64");

		const auto arm = RuntimeContext::forJava("aarch64", "64");
		QCOMPARE(arm.system, OpSys_toString(currentSystem));
		QCOMPARE(arm.architecture, "arm64");
		QCOMPARE(arm.bitness, "64");
		// No Java checked yet: what Kite was built for.
		const auto unknown = RuntimeContext::forJava(QString(), QString());
		QCOMPARE(unknown.architecture, RuntimeContext::normalizeArchitecture(
										   QSysInfo::buildCpuArchitecture()));
		QVERIFY(!unknown.bitness.isEmpty());
	}
	// LWJGL 2 the way the meta has it: one natives classifier per platform.
	void test_natives_per_architecture()
	{
		auto lib = libraryFromJson(R"({
			"name": "org.lwjgl.lwjgl:lwjgl-platform:2.9.4-nightly-20150209",
			"natives": {
				"linux": "natives-linux",
				"linux-arm64": "natives-linux-arm64",
				"osx": "natives-osx",
				"osx-arm64": "natives-osx-arm64",
				"windows": "natives-windows"
			}
		})");
		const QString dir =
			"org/lwjgl/lwjgl/lwjgl-platform/2.9.4-nightly-20150209/";
		auto nativeFile = [&](const RuntimeContext& ctx) {
			QStringList jar, native, native32, native64;
			lib->getApplicableFiles(ctx, jar, native, native32, native64,
									QString());
			return native;
		};
		QVERIFY(lib->isActive(context("osx", "arm64")));
		QCOMPARE(nativeFile(context("osx", "arm64")),
				 getStorage(dir + "lwjgl-platform-2.9.4-nightly-20150209-"
								  "natives-osx-arm64.jar"));
		// An x86_64 Java under Rosetta gets the x86_64 natives.
		QCOMPARE(nativeFile(context("osx")),
				 getStorage(dir + "lwjgl-platform-2.9.4-nightly-20150209-"
								  "natives-osx.jar"));
		QCOMPARE(nativeFile(context("linux", "arm64")),
				 getStorage(dir + "lwjgl-platform-2.9.4-nightly-20150209-"
								  "natives-linux-arm64.jar"));
		// Nothing for an arm64 Java on Windows: the x86 natives would not
		// load into it.
		QVERIFY(!lib->isActive(context("windows", "arm64")));
		QVERIFY(lib->isActive(context("windows")));
	}
	// LWJGL 3.2.2 the way the meta has it: the x86 build everywhere but on
	// arm64 Macs, which get a build of 3.3.1 instead.
	void test_rules_per_architecture()
	{
		auto x86Build = libraryFromJson(R"({
			"name": "org.lwjgl:lwjgl:3.2.2",
			"rules": [
				{"action": "allow"},
				{"action": "disallow", "os": {"name": "linux-arm64"}},
				{"action": "disallow", "os": {"name": "osx-arm64"}}
			]
		})");
		auto armBuild = libraryFromJson(R"({
			"name": "org.lwjgl:lwjgl:3.3.1-mmachina.1",
			"rules": [{"action": "allow", "os": {"name": "osx-arm64"}}]
		})");
		QVERIFY(x86Build->isActive(context("osx")));
		QVERIFY(!armBuild->isActive(context("osx")));
		QVERIFY(!x86Build->isActive(context("osx", "arm64")));
		QVERIFY(armBuild->isActive(context("osx", "arm64")));
		QVERIFY(x86Build->isActive(context("windows", "arm64")));
		QVERIFY(!armBuild->isActive(context("linux", "arm64")));

		// Mojang's own "allow only on osx" means x86_64 Macs.
		auto objcBridge = libraryFromJson(R"({
			"name": "ca.weblite:java-objc-bridge:1.0.0",
			"rules": [{"action": "allow", "os": {"name": "osx"}}]
		})");
		QVERIFY(objcBridge->isActive(context("osx")));
		QVERIFY(!objcBridge->isActive(context("osx", "arm64")));
		QVERIFY(!objcBridge->isActive(context("linux")));
	}
	// A rule keeps its platform name as given when written back.
	void test_rule_keeps_platform_name()
	{
		auto lib = libraryFromJson(R"({
			"name": "org.lwjgl:lwjgl:3.3.1-mmachina.1",
			"rules": [{"action": "allow", "os": {"name": "osx-arm64"}}]
		})");
		const auto json = MojangVersionFormat::libraryToJson(lib.get());
		const auto rule = json.value("rules").toArray().at(0).toObject();
		QCOMPARE(rule.value("os").toObject().value("name").toString(),
				 "osx-arm64");
	}

  private:
	std::unique_ptr<HttpMetaCache> cache;
	QString dataDir;
};

QTEST_GUILESS_MAIN(LibraryTest)

#include "Library_test.moc"
