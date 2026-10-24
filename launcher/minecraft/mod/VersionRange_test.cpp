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

#include <QTest>

#include "minecraft/mod/VersionRange.h"

using VersionRange::Match;

namespace
{
	QString label(Match match)
	{
		switch (match) {
		case Match::Yes:
			return QStringLiteral("yes");
		case Match::No:
			return QStringLiteral("no");
		case Match::Unknown:
			break;
		}
		return QStringLiteral("unknown");
	}

	//! "<", "=", ">" or "?" for a version that cannot be ordered.
	QString label(const std::optional<int>& order)
	{
		if (!order)
			return QStringLiteral("?");
		if (*order < 0)
			return QStringLiteral("<");
		return *order > 0 ? QStringLiteral(">") : QStringLiteral("=");
	}

	QString reversed(const QString& symbol)
	{
		if (symbol == QLatin1String("<"))
			return QStringLiteral(">");
		return symbol == QLatin1String(">") ? QStringLiteral("<") : symbol;
	}

	ModDependency dependency(ModDependency::Syntax syntax,
							 const QStringList& versions)
	{
		ModDependency result;
		result.id = QStringLiteral("some-mod");
		result.syntax = syntax;
		result.versions = versions;
		return result;
	}
} // namespace

/*!
 * The version matchers answer from what a mod's metadata says, so every
 * test here is either a rule of the loader's own grammar or a case where
 * the honest answer is "don't know". The second kind matters as much as
 * the first: a No that should have been an Unknown is a false alarm about
 * a mod that works.
 *
 * Match and the orderings are compared as short words ("yes", "<") so that
 * a failing row reads on its own in the output.
 */
class VersionRangeTest : public QObject
{
	Q_OBJECT

  private slots:
	void compare_data()
	{
		QTest::addColumn<QString>("a");
		QTest::addColumn<QString>("b");
		QTest::addColumn<QString>("expected");

		const auto row = [](const char* name, const char* a, const char* b,
							const char* expected) {
			QTest::newRow(name) << QString::fromUtf8(a) << QString::fromUtf8(b)
								<< QString::fromUtf8(expected);
		};

		row("equal", "1.20.1", "1.20.1", "=");
		row("missing components are zero", "1.21", "1.21.0", "=");
		row("many zeros", "1", "1.0.0.0", "=");
		row("numeric, not text", "1.9", "1.10", "<");
		row("longer core wins", "1.20.1.5", "1.20.1", ">");
		row("big numbers", "0.92.2", "0.90", ">");
		row("leading v", "v1.2.3", "1.2.3", "=");
		row("surrounding space", " 1.2.3 ", "1.2.3", "=");

		row("pre-release below release", "1.0.0-alpha", "1.0.0", "<");
		row("rc below release", "1.0.0-rc.1", "1.0.0", "<");
		row("pre-release by text", "1.0.0-alpha", "1.0.0-beta", "<");
		row("numeric identifier by value", "1.0.0-beta.2", "1.0.0-beta.11",
			"<");
		row("numeric before text", "1.0.0-1", "1.0.0-alpha", "<");
		row("text identifiers as text", "1.0.0-alpha.1", "1.0.0-alpha.beta",
			"<");
		row("shorter prefix first", "1.0.0-alpha", "1.0.0-alpha.1", "<");
		row("core beats pre-release", "1.0.1-alpha", "1.0.0", ">");
		row("a release number looks like a pre-release", "1.20.1-4.2.0",
			"1.20.1", "<");

		row("build ignored", "1.2.3+build5", "1.2.3", "=");
		row("build with minecraft", "0.92.2+1.20.1", "0.92.2", "=");
		row("build after pre-release", "1.2.3-beta+x", "1.2.3-beta", "=");
		row("builds differ, still equal", "1.2.3+a", "1.2.3+b", "=");

		row("empty pre-release below pre-release", "1.20.2-", "1.20.2-alpha",
			"<");
		row("empty pre-release below release", "1.20.2-", "1.20.2", "<");
		row("empty pre-release above older core", "1.20.2-", "1.20.1", ">");
		row("two empty pre-releases", "1.20.2-", "1.20.2-", "=");

		row("snapshot", "23w45a", "1.20.3", "?");
		row("template", "${file.jarVersion}", "1.0", "?");
		row("word", "NONE", "1.0", "?");
		row("underscore qualifier", "1.20.1_beta", "1.0", "?");
		row("empty", "", "1.0", "?");
		row("only v", "v", "1.0", "?");
		row("trailing dot", "1.2.", "1.0", "?");
		row("leading dot", ".1", "1.0", "?");
		row("empty component", "1..2", "1.0", "?");
		row("empty identifier", "1.0-a..b", "1.0", "?");
		row("inner space", "1.0 beta", "1.0", "?");
		row("unreadable on the right", "1.0", "x", "?");
		row("both unreadable", "x", "y", "?");
		row("too many digits", "99999999999999999999", "1", "?");
	}

