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
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTest>

#include "crashdoctor/CrashActions.h"
#include "crashdoctor/CrashCollector.h"
#include "crashdoctor/CrashEngine.h"
#include "crashdoctor/CrashRules.h"

using namespace CrashDoctor;

/*
 * Every case in testdata/expected.json is one row: real crashes from the
 * user's own instances (testdata/real, scrubbed) and cases written from the
 * loaders' message templates (testdata/synthetic). Each names the cause that
 * must come first, its captures, and causes that must or must not be there.
 * The same cases drive the reference engine in the catalog's build script, so
 * the catalog and this engine cannot drift apart unnoticed.
 */
namespace
{
	constexpr int kCatalogRules = 41;
	constexpr int kBudgetMs = 8000;
	constexpr int kNoiseLines = 20000;

	QByteArray readFile(const QString& path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}

	Catalog embeddedCatalog()
	{
		QString error;
		QStringList skipped;
		const auto catalog =
			Catalog::parse(readFile(QStringLiteral(":/kite/crash-rules.json")), &error, &skipped);
		return catalog.value_or(Catalog());
	}

	QSet<QString> stringSet(const QJsonValue& value)
	{
		QSet<QString> out;
		for (const QJsonValue& item : value.toArray()) {
			out.insert(item.toString());
		}
		return out;
	}

	Facts factsFrom(const QJsonObject& json)
	{
		Facts facts;
		facts.minecraft = json.value("mc").toString();
		facts.loader = json.value("loader").toString();
		facts.os = json.value("os").toString();
		facts.arch = json.value("arch").toString();
		facts.javaNow = json.value("javaNow").toInt();
		facts.javaRecommended = json.value("javaRecommended").toInt();
		facts.memoryMb = json.value("memoryMb").toInt();
		facts.systemMemoryMb = json.value("systemMemoryMb").toInt();
		facts.customJvmArgs = json.value("customJvmArgs").toBool();
		if (json.contains("exitCode")) {
			facts.exitCode = json.value("exitCode").toInt();
		}
		facts.exitStatus = json.value("exitStatus").toString();
		facts.installed = stringSet(json.value("installed"));
		facts.performanceMods = stringSet(json.value("performanceMods"));
		return facts;
	}

	/// The real excerpts start with "# source:" / "# kind:" lines.
	QString withoutHeader(const QString& text)
	{
		QStringList kept;
		for (const QString& line : text.split(QLatin1Char('\n'))) {
			if (!line.startsWith(QLatin1String("# source:")) &&
				!line.startsWith(QLatin1String("# kind:"))) {
				kept << line;
			}
		}
		return kept.join(QLatin1Char('\n'));
	}

	void addDocument(Evidence& evidence, const QString& source, const QString& text)
	{
		if (source == QLatin1String("log")) {
			evidence.log = text;
		} else if (source == QLatin1String("crashReport")) {
			evidence.crashReport = text;
		} else if (source == QLatin1String("hsErr")) {
			evidence.jvmError = text;
		}
	}

	Evidence evidenceFor(const QJsonObject& testCase, const QString& dataDir)
	{
		Evidence evidence;
		const QString file = QDir(dataDir).filePath(testCase.value("file").toString());
		if (testCase.value("kind").toString() == QLatin1String("real")) {
			addDocument(evidence, testCase.value("source").toString(),
						withoutHeader(QString::fromUtf8(readFile(file))));
			return evidence;
		}
		const QJsonObject docs =
			QJsonDocument::fromJson(readFile(file)).object().value("docs").toObject();
		for (auto it = docs.begin(); it != docs.end(); ++it) {
			addDocument(evidence, it.key(), it.value().toString());
		}
		return evidence;
	}

	QStringList ruleIds(const QList<Cause>& causes)
	{
		QStringList ids;
		for (const Cause& cause : causes) {
			ids << cause.ruleId;
		}
		return ids;
	}
} // namespace

class CrashEngineTest : public QObject
{
	Q_OBJECT

  private:
	QString m_dataDir;
	Catalog m_catalog;

  private slots:
	void initTestCase()
	{
		m_dataDir = QFileInfo(QFINDTESTDATA("testdata/expected.json")).absolutePath();
		QVERIFY(!m_dataDir.isEmpty());
		m_catalog = embeddedCatalog();
	}

	void embeddedCatalogLoadsWhole()
	{
		QString error;
		QStringList skipped;
		const auto catalog =
			Catalog::parse(readFile(QStringLiteral(":/kite/crash-rules.json")), &error, &skipped);
		QVERIFY2(catalog.has_value(), qPrintable(error));
		QVERIFY2(skipped.isEmpty(), qPrintable(skipped.join(QLatin1Char('\n'))));
		QCOMPARE(catalog->rules.size(), kCatalogRules);
		for (const Rule& rule : catalog->rules) {
			QVERIFY2(!rule.fixes.isEmpty() || rule.id.isEmpty(), qPrintable(rule.id));
		}
	}

