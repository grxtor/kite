/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include <algorithm>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QXmlStreamReader>

/*
 * Can Qt's own icon engine serve the icon themes Kite ships inside qrc?
 *
 * For most of this launcher's history the answer was assumed to be no, and
 * every themed icon went through XdgIcon instead -- a fork of Qt's
 * QIconLoader, carried in libraries/iconfix, which existed for two reasons:
 * it put ":/icons" on the theme search path, and it never let a desktop
 * platform plugin substitute its own icon engine. Qt can be asked for both
 * of those directly, so the fork is gone and ThemeManager::setIconTheme()
 * asks. This test is what made that safe to do, and it is what keeps it
 * safe: everything below is the contract the fork used to guarantee by
 * owning the lookup itself.
 *
 * Deliberately written against the *public Qt API only*. Nothing here may
 * reach for a launcher class to resolve an icon: the point is to pin what Qt
 * does, so that a Qt upgrade that changes icon lookup fails here, in one
 * place, with a name and a theme in the message.
 *
 * What each case is worth:
 *
 *  - resourcesArePresent / searchPathAcceptsResourcePrefix
 *      the qrc themes are reachable at all, and are laid out the way the
 *      XDG spec (and therefore Qt's loader) requires.
 *  - unknownIconDoesNotResolve
 *      the NEGATIVE CONTROL, and the most important case here. Every other
 *      assertion is of the form "this name resolves". If the way this test
 *      asks that question answered "yes" unconditionally -- which is exactly
 *      what happens when a platform icon engine takes over and reports names
 *      it cannot actually draw -- all of them would pass while proving
 *      nothing. This case fails in that situation.
 *  - uiIconsResolveInEveryTheme / instanceIconsResolveInEveryTheme
 *      the actual coverage measurement, per theme and per name. Asks two
 *      questions of each: is there an image behind the name, and is it the
 *      image that was asked for. The second matters because of
 *      dashedNamesFallBackToTheirPrefix, below.
 *  - mainWindowUiIconNamesWereRead / mainWindowUiIconNamesAreAudited /
 *    mainWindowUiIconsResolveInEveryTheme /
 *    mainWindowOptionalIconsThatGoBlank
 *      the same coverage question asked of MainWindow.ui instead of a list
 *      kept by hand here. MainWindow::applyThemedIcons() takes its icon
 *      names from a "kiteIcon" dynamic property in the XML, so the
 *      compiler never sees them and a misspelling costs one blank button
 *      and one qWarning. These read the .ui back and make it a test
 *      failure instead. They need KITE_MAINWINDOW_UI_PATH, which
 *      launcher/CMakeLists.txt passes in.
 *  - missingIconIsNull
 *      a handful of pages ask for an icon and ask for a different one if
 *      that came back empty. This is the contract those branches rest on,
 *      and the quietest thing this migration could have broken.
 *  - dashedNamesFallBackToTheirPrefix
 *      Qt applies the XDG rule that a missing "foo-bar" falls back to "foo".
 *      The retired fork did not, so this is the one lookup behaviour the
 *      migration genuinely changed, and the failure mode it introduced is
 *      silent: the wrong icon, drawn without complaint.
 *  - inheritsIsFollowedAcrossThemes
 *      ten of the eleven bundled themes carry only their own overrides and
 *      declare "Inherits=multimc" for the rest, so Qt's loader has to read
 *      and follow Inherits inside qrc or most themes lose most icons.
 *  - themeSwitchIsNotCachedByName
 *      QIcon::fromTheme keeps a process-wide cache keyed by icon *name*.
 *      Every per-theme result above is only meaningful if switching the
 *      theme invalidates that cache; otherwise the first theme's icons are
 *      handed out for all eleven and the coverage numbers are fiction.
 *
 * Where this runs matters, and the two environments answer different
 * questions:
 *
 *   offscreen (how ctest registers it)  -- deterministic; measures layout,
 *       coverage and Inherits. No desktop platform theme is loaded, so it
 *       says nothing about the fork's second reason for existing.
 *   a real session (run the binary directly, e.g.
 *       QT_QPA_PLATFORM=wayland ./IconTheme_test)
 *       -- measures whether the desktop's platform theme hijacks icon
 *       lookup. On Plasma that would be KIconEngine, which resolves against
 *       KDE's icon directories and knows nothing about ":/icons".
 *
 * initTestCase() prints which of the two it got, so a passing run can be
 * told apart from a passing run that measured nothing.
 */

#ifndef KITE_MAINWINDOW_UI_PATH
#error "KITE_MAINWINDOW_UI_PATH is not set; see add_unit_test(IconTheme) in launcher/CMakeLists.txt"
#endif