	void compare()
	{
		QFETCH(QString, a);
		QFETCH(QString, b);
		QFETCH(QString, expected);

		QCOMPARE(label(VersionRange::compare(a, b)), expected);
		// Ordering is the same read from the other side.
		QCOMPARE(label(VersionRange::compare(b, a)), reversed(expected));
	}

	void fabric_data()
	{
		QTest::addColumn<QString>("version");
		QTest::addColumn<QStringList>("alternatives");
		QTest::addColumn<QString>("expected");

		const auto row = [](const char* name, const char* version,
							const QStringList& alternatives,
							const char* expected) {
			QTest::newRow(name)
				<< QString::fromUtf8(version) << alternatives
				<< QString::fromUtf8(expected);
		};

		row("any, listed", "0.5", { "*" }, "yes");
		row("any, by x", "0.5", { "x" }, "yes");
		row("nothing asked", "0.5", {}, "yes");
		row("empty alternative", "0.5", { "" }, "yes");

		row(">= equal", "0.90", { ">=0.90" }, "yes");
		row(">= above", "0.92.2", { ">=0.90" }, "yes");
		row(">= below", "0.89", { ">=0.90" }, "no");
		row("> equal", "1.0", { ">1.0" }, "no");
		row("> above", "1.0.1", { ">1.0" }, "yes");
		row("< equal", "1.21", { "<1.21" }, "no");
		row("< below", "1.20.6", { "<1.21" }, "yes");
		row("<= equal", "1.21", { "<=1.21" }, "yes");
		row("<= above", "1.21.1", { "<=1.21" }, "no");
		row("bare is exact", "1.20.1", { "1.20.1" }, "yes");
		row("bare, other version", "1.20.2", { "1.20.1" }, "no");
		row("= is exact", "1.20.1", { "=1.20.1" }, "yes");
		row("exact, padded", "1.20", { "1.20.0" }, "yes");
		row("build ignored", "0.92.2+1.20.1", { ">=0.92.2" }, "yes");
		row("release number below its release", "1.20.1-4.2.0", { ">=1.20.1" },
			"no");

		row("~ at the bottom", "1.2.3", { "~1.2.3" }, "yes");
		row("~ within the minor", "1.2.9", { "~1.2.3" }, "yes");
		row("~ below", "1.2.2", { "~1.2.3" }, "no");
		row("~ next minor", "1.3.0", { "~1.2.3" }, "no");
		row("~ next minor's pre-release", "1.3.0-beta", { "~1.2.3" }, "no");
		row("~ short form", "1.0.9", { "~1" }, "yes");
		row("~ short form, next minor", "1.1", { "~1" }, "no");
		row("^ at the bottom", "1.2.3", { "^1.2.3" }, "yes");
		row("^ within the major", "1.99.0", { "^1.2.3" }, "yes");
		row("^ next major", "2.0.0", { "^1.2.3" }, "no");
		row("^ below", "1.2.2", { "^1.2.3" }, "no");
		row("^ is next major even for 0.x", "0.9.0", { "^0.5.3" }, "yes");
		row("^ 0.x next major", "1.0.0", { "^0.5.3" }, "no");

		row("wildcard x", "1.20.1", { "1.20.x" }, "yes");
		row("wildcard, shorter version", "1.20", { "1.20.x" }, "yes");
		row("wildcard ignores pre-release", "1.20.4-beta", { "1.20.x" }, "yes");
		row("wildcard, other minor", "1.21", { "1.20.x" }, "no");
		row("wildcard, older minor", "1.19.4", { "1.20.x" }, "no");
		row("wildcard major", "1.99", { "1.x" }, "yes");
		row("wildcard major, other", "2.0", { "1.x" }, "no");
		row("wildcard star", "1.20.6", { "1.20.*" }, "yes");
		row("wildcard capital", "1.20.6", { "1.20.X" }, "yes");
		row("wildcard on an unreadable version", "23w45a", { "1.20.x" },
			"unknown");
		row("wildcard not at the end", "1.2.3", { "1.x.3" }, "unknown");
		row("wildcard under >=", "1.2.3", { ">=1.x" }, "unknown");

		row("AND, inside", "1.20.1", { ">=1.20 <1.21" }, "yes");
		row("AND, above", "1.21", { ">=1.20 <1.21" }, "no");
		row("AND, below", "1.19", { ">=1.20 <1.21" }, "no");
		row("AND, extra space", "1.20.1", { "  >=1.20   <1.21 " }, "yes");
		row("OR, first", "1.20.1", { ">=1.20 <1.20.2", "1.21" }, "yes");
		row("OR, second", "1.21", { ">=1.20 <1.20.2", "1.21" }, "yes");
		row("OR, neither", "1.20.3", { ">=1.20 <1.20.2", "1.21" }, "no");

		// Tri-state: the version cannot be read.
		row("unreadable, any", "23w45a", { "*" }, "yes");
		row("unreadable, any, empty version", "", { "*" }, "yes");
		row("unreadable, nothing asked", "${v}", {}, "yes");
		row("unreadable, bounded", "23w45a", { ">=1.0" }, "unknown");
		row("unreadable, exact", "", { "1.0" }, "unknown");
		row("unreadable, any OR bounded", "23w45a", { "*", ">=1.0" }, "yes");
		row("unreadable, bounded OR bounded", "23w45a", { ">=1.0", "<1.0" },
			"unknown");
		row("unreadable, any AND bounded", "23w45a", { "* >=1.0" }, "unknown");

		// Tri-state: a predicate that cannot be read.
		row("predicate unreadable alone", "1.0", { ">=${min}" }, "unknown");
		row("operator alone", "1.0", { ">=" }, "unknown");
		row("garbage", "1.0", { "latest" }, "unknown");
		row("a No beats an unknown in AND", "0.5", { ">=1.0 <${max}" }, "no");
		row("an unknown with a Yes in AND", "1.5", { ">=1.0 <${max}" },
			"unknown");
		row("a Yes beats an unknown in OR", "0.5", { "<1.0", ">=${min}" },
			"yes");
		row("an unknown with a No in OR", "2.0", { "<1.0", ">=${min}" },
			"unknown");
	}

