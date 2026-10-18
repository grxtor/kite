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

#include "updater/meshupdater/AssetMatcher.h"

/*!
 * Which release file belongs on which installation.
 *
 * The fixture is the `files:` list of .github/workflows/release.yml, copied
 * entry for entry, with ${VERSION} spelled out. That workflow is the only
 * authority on what a release contains: it is what uploads the files, so a
 * name that does not appear there cannot exist, and a name that does will
 * exist in the next release whether or not it exists in the last one.
 *
 * Deliberately *not* taken from a published release. The published ones were
 * built before Qt5 support was added and use the older naming; testing
 * against them pins a contract that has already been replaced, and would
 * report the workflow as the thing that is wrong.
 *
 * The build side of the pairing comes from .github/workflows/build.yml, whose
 * matrix sets ARTIFACT_NAME to "<artifact-name>-Qt<qt-major>" -- which is
 * what reaches the updater as BuildConfig.BUILD_ARTIFACT.
 *
 * Every rule here exists for a quirk of that naming that no single-platform
 * test run can see:
 *
 *   - Qt5 and Qt6 are two complete lines in the same release
 *   - the words come in a different order on either side, e.g. the build
 *     "Windows-MinGW-w64-Qt6" is published as "Kite-Windows-MinGW-Qt6-w64-"
 *   - Windows ships three packages per line (portable, installer, plain)
 *   - macOS says "Intel"/"ARM" where Linux says "x86_64"/"aarch64"
 *   - the AppImages put the architecture after the Qt version, the Linux
 *     tarballs put it before
 */
class AssetMatcherTest : public QObject
{
	Q_OBJECT

  private:
	static QString version()
	{
		return QStringLiteral("v11.0.0");
	}

	/*!
	 * The `files:` list of release.yml, in its own order.
	 *
	 * Kept in that order, rather than sorted, so that a diff against the
	 * workflow is a straight read down both columns.
	 */
	static QStringList releaseAssetNames()
	{
		const QString v = version();
		return {
			QStringLiteral("Kite-Linux-Qt5-x86_64.AppImage"),
			QStringLiteral("Kite-Linux-Qt5-x86_64.AppImage.zsync"),
			QStringLiteral("Kite-Linux-Qt6-x86_64.AppImage"),
			QStringLiteral("Kite-Linux-Qt6-x86_64.AppImage.zsync"),
			QStringLiteral("Kite-Linux-Qt6-aarch64.AppImage"),
			QStringLiteral("Kite-Linux-Qt6-aarch64.AppImage.zsync"),
			QStringLiteral("Kite-Linux-Qt5-Portable-%1.tar.gz").arg(v),
			QStringLiteral("Kite-Linux-Qt6-Portable-%1.tar.gz").arg(v),
			QStringLiteral("Kite-Linux-aarch64-Qt6-Portable-%1.tar.gz").arg(v),
			QStringLiteral("Kite-Windows-MinGW-Qt5-w64-%1.zip").arg(v),
			QStringLiteral("Kite-Windows-MinGW-Qt5-w64-Portable-%1.zip").arg(v),
			QStringLiteral("Kite-Windows-MinGW-Qt5-w64-Setup-%1.exe").arg(v),
			QStringLiteral("Kite-Windows-MinGW-Qt6-w64-%1.zip").arg(v),
			QStringLiteral("Kite-Windows-MinGW-Qt6-w64-Portable-%1.zip").arg(v),
			QStringLiteral("Kite-Windows-MinGW-Qt6-w64-Setup-%1.exe").arg(v),
			QStringLiteral("Kite-Windows-MSVC-Qt6-arm64-%1.zip").arg(v),
			QStringLiteral("Kite-Windows-MSVC-Qt6-arm64-Portable-%1.zip").arg(v),
			QStringLiteral("Kite-Windows-MSVC-Qt6-arm64-Setup-%1.exe").arg(v),
			QStringLiteral("Kite-Windows-MSVC-Qt5-%1.zip").arg(v),
			QStringLiteral("Kite-Windows-MSVC-Qt5-Portable-%1.zip").arg(v),
			QStringLiteral("Kite-Windows-MSVC-Qt5-Setup-%1.exe").arg(v),
			QStringLiteral("Kite-Windows-MSVC-Qt6-%1.zip").arg(v),
			QStringLiteral("Kite-Windows-MSVC-Qt6-Portable-%1.zip").arg(v),
			QStringLiteral("Kite-Windows-MSVC-Qt6-Setup-%1.exe").arg(v),
			QStringLiteral("Kite-macOS-Intel-Qt5-%1.zip").arg(v),
			QStringLiteral("Kite-macOS-Intel-Qt5-%1.dmg").arg(v),
			QStringLiteral("Kite-macOS-Intel-Qt6-%1.zip").arg(v),
			QStringLiteral("Kite-macOS-Intel-Qt6-%1.dmg").arg(v),
			QStringLiteral("Kite-macOS-ARM-Qt6-%1.zip").arg(v),
			QStringLiteral("Kite-macOS-ARM-Qt6-%1.dmg").arg(v),
			QStringLiteral("Kite-%1.tar.gz").arg(v),
		};
	}

