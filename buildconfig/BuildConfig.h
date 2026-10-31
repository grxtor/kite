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

#pragma once
#include <QString>

// Build-time information passed from the build system.
class Config
{
  public:
	Config();
	QString KITE_NAME;
	QString KITE_BINARY;
	QString KITE_DISPLAYNAME;
	QString KITE_COPYRIGHT;
	QString KITE_DOMAIN;
	QString KITE_CONFIGFILE;
	QString KITE_GIT;
	QString KITE_APPID;

	int VERSION_MAJOR;
	int VERSION_MINOR;
	int VERSION_HOTFIX;

	/// The git branch name; informational.
	QString VERSION_CHANNEL;

	/// "stable" or "beta"; decides whether the updater offers pre-releases by
	/// default.
	QString UPDATE_CHANNEL;

	bool UPDATER_ENABLED = false;

	/// Short platform string, e.g. "lin64" or "win32".
	QString BUILD_PLATFORM;

	QString UPDATER_GITHUB_REPO;

	QString BUILD_DATE;

	QString USER_AGENT;

	QString USER_AGENT_UNCACHED;

	QString BUILD_ARTIFACT;

	QString COMPILER_NAME;

	QString COMPILER_VERSION;

	QString COMPILER_TARGET_SYSTEM;

	QString COMPILER_TARGET_SYSTEM_VERSION;

	QString COMPILER_TARGET_SYSTEM_PROCESSOR;

	QString NOTIFICATION_URL;

	/// Used for matching notifications.
	QString FULL_VERSION_STR;

	QString GIT_COMMIT;

	QString GIT_REFSPEC;

	/// The exact git tag of this build, if any.
	QString GIT_TAG;

	QString VERSION_STR;

	QString NEWS_RSS_URL;
	/// Semicolon-separated; they become feeds 1..N of NewsChecker.
	QString NEWS_EXTRA_FEEDS;

	QString MSAClientID;

	QString PASTE_EE_KEY;

	QString IMGUR_CLIENT_ID;

	QString META_URL;

	QString FRIENDS_URL;

	QString CURSEFORGE_API_KEY;

	QString BUG_TRACKER_URL;
	QString DISCORD_URL;
	QString SUBREDDIT_URL;
	QString PATREON_URL;

	QString RESOURCE_BASE = "https://resources.download.minecraft.net/";
	QString LIBRARY_BASE = "https://libraries.minecraft.net/";
	QString IMGUR_BASE_URL = "https://api.imgur.com/3/";
	// Byte-identical to the files the sha1s in VersionFilterData.cpp pin.
	QString FMLLIBS_BASE_URL = "https://meta.kiteclient.com/fmllibs/";

	/// Where translation updates are downloaded from; empty means never.
	QString TRANSLATIONS_BASE_URL;

	QString MODPACKSCH_API_BASE_URL = "https://api.modpacks.ch/";

	QString TECHNIC_API_BASE_URL = "https://api.technicpack.net/";
	// Technic picks the download URL from this; "kite" got a third-party mirror
	// whose archives failed the CRC check, so send the value other launchers do.
	QString TECHNIC_API_BUILD = "multimc";

	QString ATL_DOWNLOAD_SERVER_URL =
		"https://download.nodecdn.net/containers/atl/";

	/// "major.minor.hotfix".
	QString printableVersionString() const;

	/// "Name - Version", or just "Name" if the version is empty.
	QString compilerID() const;

	/// "OS Version Processor".
	QString systemID() const;
};

extern const Config BuildConfig;