	void fabric()
	{
		QFETCH(QString, version);
		QFETCH(QStringList, alternatives);
		QFETCH(QString, expected);

		QCOMPARE(label(VersionRange::matchesFabric(version, alternatives)),
				 expected);
	}

	void maven_data()
	{
		QTest::addColumn<QString>("version");
		QTest::addColumn<QString>("range");
		QTest::addColumn<QString>("expected");

		const auto row = [](const char* name, const char* version,
							const char* range, const char* expected) {
			QTest::newRow(name) << QString::fromUtf8(version)
								<< QString::fromUtf8(range)
								<< QString::fromUtf8(expected);
		};

		row("empty is any", "47.2.0", "", "yes");
		row("star is any", "47.2.0", "*", "yes");
		row("any, unreadable", "${file.jarVersion}", "", "yes");
		row("any, empty version", "", "*", "yes");
		row("bare version asks for nothing", "99", "1.20.1", "yes");
		row("bare version, unreadable", "", "1.20.1", "yes");
		row("bare version that is no version", "1.0", "latest", "unknown");
		row("bare with a comma", "1.0", "1.0,2.0", "unknown");

		row("exact", "1.20.1", "[1.20.1]", "yes");
		row("exact, padded", "1.20.1.0", "[1.20.1]", "yes");
		row("exact, other", "1.20", "[1.20.1]", "no");
		row("exact, empty version", "", "[1.20.1]", "unknown");

		row("[a,b) at the bottom", "1.20", "[1.20,1.21)", "yes");
		row("[a,b) inside", "1.20.4", "[1.20,1.21)", "yes");
		row("[a,b) at the top", "1.21", "[1.20,1.21)", "no");
		row("[a,b) below", "1.19.4", "[1.20,1.21)", "no");
		row("(a,b] at the bottom", "1.20", "(1.20,1.21]", "no");
		row("(a,b] at the top", "1.21", "(1.20,1.21]", "yes");
		row("[a,b] at the top", "1.21", "[1.20,1.21]", "yes");
		row("(a,b) both ends out", "1.21", "(1.20,1.21)", "no");

		row("[a,) at a", "47", "[47,)", "yes");
		row("[a,) above", "47.2.0", "[47,)", "yes");
		row("[a,) below", "46.9", "[47,)", "no");
		row("(a,) at a", "47", "(47,)", "no");
		row("(a,) above", "47.0.1", "(47,)", "yes");
		row("(,b) at b", "1.21", "(,1.21)", "no");
		row("(,b) below", "1.20", "(,1.21)", "yes");
		row("(,b] at b", "1.21", "(,1.21]", "yes");
		row("[a,] is open at the top", "99", "[47,]", "yes");
		row("space around", "1.5", " [ 1.0 , 2.0 ) ", "yes");

		row("several, first", "1.5", "[1,2),[3,)", "yes");
		row("several, gap", "2.5", "[1,2),[3,)", "no");
		row("several, last", "3", "[1,2),[3,)", "yes");
		row("several, spaced", "3", "[1,2), [3,)", "yes");

		row("rc below its release", "1.0-rc1", "[1.0,)", "no");
		row("rc above beta", "1.0-rc1", "[1.0-beta,)", "yes");
		row("beta number", "1.0-beta2", "[1.0-beta10,)", "no");
		row("snapshot below release", "1.0-SNAPSHOT", "(,1.0)", "yes");
		row("alpha below beta", "1.0-alpha", "(,1.0-beta]", "yes");
		row("pre between beta and rc", "1.0-pre1", "[1.0-beta,1.0-rc1)", "yes");
		row("qualifier with a dot", "1.0-rc.2", "[1.0-rc.1,1.0)", "yes");
		row("qualifier case", "1.0-RC1", "(,1.0)", "yes");

		row("build above its release", "1.20.1-47.2.0", "(1.20.1,)", "yes");
		row("build inside the core's range", "1.20.1-47.2.0", "[1.20.1,1.20.2)",
			"yes");
		row("build below next core", "1.20.1-47.2.0", "[1.20.2,)", "no");
		row("build not at or below release", "1.20.1-47.2.0", "(,1.20.1]",
			"no");
		row("build exact", "1.20.1-47.2.0", "[1.20.1-47.2.0]", "yes");
		row("build, later build wanted", "1.20.1-47.2.0", "[1.20.1-47.3.0,)",
			"no");
		row("a build is not a pre-release", "1.0-1", "[1.0-rc1,)", "yes");

		row("unknown qualifier", "1.0-foo", "[1.0,)", "unknown");
		row("unknown qualifier in the range", "1.0", "[1.0-foo,)", "unknown");
		row("build metadata", "1.0+mc1.20", "[1.0,)", "unknown");
		row("template", "${file.jarVersion}", "[1,)", "unknown");
		row("empty version", "", "[1,)", "unknown");
		row("snapshot id", "23w45a", "[1,)", "unknown");
		row("v prefix is not Maven", "v1.0", "[1,)", "unknown");

		row("unclosed", "1.5", "[1,2", "unknown");
		row("three ends", "1.5", "[1,2,3)", "unknown");
		row("round exact", "1", "(1)", "unknown");
		row("empty brackets", "1", "[]", "unknown");
		row("both ends open", "1", "(,)", "unknown");
		row("junk after", "1.5", "[1,2)x", "unknown");
		row("comma at the end", "1.5", "[1,2),", "unknown");
		row("no comma between", "1.5", "[1,2)[3,)", "unknown");
		row("unreadable bound with Yes", "5", "[1,${max})", "unknown");
		row("unreadable bound with No", "0.5", "[1,${max})", "no");
	}