	static QList<GitHubReleaseAsset> assetsFrom(const QStringList& names)
	{
		QList<GitHubReleaseAsset> assets;
		qint64 id = 1;
		for (const QString& name : names) {
			GitHubReleaseAsset asset;
			asset.id = id++;
			asset.name = name;
			asset.size = 1024;
			asset.downloadUrl =
				QUrl(QStringLiteral("https://example.invalid/%1").arg(name));
			assets.append(asset);
		}
		return assets;
	}

	static QList<GitHubReleaseAsset> releaseAssets()
	{
		return assetsFrom(releaseAssetNames());
	}

	static AssetMatcher::Installation install(const QString& artifact,
											  bool portable, bool appImage,
											  bool arm64)
	{
		AssetMatcher::Installation installation;
		installation.buildArtifact = artifact;
		installation.portable = portable;
		installation.appImage = appImage;
		installation.arm64 = arm64;
		return installation;
	}

	//! Names of what select() picked, so a failure reads as names not indexes.
	static QStringList pickedNames(const AssetMatcher::Installation& what)
	{
		QStringList names;
		for (const GitHubReleaseAsset& asset :
			 AssetMatcher::select(releaseAssets(), what)) {
			names.append(asset.name);
		}
		return names;
	}

	/*!
	 * Every build in build.yml's matrix, paired with how it can be installed.
	 *
	 * Used by the invariants below, so that adding a platform to the matrix
	 * and forgetting it here is the only way to escape them.
	 */
	static QList<AssetMatcher::Installation> everyBuild()
	{
		return {
			// artifact,                       portable, appImage, arm64
			install(QStringLiteral("Linux-Qt5"), true, false, false),
			install(QStringLiteral("Linux-Qt5"), false, true, false),
			install(QStringLiteral("Linux-Qt6"), true, false, false),
			install(QStringLiteral("Linux-Qt6"), false, true, false),
			install(QStringLiteral("Linux-aarch64-Qt6"), true, false, true),
			install(QStringLiteral("Linux-aarch64-Qt6"), false, true, true),
			install(QStringLiteral("Windows-MSVC-Qt5"), true, false, false),
			install(QStringLiteral("Windows-MSVC-Qt5"), false, false, false),
			install(QStringLiteral("Windows-MSVC-Qt6"), true, false, false),
			install(QStringLiteral("Windows-MSVC-Qt6"), false, false, false),
			install(QStringLiteral("Windows-MSVC-arm64-Qt6"), true, false, true),
			install(QStringLiteral("Windows-MSVC-arm64-Qt6"), false, false, true),
			install(QStringLiteral("Windows-MinGW-w64-Qt5"), true, false, false),
			install(QStringLiteral("Windows-MinGW-w64-Qt5"), false, false, false),
			install(QStringLiteral("Windows-MinGW-w64-Qt6"), true, false, false),
			install(QStringLiteral("Windows-MinGW-w64-Qt6"), false, false, false),
			install(QStringLiteral("macOS-Intel-Qt5"), false, false, false),
			install(QStringLiteral("macOS-Intel-Qt6"), false, false, false),
			install(QStringLiteral("macOS-ARM-Qt6"), false, false, true),
		};
	}