namespace
{

/// The icon themes compiled into the binary, one qrc each. Keep in step with
/// KITE_QRC_FILES in launcher/CMakeLists.txt (minus the ones that are not
/// icon themes: backgrounds, documents, shaders).
const QStringList kBundledThemes = {
	QStringLiteral("multimc"),      QStringLiteral("pe_dark"),
	QStringLiteral("pe_light"),     QStringLiteral("pe_colored"),
	QStringLiteral("pe_blue"),      QStringLiteral("OSX"),
	QStringLiteral("iOS"),          QStringLiteral("flat"),
	QStringLiteral("flat_white"),   QStringLiteral("breeze_dark"),
	QStringLiteral("breeze_light"),
};

/* Every name the launcher asks getThemedIcon() for, plus the ones only the
 * instance list asks for. Regenerate with:
 *
 *   grep -rhoP 'getThemedIcon\("\K[^"]+' launcher/ | sort -u
 *
 * These are the names a theme has to carry: their call sites use whatever
 * comes back, so a theme missing one of them leaves a blank space on screen.
 *
 * Names deliberately absent from this list:
 *   "logo"  -- Application::getThemedIcon() answers it from a fixed resource
 *              path, before any theme is consulted.
 *   the entries of kOptionalUiIconNames, below. */
const QStringList kUiIconNames = {
	QStringLiteral("about"),
	QStringLiteral("appearance"),
	QStringLiteral("atlauncher"),
	QStringLiteral("atlauncher-placeholder"),
	QStringLiteral("backup"),
	QStringLiteral("bug"),
	QStringLiteral("cat"),
	QStringLiteral("centralmods"),
	QStringLiteral("checkupdate"),
	QStringLiteral("copy"),
	QStringLiteral("custom-commands"),
	QStringLiteral("delete"),
	QStringLiteral("discord"),
	QStringLiteral("export"),
	QStringLiteral("flame"),
	QStringLiteral("ftb_logo"),
	QStringLiteral("help"),
	QStringLiteral("instance-settings"),
	QStringLiteral("java"),
	QStringLiteral("language"),
	QStringLiteral("launch"),
	QStringLiteral("launcher"),
	QStringLiteral("loadermods"),
	QStringLiteral("log"),
	QStringLiteral("minecraft"),
	QStringLiteral("modrinth"),
	QStringLiteral("new"),
	QStringLiteral("news"),
	QStringLiteral("noaccount"),
	QStringLiteral("patreon"),
	QStringLiteral("plugins"),
	QStringLiteral("proxy"),
	QStringLiteral("reddit-alien"),
	QStringLiteral("rename"),
	QStringLiteral("resourcepacks"),
	QStringLiteral("screenshot-placeholder"),
	QStringLiteral("screenshots"),
	QStringLiteral("settings"),
	QStringLiteral("shortcut"),
	QStringLiteral("star"),
	QStringLiteral("status-bad"),
	QStringLiteral("status-good"),
	/* Only ever asked for by InstanceDelegate::drawBadges(), so it does not
	 * turn up in the getThemedIcon() grep above. */
	QStringLiteral("status-running"),
	QStringLiteral("status-yellow"),
	QStringLiteral("tag"),
	QStringLiteral("technic"),
	QStringLiteral("unknown_server"),
	QStringLiteral("viewfolder"),
	QStringLiteral("worlds"),
};

/* Names a theme is allowed not to have, each because the one place that asks
 * for it tests the result and asks for something else instead:
 *
 *   externaltools -> loadermods   ExternalToolsPage::icon()
 *   notes         -> news         NotesPage::icon()
 *   accounts      -> noaccount    AccountListPage::icon()
 *
 * So these are not gaps, they are a design: the multimc theme is the
 * MultiMC-era icon set and does not carry icons for everything this launcher
 * grew since. Nothing is ever drawn blank, which is why they are not in the
 * mandatory list above.
 *
 * Every fallback target here is itself in kUiIconNames, so the icon the user
 * actually ends up looking at is still covered -- that, and not the presence
 * of these three, is the property worth holding. What makes the whole
 * arrangement work is that a miss comes back null; missingIconIsNull() is
 * where that is pinned. */
const QStringList kOptionalUiIconNames = {
	QStringLiteral("externaltools"),
	QStringLiteral("notes"),
	QStringLiteral("accounts"),
};

/* The directories Application scans to discover the built-in instance icons.
 * Kept identical to the list it passes to IconList, because the names found
 * here are exactly the names that end up going through the theme lookup. */
const QStringList kInstanceIconDirs = {
	QStringLiteral(":/icons/multimc/32x32/instances/"),
	QStringLiteral(":/icons/multimc/50x50/instances/"),
	QStringLiteral(":/icons/multimc/128x128/instances/"),
	QStringLiteral(":/icons/multimc/scalable/instances/"),
};

/// Resolves like the launcher does, and reports what was actually obtained.
/// Two separate questions are asked on purpose:
///
///   hasThemeIcon()  -- Qt says the name belongs to the current theme. This
///                      alone is not evidence: it is answered by the icon
///                      engine's own idea of its name, which a hijacking
///                      platform engine will happily supply for an icon it
///                      cannot draw.
///   availableSizes()-- there is at least one real image behind the name.
///                      This is the load-bearing check, and it is also the
///                      one XdgIcon::fromTheme() itself uses to decide
///                      whether a lookup failed.
///   resolvedName    -- which icon Qt actually came back with. Not always the
///                      one that was asked for: Qt implements the XDG naming
///                      rule that drops dash-separated suffixes, so a missing
///                      "foo-bar" quietly turns into "foo" if that exists.
///                      The iconfix fork does no such thing, so this is a
///                      real behavioural difference and worth pinning.
struct Resolution {
	bool claimed = false;
	bool drawable = false;
	QString resolvedName;