	void corpus_data()
	{
		QTest::addColumn<QJsonObject>("testCase");
		const QString dir =
			QFileInfo(QFINDTESTDATA("testdata/expected.json")).absolutePath();
		const QJsonArray cases =
			QJsonDocument::fromJson(readFile(QDir(dir).filePath("expected.json"))).array();
		QVERIFY(!cases.isEmpty());
		for (const QJsonValue& value : cases) {
			const QJsonObject testCase = value.toObject();
			QTest::newRow(qPrintable(testCase.value("name").toString())) << testCase;
		}
	}

	void corpus()
	{
		QFETCH(QJsonObject, testCase);
		const QJsonObject expect = testCase.value("expect").toObject();
		const Facts facts = factsFrom(testCase.value("facts").toObject());
		const QList<Cause> causes =
			analyse(m_catalog, evidenceFor(testCase, m_dataDir), facts, QStringLiteral("tr"));
		const QStringList ids = ruleIds(causes);

		if (expect.value("top").isNull()) {
			QVERIFY2(causes.isEmpty(), qPrintable(ids.join(QStringLiteral(", "))));
			return;
		}
		QVERIFY2(!causes.isEmpty(), "no cause found");
		const Cause& top = causes.first();
		QCOMPARE(top.ruleId, expect.value("top").toString());
		if (expect.contains("count")) {
			QCOMPARE(int(top.findings.size()), expect.value("count").toInt());
		}
		const QVariantMap captures = top.findings.first().captures;
		const QJsonObject wanted = expect.value("captures").toObject();
		for (auto it = wanted.begin(); it != wanted.end(); ++it) {
			QVERIFY2(captures.contains(it.key()), qPrintable(it.key()));
			QCOMPARE(captures.value(it.key()).toString(), it.value().toVariant().toString());
		}
		const QJsonObject wantedFacts = expect.value("facts").toObject();
		if (wantedFacts.contains("suggestedMemoryMb")) {
			QCOMPARE(facts.suggestedMemoryMb(), wantedFacts.value("suggestedMemoryMb").toInt());
		}
		if (wantedFacts.contains("lowerMemoryMb")) {
			QCOMPARE(facts.lowerMemoryMb(), wantedFacts.value("lowerMemoryMb").toInt());
		}
		for (const QJsonValue& also : expect.value("also").toArray()) {
			QVERIFY2(ids.contains(also.toString()), qPrintable(also.toString()));
		}
		for (const QJsonValue& absent : expect.value("absent").toArray()) {
			QVERIFY2(!ids.contains(absent.toString()), qPrintable(absent.toString()));
		}
		// Words a person reads: filled in, in both languages.
		QVERIFY(!top.title.isEmpty());
		QVERIFY(!top.title.contains(QLatin1Char('{')));
		QVERIFY(!top.explanation.contains(QLatin1Char('{')));
		const QList<Cause> english =
			analyse(m_catalog, evidenceFor(testCase, m_dataDir), facts, QStringLiteral("en"));
		QCOMPARE(english.first().ruleId, top.ruleId);
		QVERIFY(english.first().title != top.title);
		// Every fix it offers is one of the closed set, with its parameters.
		for (const ResolvedFix& fix : top.fixes) {
			const CrashActions::Spec* spec = CrashActions::find(fix.action);
			QVERIFY2(spec, qPrintable(fix.action));
			for (const CrashActions::ParamSpec& param : spec->params) {
				QVERIFY2(param.optional || fix.params.contains(param.name),
						 qPrintable(fix.action + QLatin1Char('.') + param.name));
			}
			QVERIFY(!fix.label.isEmpty());
		}
	}

	// A grouped card does all its findings in one run.
	void groupedFixCoversEveryFinding()
	{
		const QString log = QStringLiteral(
			"\t - Mod 'Emotecraft' (emotecraft) 3.3.0 requires version 1.2.6 or later of "
			"player_animation_library, which is missing!\n"
			"\t - Mod 'Show Me Your Skin!' (showmeyourskin) 2.0.3 requires any version of cicada, "
			"which is missing!\n");
		Facts facts;
		facts.loader = QStringLiteral("fabric");
		facts.minecraft = QStringLiteral("26.1.2");
		const QList<Cause> causes = analyse(m_catalog, {log, {}, {}}, facts, QStringLiteral("tr"));
		QVERIFY(!causes.isEmpty());
		const Cause& top = causes.first();
		QCOMPARE(top.ruleId, QStringLiteral("mods.missing-dependency"));
		QCOMPARE(top.title, QStringLiteral("2 mod eksik"));
		QVERIFY(!top.fixes.isEmpty());
		QCOMPARE(top.fixes.first().action, QStringLiteral("installMissingDependency"));
		QCOMPARE(top.fixes.first().params.value("mod").toStringList(),
				 QStringList({"player_animation_library", "cicada"}));
		QCOMPARE(top.fixes.first().label, QStringLiteral("Hepsini yükle"));
	}