  private slots:

	// -- The release, from every build's point of view ---------------------

	/*!
	 * Each build finds exactly one file, and the right one.
	 *
	 * One table, one row per build in the matrix; a failing row names the
	 * platform it broke on. Expectations are read off release.yml by hand,
	 * which is the point -- if the workflow and this table disagree, one of
	 * the two is wrong and a human has to say which.
	 */
	void tst_Release_data()
	{
		QTest::addColumn<QString>("artifact");
		QTest::addColumn<bool>("portable");
		QTest::addColumn<bool>("appImage");
		QTest::addColumn<bool>("arm64");
		QTest::addColumn<QString>("expected");

		const QString v = version();

		// -- Linux tarballs: architecture before the Qt version -----------
		QTest::newRow("Linux x86_64 Qt5 portable")
			<< "Linux-Qt5" << true << false << false
			<< QStringLiteral("Kite-Linux-Qt5-Portable-%1.tar.gz").arg(v);
		QTest::newRow("Linux x86_64 Qt6 portable")
			<< "Linux-Qt6" << true << false << false
			<< QStringLiteral("Kite-Linux-Qt6-Portable-%1.tar.gz").arg(v);
		QTest::newRow("Linux aarch64 Qt6 portable")
			<< "Linux-aarch64-Qt6" << true << false << true
			<< QStringLiteral("Kite-Linux-aarch64-Qt6-Portable-%1.tar.gz")
				   .arg(v);

		// -- AppImages: architecture after the Qt version -----------------
		QTest::newRow("Linux x86_64 Qt5 AppImage")
			<< "Linux-Qt5" << false << true << false
			<< "Kite-Linux-Qt5-x86_64.AppImage";
		QTest::newRow("Linux x86_64 Qt6 AppImage")
			<< "Linux-Qt6" << false << true << false
			<< "Kite-Linux-Qt6-x86_64.AppImage";
		QTest::newRow("Linux aarch64 Qt6 AppImage")
			<< "Linux-aarch64-Qt6" << false << true << true
			<< "Kite-Linux-Qt6-aarch64.AppImage";

		// -- Windows MSVC -------------------------------------------------
		QTest::newRow("Windows MSVC Qt5 portable")
			<< "Windows-MSVC-Qt5" << true << false << false
			<< QStringLiteral("Kite-Windows-MSVC-Qt5-Portable-%1.zip").arg(v);
		QTest::newRow("Windows MSVC Qt6 portable")
			<< "Windows-MSVC-Qt6" << true << false << false
			<< QStringLiteral("Kite-Windows-MSVC-Qt6-Portable-%1.zip").arg(v);

		// A non-portable Windows installation gets the installer, never the
		// plain archive: the installer owns the uninstall entry and the
		// registry keys that unpacking a zip over the top would leave
		// pointing at the old version.
		QTest::newRow("Windows MSVC Qt5 installed")
			<< "Windows-MSVC-Qt5" << false << false << false
			<< QStringLiteral("Kite-Windows-MSVC-Qt5-Setup-%1.exe").arg(v);
		QTest::newRow("Windows MSVC Qt6 installed")
			<< "Windows-MSVC-Qt6" << false << false << false
			<< QStringLiteral("Kite-Windows-MSVC-Qt6-Setup-%1.exe").arg(v);

		QTest::newRow("Windows MSVC arm64 Qt6 portable")
			<< "Windows-MSVC-arm64-Qt6" << true << false << true
			<< QStringLiteral("Kite-Windows-MSVC-Qt6-arm64-Portable-%1.zip")
				   .arg(v);
		QTest::newRow("Windows MSVC arm64 Qt6 installed")
			<< "Windows-MSVC-arm64-Qt6" << false << false << true
			<< QStringLiteral("Kite-Windows-MSVC-Qt6-arm64-Setup-%1.exe")
				   .arg(v);

		// -- Windows MinGW: the build says "w64-Qt6", the file says
		//    "Qt6-w64". Matching the artifact name as one substring finds
		//    nothing here, which is why the words are matched as a set.
		QTest::newRow("Windows MinGW Qt5 portable")
			<< "Windows-MinGW-w64-Qt5" << true << false << false
			<< QStringLiteral("Kite-Windows-MinGW-Qt5-w64-Portable-%1.zip")
				   .arg(v);
		QTest::newRow("Windows MinGW Qt6 portable")
			<< "Windows-MinGW-w64-Qt6" << true << false << false
			<< QStringLiteral("Kite-Windows-MinGW-Qt6-w64-Portable-%1.zip")
				   .arg(v);
		QTest::newRow("Windows MinGW Qt5 installed")
			<< "Windows-MinGW-w64-Qt5" << false << false << false
			<< QStringLiteral("Kite-Windows-MinGW-Qt5-w64-Setup-%1.exe")
				   .arg(v);
		QTest::newRow("Windows MinGW Qt6 installed")
			<< "Windows-MinGW-w64-Qt6" << false << false << false
			<< QStringLiteral("Kite-Windows-MinGW-Qt6-w64-Setup-%1.exe")
				   .arg(v);

		// -- macOS: the .zip is the update, the .dmg is for a human -------
		QTest::newRow("macOS Intel Qt5")
			<< "macOS-Intel-Qt5" << false << false << false
			<< QStringLiteral("Kite-macOS-Intel-Qt5-%1.zip").arg(v);
		QTest::newRow("macOS Intel Qt6")
			<< "macOS-Intel-Qt6" << false << false << false
			<< QStringLiteral("Kite-macOS-Intel-Qt6-%1.zip").arg(v);
		QTest::newRow("macOS ARM Qt6")
			<< "macOS-ARM-Qt6" << false << false << true
			<< QStringLiteral("Kite-macOS-ARM-Qt6-%1.zip").arg(v);
	}