	bool substituted(const QString& requested) const
	{
		return drawable && resolvedName != requested;
	}
};

Resolution resolve(const QString& name)
{
	Resolution result;
	result.claimed = QIcon::hasThemeIcon(name);

	const QIcon icon = QIcon::fromTheme(name);
	result.drawable = !icon.availableSizes().isEmpty();
	result.resolvedName = icon.name();
	return result;
}

/// Names of the instance icons shipped in the theme resources.
QStringList discoverInstanceIconNames()
{
	QSet<QString> names;
	for (const QString& dirPath : kInstanceIconDirs) {
		QDir dir(dirPath);
		const QFileInfoList entries =
			dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
		for (const QFileInfo& entry : entries) {
			names.insert(entry.baseName());
		}
	}
	QStringList sorted = names.values();
	sorted.sort();
	return sorted;
}

/// One "kiteIcon" property found in a .ui file: the icon name, and the
/// object that asked for it, so a failure names something greppable.
struct UiIconRequest {
	QString objectName;
	QString iconName;
};

/* Read every "kiteIcon" dynamic property out of a Designer .ui file.
 *
 * MainWindow::applyThemedIcons() walks its own children looking for exactly
 * this property and assigns whatever the name resolves to, so this is the
 * complete list of icon names MainWindow asks for by way of the .ui -- and
 * the list the compiler cannot check, since the names are XML text.
 *
 * Parsed with QXmlStreamReader rather than matched with a regular
 * expression, so that the structure the reader is relying on (the value is a
 * <string> child of the property element) is the structure that is actually
 * required, and a .ui that stops having it fails loudly instead of quietly
 * yielding nothing.
 */
QList<UiIconRequest> readUiIconRequests(const QString& uiPath, QString* error)
{
	QList<UiIconRequest> found;

	QFile file(uiPath);
	if (!file.open(QIODevice::ReadOnly)) {
		*error = uiPath + QStringLiteral(": ") + file.errorString();
		return found;
	}

	QXmlStreamReader xml(&file);

	/* The nearest enclosing named element. Designer nests a property inside
	 * the <action> or <widget> it belongs to, and nothing else carries a
	 * name we would want in a failure message. */
	QString owner;

	while (!xml.atEnd()) {
		if (xml.readNext() != QXmlStreamReader::StartElement) {
			continue;
		}

		const QString element = xml.name().toString();
		if (element == QStringLiteral("action") ||
			element == QStringLiteral("widget")) {
			owner = xml.attributes()
						.value(QStringLiteral("name"))
						.toString();
			continue;
		}
		if (element != QStringLiteral("property")) {
			continue;
		}
		if (xml.attributes().value(QStringLiteral("name")).toString() !=
			QStringLiteral("kiteIcon")) {
			continue;
		}

		QString value;
		while (!xml.atEnd()) {
			const QXmlStreamReader::TokenType token = xml.readNext();
			if (token == QXmlStreamReader::StartElement &&
				xml.name().toString() == QStringLiteral("string")) {
				value = xml.readElementText();
				break;
			}
			if (token == QXmlStreamReader::EndElement &&
				xml.name().toString() == QStringLiteral("property")) {
				break;
			}
		}

		if (value.isEmpty()) {
			*error = QStringLiteral("%1: the kiteIcon property on '%2' has "
									"no <string> value")
						 .arg(uiPath, owner);
			return found;
		}
		found.append({owner, value});
	}

	if (xml.hasError()) {
		*error = QStringLiteral("%1:%2: %3")
					 .arg(uiPath)
					 .arg(xml.lineNumber())
					 .arg(xml.errorString());
	}
	return found;
}

/// One rendered frame of an icon, for comparing "is this the same icon?"
/// without depending on any file path the engine picked.
QImage renderedIcon(const QString& name)
{
	return QIcon::fromTheme(name).pixmap(32).toImage();
}

/// Whether a real windowing system is available. Only used to pick a default
/// for QT_QPA_PLATFORM; an explicit setting always wins.
bool windowingSystemAvailable()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
	return true;
#else
	return qEnvironmentVariableIsSet("WAYLAND_DISPLAY") ||
		   qEnvironmentVariableIsSet("DISPLAY");
#endif
}

} // namespace