	// GPU fixes only where they mean something.
	void windowsOnlyFixIsHiddenElsewhere()
	{
		const QString log = QStringLiteral("GLFW error 65542: WGL: The driver does not appear to support OpenGL.\n");
		Facts facts;
		facts.loader = QStringLiteral("vanilla");
		facts.os = QStringLiteral("macos");
		const QList<Cause> mac = analyse(m_catalog, {log, {}, {}}, facts, QStringLiteral("en"));
		QVERIFY(!mac.isEmpty());
		QVERIFY(mac.first().fixes.isEmpty());
		facts.os = QStringLiteral("windows");
		const QList<Cause> windows = analyse(m_catalog, {log, {}, {}}, facts, QStringLiteral("en"));
		QCOMPARE(windows.first().fixes.first().action, QStringLiteral("useDedicatedGpu"));
	}

	// What a downloaded catalog cannot do: an unknown action, a PCRE verb, a
	// placeholder that is no capture, a pattern past the size limit, a capture
	// with no type. Each is left out on its own; the rest still loads.
	void hostileCatalogPartsAreLeftOut()
	{
		const auto rule = [](const QString& id, const QJsonObject& extra) {
			QJsonObject r{{"id", id},
						  {"priority", 50},
						  {"match", QJsonObject{{"any", QJsonArray{QJsonObject{{"re", "boom"}}}}}},
						  {"title", QJsonObject{{"en", "Boom"}, {"tr", "Bum"}}},
						  {"explanation", QJsonObject{{"en", "It went boom."}, {"tr", "Bum dedi."}}}};
			for (auto it = extra.begin(); it != extra.end(); ++it) {
				r.insert(it.key(), it.value());
			}
			return r;
		};
		const QJsonObject runCommand{
			{"fixes", QJsonArray{QJsonObject{{"action", "runCommand"},
											 {"params", QJsonObject{{"command", "rm -rf ~"}}},
											 {"label", QJsonObject{{"en", "Go"}, {"tr", "Git"}}}}}}};
		const QJsonObject extraParam{
			{"fixes", QJsonArray{QJsonObject{{"action", "setMemory"},
											 {"params", QJsonObject{{"maxMb", "4096"}, {"jvmArgs", "-Dx=1"}}},
											 {"label", QJsonObject{{"en", "Go"}, {"tr", "Git"}}}}}}};
		const QJsonObject verb{
			{"match", QJsonObject{{"any", QJsonArray{QJsonObject{{"re", "(*LIMIT_MATCH=999999999)boom"}}}}}}};
		const QJsonObject longPattern{
			{"match", QJsonObject{{"any", QJsonArray{QJsonObject{{"re", QString(kMaxPatternLength + 1, 'a')}}}}}}};
		const QJsonObject unknownPlaceholder{{"title", QJsonObject{{"en", "{secret}"}, {"tr", "{secret}"}}}};
		const QJsonObject untypedGroup{
			{"match", QJsonObject{{"any", QJsonArray{QJsonObject{{"re", "(?<path>/\\S+)"}}}}}}};
		const QJsonArray rules{rule("test.ok", {}),
							   rule("test.command", runCommand),
							   rule("test.extra-param", extraParam),
							   rule("test.verb", verb),
							   rule("test.long", longPattern),
							   rule("test.placeholder", unknownPlaceholder),
							   rule("test.untyped", untypedGroup)};
		const QJsonObject root{{"formatVersion", kFormatVersion}, {"catalogVersion", 1}, {"rules", rules}};
		QStringList skipped;
		const auto catalog = Catalog::parse(QJsonDocument(root).toJson(), nullptr, &skipped);
		QVERIFY(catalog.has_value());
		QStringList kept;
		for (const Rule& r : catalog->rules) {
			kept << r.id;
			QVERIFY(r.fixes.isEmpty());
		}
		QCOMPARE(kept, QStringList({"test.ok", "test.command", "test.extra-param"}));
		QCOMPARE(skipped.size(), 6);
	}