	void tst_Release()
	{
		QFETCH(QString, artifact);
		QFETCH(bool, portable);
		QFETCH(bool, appImage);
		QFETCH(bool, arm64);
		QFETCH(QString, expected);

		const QStringList picked =
			pickedNames(install(artifact, portable, appImage, arm64));

		// One match, not "at least one": a second would put a file picker in
		// front of a user who never asked for a choice.
		QCOMPARE(picked, QStringList{expected});
	}

	// -- Invariants across the whole matrix --------------------------------

	//! No build is ever left choosing between several files.
	void tst_NoBuildGetsAnAmbiguousChoice()
	{
		for (const AssetMatcher::Installation& what : everyBuild()) {
			const QStringList picked = pickedNames(what);
			QVERIFY2(picked.size() == 1,
					 qPrintable(QStringLiteral("%1 (portable %2, appimage %3, "
											   "arm64 %4) picked %5: %6")
									.arg(what.buildArtifact)
									.arg(what.portable)
									.arg(what.appImage)
									.arg(what.arm64)
									.arg(picked.size())
									.arg(picked.join(", "))));
		}
	}

	/*!
	 * The Qt5 and Qt6 lines never cross.
	 *
	 * Both are published in full in every release, so this is not
	 * hypothetical: a Qt5 build that took the Qt6 archive would replace its
	 * own Qt libraries with ones its binaries cannot load.
	 */
	void tst_QtLinesNeverCross()
	{
		for (const AssetMatcher::Installation& what : everyBuild()) {
			const int ourQt =
				AssetMatcher::qtMajorFromName(what.buildArtifact.toLower());
			QVERIFY2(ourQt != 0,
					 qPrintable(QStringLiteral("%1 states no Qt version")
									.arg(what.buildArtifact)));

			for (const QString& name : pickedNames(what)) {
				const int theirQt =
					AssetMatcher::qtMajorFromName(name.toLower());
				QVERIFY2(theirQt == ourQt,
						 qPrintable(QStringLiteral("%1 was offered %2")
										.arg(what.buildArtifact, name)));
			}
		}
	}