	void maven()
	{
		QFETCH(QString, version);
		QFETCH(QString, range);
		QFETCH(QString, expected);

		QCOMPARE(label(VersionRange::matchesMaven(version, range)), expected);
	}

	void matches_usesTheDependencysSyntax()
	{
		const ModDependency fabricDep =
			dependency(ModDependency::Syntax::Fabric, { ">=1.0 <2" });
		QCOMPARE(label(VersionRange::matches(QStringLiteral("1.5"), fabricDep)),
				 QStringLiteral("yes"));
		QCOMPARE(label(VersionRange::matches(QStringLiteral("2.5"), fabricDep)),
				 QStringLiteral("no"));

		// The same text means nothing to Maven.
		const ModDependency mavenDep =
			dependency(ModDependency::Syntax::Maven, { "[1.0,2)" });
		QCOMPARE(label(VersionRange::matches(QStringLiteral("1.5"), mavenDep)),
				 QStringLiteral("yes"));
		QCOMPARE(label(VersionRange::matches(QStringLiteral("2.5"), mavenDep)),
				 QStringLiteral("no"));
		QCOMPARE(label(VersionRange::matches(
					 QStringLiteral("1.5"),
					 dependency(ModDependency::Syntax::Maven, { ">=1.0 <2" }))),
				 QStringLiteral("unknown"));
	}

	void matches_noVersionsIsAny()
	{
		for (const auto syntax :
			 { ModDependency::Syntax::Fabric, ModDependency::Syntax::Maven }) {
			const ModDependency any = dependency(syntax, {});
			QCOMPARE(label(VersionRange::matches(QStringLiteral("1.0"), any)),
					 QStringLiteral("yes"));
			QCOMPARE(label(VersionRange::matches(QString(), any)),
					 QStringLiteral("yes"));
		}
	}