class IconThemeTest : public QObject
{
	Q_OBJECT

  private:
	QStringList m_instanceIconNames;

  private slots:
	void initTestCase()
	{
		/* The same thing ThemeManager::setIconTheme() does, restated here
		 * rather than called, so that this test measures Qt and not our
		 * wrapper around it. ":/icons" goes in front so the bundled themes
		 * win over a same-named theme installed on the host. */
		QStringList searchPaths = QIcon::themeSearchPaths();
		searchPaths.prepend(QStringLiteral(":/icons"));
		QIcon::setThemeSearchPaths(searchPaths);

		m_instanceIconNames = discoverInstanceIconNames();

		/* So that a green run can be read for what it actually covered. */
		qInfo().noquote() << "platform  :"
						  << QGuiApplication::platformName();
		qInfo().noquote() << "themehint :"
						  << (qEnvironmentVariableIsSet("QT_QPA_PLATFORMTHEME")
								  ? qEnvironmentVariable("QT_QPA_PLATFORMTHEME")
								  : QStringLiteral("<unset>"));
		qInfo().noquote() << "desktop   :"
						  << (qEnvironmentVariableIsSet("XDG_CURRENT_DESKTOP")
								  ? qEnvironmentVariable("XDG_CURRENT_DESKTOP")
								  : QStringLiteral("<unset>"));
		qInfo().noquote() << "ui icons  :" << kUiIconNames.size()
						  << "instance icons  :" << m_instanceIconNames.size()
						  << "themes  :" << kBundledThemes.size();
	}

	void cleanupTestCase()
	{
		QIcon::setThemeName(QString());
	}

	// -----------------------------------------------------------------
	// Layout
	// -----------------------------------------------------------------

	/// The themes are in the binary, and the test can see them. Guards the
	/// static-library trap: the compiled qrc lives in Kite_logic, and a
	/// linker is free to drop the object file that registers it if nothing
	/// references it.
	void resourcesArePresent()
	{
		for (const QString& theme : kBundledThemes) {
			const QString indexPath = ":/icons/" + theme + "/index.theme";
			QVERIFY2(QFile::exists(indexPath),
					 qPrintable(indexPath +
								" is missing: the icon theme resources were "
								"not registered in this binary"));
		}
	}

	/// Qt keeps a resource prefix in the theme search path, and the themes
	/// underneath it are shaped the way its loader expects: a directory per
	/// theme name, each with an index.theme at its root.
	void searchPathAcceptsResourcePrefix()
	{
		QVERIFY(QIcon::themeSearchPaths().contains(QStringLiteral(":/icons")));

		for (const QString& theme : kBundledThemes) {
			QFile index(":/icons/" + theme + "/index.theme");
			QVERIFY2(index.open(QIODevice::ReadOnly), qPrintable(theme));
			const QByteArray head = index.read(256);
			QVERIFY2(head.contains("[Icon Theme]"),
					 qPrintable(theme + ": index.theme has no [Icon Theme] "
										"section"));
		}
	}

	/// Instance icons are discovered by scanning qrc, so a scan that finds
	/// nothing would silently empty the coverage case below.
	void instanceIconsWereDiscovered()
	{
		QVERIFY2(!m_instanceIconNames.isEmpty(),
				 "no built-in instance icons found under :/icons/multimc");
		QVERIFY2(m_instanceIconNames.contains(QStringLiteral("grass")),
				 "the default instance icon 'grass' was not among them");
	}

	// -----------------------------------------------------------------
	// Negative control -- read the comment at the top of the file
	// -----------------------------------------------------------------

	/// A name no theme carries must not resolve, in every bundled theme.
	/// Without this, "everything resolved" is not a measurement.
	///
	/// The name has no dashes in it on purpose -- see
	/// dashedNamesFallBackToTheirPrefix() for why one with dashes would make
	/// this case lie.
	void unknownIconDoesNotResolve()
	{
		const QString bogus = QStringLiteral("kitenosuchiconanywhere");

		for (const QString& theme : kBundledThemes) {
			QIcon::setThemeName(theme);

			const Resolution got = resolve(bogus);
			QVERIFY2(!got.drawable,
					 qPrintable(theme + ": a nonexistent icon name produced a "
										"drawable icon, so icon lookup is not "
										"being served by the bundled themes"));
			QVERIFY2(!got.claimed,
					 qPrintable(theme + ": hasThemeIcon() claimed a "
										"nonexistent icon name"));
		}
	}

