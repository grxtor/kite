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

#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include "GitHubRelease.h"

namespace AssetMatcher
{
	struct Installation {
		/*! CI build name (BuildConfig.BUILD_ARTIFACT), e.g. "Windows-MSVC-Qt6", "Linux-aarch64-Qt6".
		 * Empty means a local build, which has no published counterpart and matches nothing. */
		QString buildArtifact;

		//! portable.txt sits in the installation root.
		bool portable = false;

		//! Running from a mounted AppImage.
		bool appImage = false;

		//! 64-bit ARM, however the platform spells it.
		bool arm64 = false;
	};

	/*! Outcome for one asset, with the reason spelled out for the log. */
	struct Decision {
		bool accepted = false;
		QString reason;
	};

	/*! Judges a single asset; the reason is always filled in, accepted or not, for the update log. */
	Decision consider(const GitHubReleaseAsset& asset,
					  const Installation& installation);

	/*! Every asset that belongs on \a installation. More than one is not an error (the caller asks
	 * the user). \a log, when given, receives one line per asset. */
	QList<GitHubReleaseAsset> select(const QList<GitHubReleaseAsset>& assets,
									 const Installation& installation,
									 QStringList* log = nullptr);

	/*! The Qt major version a name declares through a "-qt<N>" token, or 0. Exposed for the test. */
	int qtMajorFromName(const QString& lowerName);

} // namespace AssetMatcher