	void matches_severalMavenRangesAreAlternatives()
	{
		const ModDependency several = dependency(ModDependency::Syntax::Maven,
												 { "[1,2)", "[5,6)" });
		QCOMPARE(label(VersionRange::matches(QStringLiteral("5.5"), several)),
				 QStringLiteral("yes"));
		QCOMPARE(label(VersionRange::matches(QStringLiteral("3"), several)),
				 QStringLiteral("no"));
	}

	void describe_data()
	{
		QTest::addColumn<int>("syntax");
		QTest::addColumn<QStringList>("versions");
		QTest::addColumn<QString>("expected");

		const auto fabricRow = [](const char* name, const QStringList& versions,
							   const char* expected) {
			QTest::newRow(name)
				<< int(ModDependency::Syntax::Fabric) << versions
				<< QString::fromUtf8(expected);
		};
		const auto mavenRow = [](const char* name, const QStringList& versions,
							  const char* expected) {
			QTest::newRow(name)
				<< int(ModDependency::Syntax::Maven) << versions
				<< QString::fromUtf8(expected);
		};

		fabricRow("fabric >=", { ">=0.92.0" }, "0.92.0 or newer");
		fabricRow("fabric >", { ">1.0" }, "newer than 1.0");
		fabricRow("fabric <", { "<1.21" }, "older than 1.21");
		fabricRow("fabric <=", { "<=1.21" }, "1.21 or older");
		fabricRow("fabric bare", { "1.20.1" }, "1.20.1");
		fabricRow("fabric =", { "=1.20.1" }, "1.20.1");
		fabricRow("fabric range", { ">=1.20 <1.21" },
				  "1.20 or newer, below 1.21");
		fabricRow("fabric range, spaced", { "  >=1.20   <1.21 " },
			   "1.20 or newer, below 1.21");
		fabricRow("fabric wildcard", { "1.20.x" }, "1.20.x");
		fabricRow("fabric star", { "*" }, "");
		fabricRow("fabric nothing", {}, "");
		fabricRow("fabric empty text", { "" }, "");
		fabricRow("fabric any among others", { "*", ">=1" }, "");
		fabricRow("fabric tilde", { "~0.5.3" }, "~0.5.3");
		fabricRow("fabric caret", { "^1.2" }, "^1.2");
		fabricRow("fabric exclusive range", { ">1.0 <2.0" }, ">1.0 <2.0");
		fabricRow("fabric inclusive range", { ">=1.0 <=2.0" }, ">=1.0 <=2.0");
		fabricRow("fabric two lower bounds", { ">=1 >=2" }, ">=1 >=2");
		fabricRow("fabric alternatives", { ">=1.0", "<0.5" }, ">=1.0 or <0.5");
		fabricRow("fabric alternative versions", { "1.20", "1.20.1" },
			   "1.20 or 1.20.1");
		fabricRow("fabric operator alone", { ">=" }, ">=");

		mavenRow("maven [a,)", { "[47,)" }, "47 or newer");
		mavenRow("maven (a,)", { "(47,)" }, "newer than 47");
		mavenRow("maven (,b)", { "(,1.21)" }, "older than 1.21");
		mavenRow("maven (,b]", { "(,1.21]" }, "1.21 or older");
		mavenRow("maven [a]", { "[1.20.1]" }, "1.20.1");
		mavenRow("maven [a,b)", { "[1.20,1.21)" }, "1.20 or newer, below 1.21");
		mavenRow("maven (a,b]", { "(1.20,1.21]" }, "(1.20,1.21]");
		mavenRow("maven [a,b]", { "[1.20,1.21]" }, "[1.20,1.21]");
		mavenRow("maven several", { "[1,2),[3,)" }, "[1,2),[3,)");
		mavenRow("maven empty", { "" }, "");
		mavenRow("maven star", { "*" }, "");
		mavenRow("maven nothing", {}, "");
		mavenRow("maven bare version is no demand", { "1.20.1" }, "");
		mavenRow("maven malformed", { "[1,2" }, "[1,2");
		mavenRow("maven spaced", { " [47,) " }, "47 or newer");
	}

	void describe()
	{
		QFETCH(int, syntax);
		QFETCH(QStringList, versions);
		QFETCH(QString, expected);

		const ModDependency subject =
			dependency(ModDependency::Syntax(syntax), versions);
		QCOMPARE(VersionRange::describe(subject), expected);
	}
};

QTEST_GUILESS_MAIN(VersionRangeTest)

#include "VersionRange_test.moc"