	/// A miss comes back as a *null* QIcon, not as an empty non-null one.
	///
	/// Three call sites are built on this -- ExternalToolsPage::icon(),
	/// NotesPage::icon() and AccountListPage::icon() all do
	/// "if (icon.isNull()) ask for something else". It is the reason
	/// kOptionalUiIconNames can be optional at all.
	///
	/// Worth its own case because it is the quietest way this migration
	/// could have gone wrong: the retired fork returned its `fallback`
	/// argument, defaulting to QIcon(), whenever availableSizes() came back
	/// empty. Qt reaching the same answer by its own route is not something
	/// to take on trust, and if a future Qt returns a non-null placeholder
	/// instead, those three isNull() branches stop firing and the pages go
	/// blank with nothing logged anywhere.
	void missingIconIsNull()
	{
		/* Asked for through a theme that really is missing two of the
		 * optional names, so this exercises a genuine miss rather than a
		 * synthetic one. */
		QIcon::setThemeName(QStringLiteral("multimc"));

		for (const QString& name : {QStringLiteral("externaltools"),
									QStringLiteral("notes"),
									QStringLiteral("kitenosuchiconanywhere")}) {
			const QIcon icon = QIcon::fromTheme(name);
			QVERIFY2(icon.isNull(),
					 qPrintable(name + ": a missing icon came back non-null, "
									   "so the isNull() fallbacks in "
									   "ExternalToolsPage/NotesPage/"
									   "AccountListPage no longer fire"));
		}

		/* And the icons those fallbacks land on are present here, which is
		 * what the user actually sees on those pages. */
		for (const QString& name : {QStringLiteral("loadermods"),
									QStringLiteral("news"),
									QStringLiteral("noaccount")}) {
			QVERIFY2(resolve(name).drawable,
					 qPrintable(name + ": the icon a fallback lands on is "
									   "missing from the multimc theme"));
		}
	}

	// -----------------------------------------------------------------
	// Coverage
	// -----------------------------------------------------------------

	/// Every icon name the UI asks for, in every bundled theme.
	///
	/// Collects the whole failure set instead of stopping at the first, so
	/// one run says exactly which theme is missing which icon -- the useful
	/// output if this ever goes red after an icon is added.
	void uiIconsResolveInEveryTheme()
	{
		QStringList notDrawable;
		QStringList substituted;

		for (const QString& theme : kBundledThemes) {
			QIcon::setThemeName(theme);

			for (const QString& name : kUiIconNames) {
				const Resolution got = resolve(name);
				if (!got.drawable) {
					notDrawable << (theme + "/" + name);
				} else if (got.substituted(name)) {
					substituted << (theme + "/" + name + " -> " +
									got.resolvedName);
				}
			}

			/* The optional ones are not required to be there, but if a
			 * theme does answer for one it had better be with the right
			 * picture, so they go through the substitution check too. */
			for (const QString& name : kOptionalUiIconNames) {
				const Resolution got = resolve(name);
				if (got.substituted(name)) {
					substituted << (theme + "/" + name + " -> " +
									got.resolvedName);
				}
			}
		}

		QVERIFY2(notDrawable.isEmpty(),
				 qPrintable(QStringLiteral("no image behind %1 theme/name "
										   "pairs: %2")
								.arg(notDrawable.size())
								.arg(notDrawable.join(QStringLiteral(", ")))));

		/* Worse than a missing icon, because it is invisible: the wrong
		 * picture is drawn and nothing anywhere reports a problem. Only
		 * reachable through the dash rule, so it means a theme is missing
		 * the dashed icon while carrying something named after its
		 * prefix. */
		QVERIFY2(substituted.isEmpty(),
				 qPrintable(QStringLiteral("%1 theme/name pairs silently "
										   "resolved to a DIFFERENT icon: %2")
								.arg(substituted.size())
								.arg(substituted.join(QStringLiteral(", ")))));
	}

	/// Pins the XDG dash rule itself, which is the one lookup behaviour Qt
	/// has and the iconfix fork does not.
	///
	/// Kept as its own case so that the difference is documented and
	/// deliberate rather than discovered again later: an icon named
	/// "<known>-<something>" that no theme carries comes back as "<known>".
	/// uiIconsResolveInEveryTheme() is what makes sure the launcher's real
	/// icon names never take that path.
	void dashedNamesFallBackToTheirPrefix()
	{
		QIcon::setThemeName(QStringLiteral("multimc"));

		const Resolution got = resolve(QStringLiteral("launch-no-such-variant"));
		QVERIFY2(got.drawable,
				 "Qt no longer applies the XDG dash rule; if that is "
				 "intentional the silent-substitution check in "
				 "uiIconsResolveInEveryTheme() is now dead weight");
		QCOMPARE(got.resolvedName, QStringLiteral("launch"));

		/* hasThemeIcon() does not join in: it compares the resolved name
		 * with the requested one, which is why it is not enough on its own
		 * to tell "found" from "found something else". */
		QVERIFY(!QIcon::hasThemeIcon(QStringLiteral("launch-no-such-variant")));
	}

