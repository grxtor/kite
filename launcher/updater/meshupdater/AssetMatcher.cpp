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

#include "AssetMatcher.h"

#include <QRegularExpression>

namespace
{
	enum class Arch {
		Unknown,
		X86_64,
		Arm64,
	};

	QStringList wordsOf(const QString& lowerName)
	{
		static const QRegularExpression separators(QStringLiteral("[-. ]+"));
		return lowerName.split(separators, Qt::SkipEmptyParts);
	}

	Arch archOf(const QString& lowerName)
	{
		const QStringList words = wordsOf(lowerName);

		for (const QString& word : words) {
			if (word == QLatin1String("arm64") ||
				word == QLatin1String("aarch64") ||
				word == QLatin1String("arm")) {
				return Arch::Arm64;
			}
			if (word == QLatin1String("x86_64") ||
				word == QLatin1String("x64") ||
				word == QLatin1String("amd64") ||
				word == QLatin1String("intel")) {
				return Arch::X86_64;
			}
		}

		return Arch::Unknown;
	}

	bool endsWithAny(const QString& lowerName, const QStringList& suffixes)
	{
		for (const QString& suffix : suffixes) {
			if (lowerName.endsWith(suffix))
				return true;
		}
		return false;
	}

	//! A container the updater can unpack over an installation.
	bool isArchive(const QString& lowerName)
	{
		return endsWithAny(lowerName, {QStringLiteral(".zip"),
									   QStringLiteral(".tar.gz"),
									   QStringLiteral(".tgz")});
	}

	//! A program the updater can hand the installation over to.
	bool isInstaller(const QString& lowerName)
	{
		return lowerName.endsWith(QLatin1String(".exe"));
	}

	//! Sidecars: checksums, signatures, zsync control files.
	bool isSidecar(const QString& lowerName)
	{
		return endsWithAny(lowerName, {QStringLiteral(".zsync"),
									   QStringLiteral(".sha256"),
									   QStringLiteral(".sha512"),
									   QStringLiteral(".sig"),
									   QStringLiteral(".asc")});
	}

	struct ArtifactIdentity {
		QStringList requiredWords;
		int qtMajor = 0;
		bool legacy = false;
		bool windows = false;
	};

	ArtifactIdentity identityOf(const QString& buildArtifact)
	{
		ArtifactIdentity identity;

		static const QRegularExpression qtWord(QStringLiteral("^qt(\\d+)$"));

		const QStringList words =
			buildArtifact.toLower().split(QLatin1Char('-'), Qt::SkipEmptyParts);
		for (const QString& word : words) {
			if (const QRegularExpressionMatch match = qtWord.match(word);
				match.hasMatch()) {
				identity.qtMajor = match.captured(1).toInt();
				continue;
			}
			if (word == QLatin1String("legacy"))
				identity.legacy = true;
			if (word == QLatin1String("windows"))
				identity.windows = true;

			identity.requiredWords.append(word);
		}

		return identity;
	}

} // namespace

int AssetMatcher::qtMajorFromName(const QString& lowerName)
{
	// The token is written "-qt6", never "qt6" on its own, so the leading
	// separator is part of the pattern: without it, a hypothetical
	// "Kite-LGBTQ6..." would report a Qt version.
	static const QRegularExpression qtToken(QStringLiteral("-qt(\\d+)"));

	const QRegularExpressionMatch match = qtToken.match(lowerName);
	return match.hasMatch() ? match.captured(1).toInt() : 0;
}