	/*!
	 * An x86_64 build is never offered an ARM package, or the other way
	 * round -- across all three spellings in use: "aarch64" on Linux,
	 * "arm64" on Windows, and a bare "ARM" on macOS.
	 */
	void tst_ArchitectureIsNeverCrossed()
	{
		for (const AssetMatcher::Installation& what : everyBuild()) {
			for (const QString& name : pickedNames(what)) {
				const QString lower = name.toLower();
				const bool assetIsArm =
					lower.contains(QStringLiteral("aarch64")) ||
					lower.contains(QStringLiteral("arm64")) ||
					lower.contains(QStringLiteral("-arm-"));
				const bool assetIsIntel =
					lower.contains(QStringLiteral("x86_64")) ||
					lower.contains(QStringLiteral("intel"));

				if (what.arm64) {
					QVERIFY2(!assetIsIntel,
							 qPrintable(QStringLiteral("%1 was offered %2")
											.arg(what.buildArtifact, name)));
				} else {
					QVERIFY2(!assetIsArm,
							 qPrintable(QStringLiteral("%1 was offered %2")
											.arg(what.buildArtifact, name)));
				}
			}
		}
	}

	//! Portable installs get portable packages, and only those.
	void tst_PortabilityIsNeverCrossed()
	{
		for (const AssetMatcher::Installation& what : everyBuild()) {
			if (what.appImage)
				continue; // An AppImage is neither, and is covered below.

			for (const QString& name : pickedNames(what)) {
				const bool assetIsPortable =
					name.contains(QStringLiteral("Portable"));
				if (what.portable) {
					QVERIFY2(assetIsPortable, qPrintable(name));
				} else {
					QVERIFY2(!assetIsPortable, qPrintable(name));
				}
			}
		}
	}

	/*!
	 * An AppImage cannot be updated by unpacking a tarball over it -- it is
	 * one mounted file -- and a normal installation has no use for one.
	 */
	void tst_AppImageNessIsNeverCrossed()
	{
		for (const AssetMatcher::Installation& what : everyBuild()) {
			for (const QString& name : pickedNames(what)) {
				QCOMPARE(name.endsWith(QStringLiteral(".AppImage")),
						 what.appImage);
			}
		}
	}

	// -- Things that must never be picked, by anyone ------------------------

	void tst_NobodyIsOfferedTheSourceTarball()
	{
		for (const AssetMatcher::Installation& what : everyBuild()) {
			QVERIFY2(!pickedNames(what).contains(
						 QStringLiteral("Kite-%1.tar.gz").arg(version())),
					 qPrintable(what.buildArtifact));
		}
	}

	void tst_NobodyIsOfferedASidecarOrDiskImage()
	{
		for (const AssetMatcher::Installation& what : everyBuild()) {
			for (const QString& name : pickedNames(what)) {
				QVERIFY2(!name.endsWith(QStringLiteral(".zsync")),
						 qPrintable(name));
				QVERIFY2(!name.endsWith(QStringLiteral(".dmg")),
						 qPrintable(name));
			}
		}
	}

	/*!
	 * A build that cannot say what it is matches nothing.
	 *
	 * BUILD_ARTIFACT is empty for a local build, and the worst outcome there
	 * is overwriting a developer's tree with a release.
	 */
	void tst_LocalBuildMatchesNothing()
	{
		QVERIFY(pickedNames(install(QString(), true, false, false)).isEmpty());
		QVERIFY(pickedNames(install(QStringLiteral("   "), false, false, false))
					.isEmpty());
	}

	// -- The Qt version rule, in isolation ---------------------------------