	/// The built-in instance icons, in every bundled theme. These live in
	/// "instances" subdirectories that only the multimc theme actually has,
	/// so this case leans entirely on Inherits.
	void instanceIconsResolveInEveryTheme()
	{
		QStringList missing;
		QStringList substituted;

		for (const QString& theme : kBundledThemes) {
			QIcon::setThemeName(theme);

			for (const QString& name : m_instanceIconNames) {
				const Resolution got = resolve(name);
				if (!got.drawable) {
					missing << (theme + "/" + name);
				} else if (got.substituted(name)) {
					substituted << (theme + "/" + name + " -> " +
									got.resolvedName);
				}
			}
		}

		QVERIFY2(missing.isEmpty(),
				 qPrintable(QStringLiteral("no image behind %1 instance "
										   "theme/name pairs: %2")
								.arg(missing.size())
								.arg(missing.join(QStringLiteral(", ")))));
		QVERIFY2(substituted.isEmpty(),
				 qPrintable(QStringLiteral("%1 instance theme/name pairs "
										   "silently resolved to a DIFFERENT "
										   "icon: %2")
								.arg(substituted.size())
								.arg(substituted.join(QStringLiteral(", ")))));
	}

	// -----------------------------------------------------------------
	// The names in MainWindow.ui
	// -----------------------------------------------------------------

	/// The .ui is readable and really does name icons, so the two cases
	/// below cannot pass by measuring an empty list.
	void mainWindowUiIconNamesWereRead()
	{
		QString error;
		const QList<UiIconRequest> requests =
			readUiIconRequests(QStringLiteral(KITE_MAINWINDOW_UI_PATH),
							   &error);

		QVERIFY2(error.isEmpty(), qPrintable(error));
		QVERIFY2(!requests.isEmpty(),
				 "no kiteIcon properties found in MainWindow.ui, so "
				 "MainWindow::applyThemedIcons() has nothing to do -- either "
				 "the property was renamed or the .ui moved");

		QSet<QString> names;
		for (const UiIconRequest& request : requests) {
			QVERIFY2(!request.objectName.isEmpty(),
					 qPrintable(QStringLiteral(
									"a kiteIcon property ('%1') is not "
									"inside a named action or widget")
									.arg(request.iconName)));
			names.insert(request.iconName);
		}

		qInfo().noquote() << ".ui icon requests:" << requests.size()
						  << "distinct names:" << names.size();
	}

	/*!
	 * Every icon name in MainWindow.ui is one this file already audits.
	 *
	 * This is the case that turns a .ui typo from a log line into a test
	 * failure. applyThemedIcons() finds its icons by walking children for a
	 * dynamic property, so the names live in XML and the compiler never
	 * sees them: "setttings" would build, link, ship, and leave one blank
	 * toolbar button plus a qWarning nobody reads. Here it fails with the
	 * name and the action in the message.
	 *
	 * It holds the reverse too, which matters more over time: kUiIconNames
	 * is maintained by hand from a grep of getThemedIcon() calls, and that
	 * grep cannot see the .ui at all. Anything moved from C++ into the .ui
	 * would silently drop out of the coverage cases above without this.
	 */
	void mainWindowUiIconNamesAreAudited()
	{
		QString error;
		const QList<UiIconRequest> requests =
			readUiIconRequests(QStringLiteral(KITE_MAINWINDOW_UI_PATH),
							   &error);
		QVERIFY2(error.isEmpty(), qPrintable(error));
		QVERIFY(!requests.isEmpty());

		QStringList unaudited;
		for (const UiIconRequest& request : requests) {
			if (kUiIconNames.contains(request.iconName) ||
				kOptionalUiIconNames.contains(request.iconName)) {
				continue;
			}
			unaudited << (request.objectName + QStringLiteral("/") +
						  request.iconName);
		}

		QVERIFY2(unaudited.isEmpty(),
				 qPrintable(QStringLiteral(
								"MainWindow.ui asks for %1 icon name(s) that "
								"this test does not cover -- add them to "
								"kUiIconNames, or fix the typo: %2")
								.arg(unaudited.size())
								.arg(unaudited.join(QStringLiteral(", ")))));
	}