AssetMatcher::Decision
AssetMatcher::consider(const GitHubReleaseAsset& asset,
					   const Installation& installation)
{
	Decision decision;

	if (installation.buildArtifact.trimmed().isEmpty()) {
		// A local build. It has no published counterpart, and guessing one
		// would mean overwriting a developer's tree with a release.
		decision.reason =
			QStringLiteral("this build has no artifact name, so no release "
						   "file can belong to it");
		return decision;
	}

	const QString name = asset.name.toLower();
	const ArtifactIdentity identity = identityOf(installation.buildArtifact);

	if (isSidecar(name)) {
		decision.reason = QStringLiteral("a checksum or zsync sidecar, not a "
										 "package");
		return decision;
	}

	// AppImage-ness has to match exactly, in both directions. An AppImage
	// install cannot be updated by unpacking a tarball over it (there is
	// nothing to unpack over -- it is one mounted file), and a normal install
	// has no use for an AppImage.
	const bool assetIsAppImage = name.endsWith(QLatin1String(".appimage"));
	if (assetIsAppImage != installation.appImage) {
		decision.reason = assetIsAppImage
							  ? QStringLiteral("an AppImage, and this is not "
											   "an AppImage installation")
							  : QStringLiteral("not an AppImage, and this is "
											   "an AppImage installation");
		return decision;
	}

	if (name.endsWith(QLatin1String(".dmg"))) {
		// A disk image is for a human dragging an app into /Applications.
		decision.reason = QStringLiteral("a disk image, which is for manual "
										 "installation");
		return decision;
	}

	const bool assetIsArchive = isArchive(name);
	const bool assetIsInstaller = isInstaller(name);
	if (!assetIsAppImage && !assetIsArchive && !assetIsInstaller) {
		decision.reason =
			QStringLiteral("not a package the updater knows how to install");
		return decision;
	}

	for (const QString& word : identity.requiredWords) {
		if (!name.contains(word)) {
			decision.reason =
				QStringLiteral("built for a different platform (no '%1')")
					.arg(word);
			return decision;
		}
	}

	if (const int assetQtMajor = qtMajorFromName(name);
		assetQtMajor != 0 && identity.qtMajor != 0 &&
		assetQtMajor != identity.qtMajor) {
		decision.reason = QStringLiteral("built against Qt %1, this is a Qt "
										 "%2 build")
							  .arg(assetQtMajor)
							  .arg(identity.qtMajor);
		return decision;
	}

	// "Legacy" is a separate product line for older Windows and macOS. It
	// shares every other word with the ordinary build, so without this an
	// ordinary install would quietly migrate onto it.
	if (!identity.legacy && name.contains(QLatin1String("legacy"))) {
		decision.reason = QStringLiteral("a legacy build, this is not");
		return decision;
	}

	const Arch assetArch = archOf(name);
	const Arch ourArch =
		installation.arm64 ? Arch::Arm64 : Arch::X86_64;
	if (assetArch != Arch::Unknown && assetArch != ourArch) {
		decision.reason =
			QStringLiteral("built for a different CPU architecture");
		return decision;
	}

	const bool assetIsPortable = name.contains(QLatin1String("portable"));

	if (identity.windows && !installation.portable) {
		if (!assetIsInstaller) {
			decision.reason = QStringLiteral("not the installer, which is "
											 "what a non-portable Windows "
											 "installation needs");
			return decision;
		}
	} else if (assetIsPortable != installation.portable) {
		decision.reason =
			assetIsPortable
				? QStringLiteral("a portable package, this installation is "
								 "not portable")
				: QStringLiteral("not a portable package, this installation "
								 "is portable");
		return decision;
	}

	decision.accepted = true;
	decision.reason = QStringLiteral("matches this installation");
	return decision;
}

QList<GitHubReleaseAsset>
AssetMatcher::select(const QList<GitHubReleaseAsset>& assets,
					 const Installation& installation, QStringList* log)
{
	QList<GitHubReleaseAsset> selected;

	for (const GitHubReleaseAsset& asset : assets) {
		const Decision decision = consider(asset, installation);

		if (log) {
			log->append(QStringLiteral("%1 %2: %3")
							.arg(decision.accepted ? QStringLiteral("Accepting")
												   : QStringLiteral("Rejecting"),
								 asset.name, decision.reason));
		}

		if (decision.accepted)
			selected.append(asset);
	}

	return selected;
}