	void tst_QtMajorFromName_data()
	{
		QTest::addColumn<QString>("lowerName");
		QTest::addColumn<int>("expected");

		QTest::newRow("linux tarball") << "kite-linux-qt6-portable-v11.0.0.tar.gz" << 6;
		QTest::newRow("linux qt5 tarball") << "kite-linux-qt5-portable-v11.0.0.tar.gz" << 5;
		QTest::newRow("windows package") << "kite-windows-msvc-qt6-portable-v11.0.0.zip" << 6;
		QTest::newRow("appimage") << "kite-linux-qt6-x86_64.appimage" << 6;
		QTest::newRow("macos") << "kite-macos-arm-qt6-v11.0.0.zip" << 6;
		QTest::newRow("artifact name") << "windows-mingw-w64-qt6" << 6;
		QTest::newRow("source tarball states nothing")
			<< "kite-v11.0.0.tar.gz" << 0;
		// The separator is part of the pattern, so a word merely containing
		// "qt" followed by digits is not a Qt version.
		QTest::newRow("no separator, no version") << "kiteqt6.zip" << 0;
	}

	void tst_QtMajorFromName()
	{
		QFETCH(QString, lowerName);
		QFETCH(int, expected);
		QCOMPARE(AssetMatcher::qtMajorFromName(lowerName), expected);
	}

	/*!
	 * An asset that states no Qt version is still accepted.
	 *
	 * Nothing release.yml publishes today looks like this. It is here for the
	 * day Qt5 support is dropped and the token goes away again: the rule has
	 * to survive that without matching nothing at all. See the comment in
	 * AssetMatcher::consider().
	 */
	void tst_UnstatedQtIsAccepted()
	{
		const QList<GitHubReleaseAsset> untagged = assetsFrom(
			{QStringLiteral("Kite-Windows-MSVC-Portable-v11.0.0.zip")});

		QCOMPARE(AssetMatcher::select(
					 untagged, install(QStringLiteral("Windows-MSVC-Qt6"), true,
									   false, false))
					 .size(),
				 1);
	}

	// -- Other separate product lines --------------------------------------

	/*!
	 * The legacy Windows line shares every other word with the ordinary one.
	 *
	 * release.yml builds its name by appending "-Legacy" to the same stem, so
	 * without this rule an ordinary install would quietly drift onto it.
	 */
	void tst_LegacyLineIsNotOfferedToAnOrdinaryBuild()
	{
		const QList<GitHubReleaseAsset> legacy = assetsFrom(
			{QStringLiteral("Kite-Windows-MSVC-Qt5-Legacy-Portable-v11.0.0.zip")});

		QVERIFY(AssetMatcher::select(
					legacy, install(QStringLiteral("Windows-MSVC-Qt5"), true,
									false, false))
					.isEmpty());

		// A legacy build is entitled to it.
		QCOMPARE(AssetMatcher::select(
					 legacy,
					 install(QStringLiteral("Windows-MSVC-Legacy-Qt5"), true,
							 false, false))
					 .size(),
				 1);
	}

	// -- The log contract --------------------------------------------------

	/*!
	 * Every asset is accounted for in the log, accepted or not.
	 *
	 * "Why did it pick that file" is the first question when an update goes
	 * wrong, and on a user's machine the log is the only thing that can
	 * answer it.
	 */
	void tst_EveryAssetIsExplainedInTheLog()
	{
		QStringList log;
		const QList<GitHubReleaseAsset> assets = releaseAssets();
		AssetMatcher::select(
			assets,
			install(QStringLiteral("Windows-MSVC-Qt6"), true, false, false),
			&log);

		QCOMPARE(log.size(), assets.size());
		for (const QString& line : log) {
			QVERIFY2(!line.isEmpty(), "a log line without a reason");
		}
	}

	//! consider() states a reason whether it accepts or refuses.
	void tst_DecisionAlwaysCarriesAReason()
	{
		const AssetMatcher::Installation what =
			install(QStringLiteral("Windows-MSVC-Qt6"), true, false, false);

		for (const GitHubReleaseAsset& asset : releaseAssets()) {
			const AssetMatcher::Decision decision =
				AssetMatcher::consider(asset, what);
			QVERIFY2(!decision.reason.isEmpty(), qPrintable(asset.name));
		}
	}
};

QTEST_GUILESS_MAIN(AssetMatcherTest)

#include "AssetMatcher_test.moc"