	/*!
	 * Every mandatory name in MainWindow.ui draws something, in every
	 * bundled theme, and draws the icon it asked for.
	 *
	 * Overlaps uiIconsResolveInEveryTheme() by design. That case measures a
	 * hand-maintained list; this one measures the file MainWindow actually
	 * reads, so the .ui is the source of truth for its own icons and the
	 * two cannot drift apart silently.
	 *
	 * The kOptionalUiIconNames entries are excluded, and that exclusion is
	 * NOT free here the way it is for the page icons: the pages that ask
	 * for "notes" and "accounts" test the result and ask for something else
	 * when it comes back null, whereas applyThemedIcons() assigns whatever
	 * it got. So a MainWindow action naming an optional icon really is
	 * blank in a theme that lacks it -- see
	 * mainWindowOptionalIconsThatGoBlank(), which reports exactly where.
	 */
	void mainWindowUiIconsResolveInEveryTheme()
	{
		QString error;
		const QList<UiIconRequest> requests =
			readUiIconRequests(QStringLiteral(KITE_MAINWINDOW_UI_PATH),
							   &error);
		QVERIFY2(error.isEmpty(), qPrintable(error));
		QVERIFY(!requests.isEmpty());

		QStringList notDrawable;
		QStringList substituted;

		for (const QString& theme : kBundledThemes) {
			QIcon::setThemeName(theme);

			for (const UiIconRequest& request : requests) {
				if (kOptionalUiIconNames.contains(request.iconName)) {
					continue;
				}

				const Resolution got = resolve(request.iconName);
				const QString where = theme + QStringLiteral("/") +
									  request.objectName +
									  QStringLiteral("/") + request.iconName;
				if (!got.drawable) {
					notDrawable << where;
				} else if (got.substituted(request.iconName)) {
					substituted << (where + QStringLiteral(" -> ") +
									got.resolvedName);
				}
			}
		}

		QVERIFY2(notDrawable.isEmpty(),
				 qPrintable(QStringLiteral("%1 MainWindow.ui icon(s) have no "
										   "image behind them: %2")
								.arg(notDrawable.size())
								.arg(notDrawable.join(QStringLiteral(", ")))));
		QVERIFY2(substituted.isEmpty(),
				 qPrintable(QStringLiteral("%1 MainWindow.ui icon(s) silently "
										   "resolved to a DIFFERENT icon: %2")
								.arg(substituted.size())
								.arg(substituted.join(QStringLiteral(", ")))));
	}

	/*!
	 * Reports, without failing, which MainWindow actions are left blank by
	 * which theme.
	 *
	 * kOptionalUiIconNames exists because three *pages* fall back when an
	 * icon is missing. MainWindow does not: applyThemedIcons() assigns the
	 * null QIcon and logs. So an action in the .ui naming one of those
	 * icons is genuinely blank wherever the theme lacks it, and the comment
	 * on kOptionalUiIconNames -- "nothing is ever drawn blank" -- is not
	 * true of MainWindow.
	 *
	 * Deliberately not a QVERIFY. Whether those actions should get icons
	 * added to the theme, or a fallback like the pages have, is a decision
	 * about the product and not one this test gets to make; failing here
	 * would only mean the finding gets silenced. What it does do is keep
	 * the situation measured and in the run output, so it cannot quietly
	 * get worse.
	 *
	 * The one case here that narrows the search path to ":/icons" alone.
	 * initTestCase() only prepends it, matching the launcher, so the
	 * bundled themes' "Inherits=default" can still reach whatever icon
	 * themes the build machine happens to have installed -- and then this
	 * case would report a different answer on a KDE box than on a bare CI
	 * runner or a fresh Windows install, which is the situation a user
	 * actually gets. Asking only the themes Kite ships is the question
	 * with one answer.
	 */
	void mainWindowOptionalIconsThatGoBlank()
	{
		QString error;
		const QList<UiIconRequest> requests =
			readUiIconRequests(QStringLiteral(KITE_MAINWINDOW_UI_PATH),
							   &error);
		QVERIFY2(error.isEmpty(), qPrintable(error));

		const QStringList hostSearchPaths = QIcon::themeSearchPaths();
		QIcon::setThemeSearchPaths({QStringLiteral(":/icons")});

		QStringList blank;
		for (const QString& theme : kBundledThemes) {
			QIcon::setThemeName(theme);
			for (const UiIconRequest& request : requests) {
				if (!kOptionalUiIconNames.contains(request.iconName)) {
					continue;
				}
				if (!resolve(request.iconName).drawable) {
					blank << (theme + QStringLiteral("/") +
							  request.objectName + QStringLiteral(" (") +
							  request.iconName + QStringLiteral(")"));
				}
			}
		}

		// Before any QVERIFY below, so a failure cannot leave the rest of
		// the run measuring a search path this case set up.
		QIcon::setThemeSearchPaths(hostSearchPaths);

		if (blank.isEmpty()) {
			qInfo("every MainWindow.ui icon resolves in every theme, "
				  "optional ones included");
		} else {
			qWarning().noquote()
				<< QStringLiteral("%1 MainWindow action(s) have no icon in "
								  "some theme, and no fallback: %2")
					   .arg(blank.size())
					   .arg(blank.join(QStringLiteral(", ")));
		}
	}

	// -----------------------------------------------------------------
	// Inherits
	// -----------------------------------------------------------------