	void wrongFormatVersionIsRefused()
	{
		QString error;
		QVERIFY(!Catalog::parse(R"({"formatVersion": 2, "rules": []})", &error).has_value());
		QVERIFY(!error.isEmpty());
		QVERIFY(!Catalog::parse("not json", &error).has_value());
	}

	// A capture can't point outside the config folder or name another file type.
	void configPathsStayInside()
	{
		Facts facts;
		facts.loader = QStringLiteral("forge");
		const auto analyseLine = [&](const QString& file) {
			const QString log = QStringLiteral("Failed loading config file %1 of type CLIENT for modid create\n").arg(file);
			return analyse(m_catalog, {log, {}, {}}, facts, QStringLiteral("en"));
		};
		QCOMPARE(analyseLine("create-client.toml").first().fixes.first().params.value("file").toString(),
				 QStringLiteral("create-client.toml"));
		for (const QString& bad : {QStringLiteral("../../../.ssh/id.toml"), QStringLiteral("/etc/x.toml")}) {
			const QList<Cause> causes = analyseLine(bad);
			for (const Cause& cause : causes) {
				for (const ResolvedFix& fix : cause.fixes) {
					QVERIFY(fix.action != QLatin1String("resetConfigFile"));
				}
			}
		}
	}

	// Two megabytes of a busy log, with the cause at the end, in good time.
	void bigLogStaysFast()
	{
		QString log;
		auto* random = QRandomGenerator::global();
		for (int i = 0; i < kNoiseLines; ++i) {
			log += QStringLiteral("[12:00:%1] [Render thread/INFO]: Loaded %2 recipes from mod%3 at "
								  "net.minecraft.world.level.Level.tick(Level.java:%4) (sodium, iris, optifine)\n")
					   .arg(i % 60)
					   .arg(random->bounded(10000))
					   .arg(random->bounded(400))
					   .arg(random->bounded(9000));
		}
		log += QStringLiteral("java.lang.OutOfMemoryError: Java heap space\n");
		Facts facts;
		facts.loader = QStringLiteral("fabric");
		facts.memoryMb = 2048;
		facts.systemMemoryMb = 16384;
		QElapsedTimer timer;
		timer.start();
		const QList<Cause> causes = analyse(m_catalog, {log, {}, {}}, facts, QStringLiteral("en"));
		QVERIFY2(timer.elapsed() < kBudgetMs, qPrintable(QString::number(timer.elapsed())));
		QVERIFY(!causes.isEmpty());
		QCOMPARE(causes.first().ruleId, QStringLiteral("memory.heap-full"));
	}

	// Kite's own lines say how the game ended and which Java ran it.
	void exitAndJavaFromKiteLines()
	{
		const QString log = QStringLiteral(
			"Java is version 17.0.15, using 64-bit architecture, from Microsoft.\n"
			"[12:00:00] [main/INFO]: Process exited with code 3 in a mod's message\n"
			"Process crashed with exitcode 9.\n");
		const auto exit = exitFromLog(log);
		QVERIFY(exit.has_value());
		QCOMPARE(exit->first, 9);
		QCOMPARE(exit->second, QStringLiteral("crash"));
		QCOMPARE(exitFromLog(QStringLiteral("Process exited with code 0.\n"))->second,
				 QStringLiteral("normal"));
		QVERIFY(!exitFromLog(QStringLiteral("no exit here")).has_value());
		QCOMPARE(javaFromLog(log), 17);
		QCOMPARE(javaFromLog(QStringLiteral("nothing")), 0);
	}

	// Memory offers: never past what the computer can spare, never below 1 GB.
	void memoryOffers()
	{
		Facts facts;
		facts.loader = QStringLiteral("fabric");
		facts.memoryMb = 2048;
		facts.systemMemoryMb = 16384;
		QCOMPARE(facts.suggestedMemoryMb(), 4096);
		QVERIFY(facts.memoryRoom());
		facts.memoryMb = 12288;
		QVERIFY(facts.suggestedMemoryMb() <= 16384 - 4096);
		QVERIFY(!facts.memoryRoom());
		facts.memoryMb = 1024;
		facts.systemMemoryMb = 2048;
		QCOMPARE(facts.lowerMemoryMb(), 1024);
	}

	void normalizeStripsFormatting()
	{
		QCOMPARE(normalize(QString::fromUtf8("Mod \xc2\xa7" "ecreate\xc2\xa7r\r\nnext\x1b[31m!")),
				 QStringLiteral("Mod create\nnext!"));
	}
};

QTEST_GUILESS_MAIN(CrashEngineTest)

#include "CrashEngine_test.moc"