	/// An icon a theme does not carry itself still resolves through the
	/// theme it inherits from.
	///
	/// The name is picked by looking at the resources rather than being
	/// hardcoded, so this keeps testing inheritance even after the themes
	/// are re-cut.
	void inheritsIsFollowedAcrossThemes()
	{
		/* A theme other than multimc, and a name that theme has no file of
		 * its own for anywhere in its directory. */
		QString borrowingTheme;
		QString borrowedName;

		for (const QString& theme : kBundledThemes) {
			if (theme == QStringLiteral("multimc")) {
				continue;
			}
			for (const QString& name : kUiIconNames) {
				bool ownedHere = false;
				QDirIterator it(":/icons/" + theme,
								QDirIterator::Subdirectories);
				while (it.hasNext()) {
					it.next();
					if (it.fileInfo().baseName() == name) {
						ownedHere = true;
						break;
					}
				}
				if (!ownedHere) {
					borrowingTheme = theme;
					borrowedName = name;
					break;
				}
			}
			if (!borrowingTheme.isEmpty()) {
				break;
			}
		}

		QVERIFY2(!borrowingTheme.isEmpty(),
				 "every theme carries every icon itself, so inheritance "
				 "cannot be measured -- if that is really true this case can "
				 "go, but check the resources first");

		QIcon::setThemeName(borrowingTheme);
		QVERIFY2(resolve(borrowedName).drawable,
				 qPrintable(borrowingTheme + " does not inherit '" +
							borrowedName + "' from the theme it declares in "
										   "Inherits"));
	}

	// -----------------------------------------------------------------
	// Cache
	// -----------------------------------------------------------------

	/// Switching the icon theme really does change what fromTheme() hands
	/// out, rather than returning whatever was first cached under that name.
	///
	/// Checked as order-independence: the icon a theme produces must not
	/// depend on which themes were selected before it. A name-keyed cache
	/// that survived setThemeName() would make the first theme visited win,
	/// and the forward and reverse passes would disagree. Themes that
	/// genuinely ship the same image do not trip this.
	void themeSwitchIsNotCachedByName()
	{
		const QStringList probes = {QStringLiteral("launch"),
									QStringLiteral("settings"),
									QStringLiteral("new"),
									QStringLiteral("viewfolder")};

		QMap<QString, QImage> forward;
		for (const QString& theme : kBundledThemes) {
			QIcon::setThemeName(theme);
			for (const QString& name : probes) {
				forward.insert(theme + "/" + name, renderedIcon(name));
			}
		}

		QStringList disagreed;
		QStringList reversed = kBundledThemes;
		std::reverse(reversed.begin(), reversed.end());
		for (const QString& theme : reversed) {
			QIcon::setThemeName(theme);
			for (const QString& name : probes) {
				const QString key = theme + "/" + name;
				if (renderedIcon(name) != forward.value(key)) {
					disagreed << key;
				}
			}
		}

		QVERIFY2(disagreed.isEmpty(),
				 qPrintable(QStringLiteral("icon depended on which theme was "
										   "selected before it, so a stale "
										   "cache is being served for: %1")
								.arg(disagreed.join(QStringLiteral(", ")))));
	}
};

/*
 * Not QTEST_MAIN/QTEST_GUILESS_MAIN.
 *
 * QIcon theme lookup needs a QGuiApplication -- the platform theme is reached
 * through it, and keeping that from taking over icon lookup is the whole
 * point, so under a QCoreApplication there would be nothing to measure.
 * Widgets are not needed, so QGuiApplication it is.
 *
 * The platform is not forced here. ctest pins it to offscreen for a
 * deterministic run (see launcher/CMakeLists.txt); running the binary by hand
 * inside a desktop session is the other half of the measurement, and that
 * only works if the session's own platform is left alone. Offscreen is only
 * the default when there is no windowing system to talk to at all.
 */
int main(int argc, char* argv[])
{
	if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM") &&
		!windowingSystemAvailable()) {
		qputenv("QT_QPA_PLATFORM", "offscreen");
	}

	QGuiApplication app(argc, argv);

	/* The compiled themes sit in Kite_logic, which is a static library, so
	 * the linker drops the object files that register them unless something
	 * refers to them by symbol. main.cpp does this for the launcher itself
	 * for the same reason; every icon theme listed there is repeated here,
	 * because leaving one out would show up as that theme having no icons at
	 * all rather than as a link error. */
	Q_INIT_RESOURCE(multimc);
	Q_INIT_RESOURCE(pe_dark);
	Q_INIT_RESOURCE(pe_light);
	Q_INIT_RESOURCE(pe_blue);
	Q_INIT_RESOURCE(pe_colored);
	Q_INIT_RESOURCE(breeze_dark);
	Q_INIT_RESOURCE(breeze_light);
	Q_INIT_RESOURCE(OSX);
	Q_INIT_RESOURCE(iOS);
	Q_INIT_RESOURCE(flat);
	Q_INIT_RESOURCE(flat_white);

	IconThemeTest testCase;
	return QTest::qExec(&testCase, argc, argv);
}

#include "IconTheme_test.moc"
