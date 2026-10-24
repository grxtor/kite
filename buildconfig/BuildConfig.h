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

/**
 * \brief The Config class holds all the build-time information passed from the
 * build system.
 */
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

	/** Prefix for the launcher's own environment variables, e.g. "KITE"
	 * for KITE_DISABLE_GLVULKAN. Configurable rather than hardcoded so a
	 * rebranded build's variables carry its own name. */
	QString KITE_ENVNAME;

	/// The major version number.
	int VERSION_MAJOR;
	/// The minor version number.
	int VERSION_MINOR;
	/// The hotfix number.
	int VERSION_HOTFIX;
	/// The build number.
	int VERSION_BUILD;

	/**
	 * The version channel
	 * This is used by the updater to determine what channel the current version
	 * came from.
	 */
	QString VERSION_CHANNEL;

	/**
	 * The release channel this build subscribes to: "stable" or "beta".
	 *
	 * Unlike VERSION_CHANNEL (which is just the git branch name), this is set
	 * deliberately at configure time via Kite_UPDATE_CHANNEL, and it is
	 * what decides whether the updater is allowed to offer pre-releases by
	 * default.
	 */
	QString UPDATE_CHANNEL;

	bool UPDATER_ENABLED = false;

	/// A short string identifying this build's platform. For example, "lin64"
	/// or "win32".
	QString BUILD_PLATFORM;

	QString UPDATER_GITHUB_REPO;

	/// A string containing the build timestamp
	QString BUILD_DATE;

	/// User-Agent to use.
	QString USER_AGENT;

	/// User-Agent to use for uncached requests.
	QString USER_AGENT_UNCACHED;

	QString BUILD_ARTIFACT;

	/// Compiler name
	QString COMPILER_NAME;

	/// Compiler version
	QString COMPILER_VERSION;

	/// Target system name (e.g. "Linux", "Windows")
	QString COMPILER_TARGET_SYSTEM;

	/// Target system version
	QString COMPILER_TARGET_SYSTEM_VERSION;

	/// Target system processor (e.g. "x86_64")
	QString COMPILER_TARGET_SYSTEM_PROCESSOR;

	/// URL for notifications
	QString NOTIFICATION_URL;

	/// Used for matching notifications
	QString FULL_VERSION_STR;

	/// The git commit hash of this build
	QString GIT_COMMIT;

	/// The git refspec of this build
	QString GIT_REFSPEC;

	/// The exact git tag of this build, if any
	QString GIT_TAG;

	/// This is printed on start to standard output
	QString VERSION_STR;

	/**
	 * This is used to fetch the news RSS feed.
	 * It is empty by default in CMakeLists.txt
	 */
	QString NEWS_RSS_URL;
	/// Semicolon-separated extra RSS feed URLs, shown by the news viewer
	/// alongside NEWS_RSS_URL. They become feeds 1..N of NewsChecker.
	QString NEWS_EXTRA_FEEDS;

	QString MSAClientID;

	/**
	 * API key you can get from paste.ee when you register an account
	 */
	QString PASTE_EE_KEY;

	/**
	 * Client ID you can get from Imgur when you register an application
	 */
	QString IMGUR_CLIENT_ID;

	/**
	 * Metadata repository URL prefix
	 */
	QString META_URL;

	/**
	 * API key for the CurseForge API
	 */
	QString CURSEFORGE_API_KEY;

	QString BUG_TRACKER_URL;
	QString DISCORD_URL;
	QString SUBREDDIT_URL;
	QString PATREON_URL;

	/**
	 * GitLab Unleash-compatible feature flags endpoint.
	 */
	QString UNLEASH_URL;

	/**
	 * Unleash instance id.
	 */
	QString UNLEASH_INSTANCE_ID;

	/**
	 * Unleash application name / environment
	 */
	QString UNLEASH_APP_NAME;

	/// True when an instance id is configured and feature flags can be fetched.
	bool FEATURE_FLAGS_ENABLED = false;

	QString RESOURCE_BASE = "https://resources.download.minecraft.net/";
	QString LIBRARY_BASE = "https://libraries.minecraft.net/";
	QString IMGUR_BASE_URL = "https://api.imgur.com/3/";
	// TODO(fork): still points at Project Tick's server.
	QString FMLLIBS_BASE_URL = "https://files.projecttick.org/fmllibs/";

	/// Where translation updates are downloaded from; empty means never.
	QString TRANSLATIONS_BASE_URL;

	QString MODPACKSCH_API_BASE_URL = "https://api.modpacks.ch/";

	QString TECHNIC_API_BASE_URL = "https://api.technicpack.net/";
	/**
	 * \brief Identifies our build to the Technic API.
	 *
	 * Technic takes this into account when deciding which download URL to hand
	 * back, and an unrecognised value is not guaranteed to resolve to the
	 * official CDN: asking as "kite" returned a third-party mirror
	 * (bhrepo.com) whose archive failed libarchive's CRC check on every
	 * attempt, byte for byte. "multimc" is the long-recognised value that other
	 * launchers send.
	 *
	 * Keep this in one place. It used to be spelled out inside three separate
	 * request URLs, which is how the three of them came to disagree with the
	 * rest of the world unnoticed.
	 */
	QString TECHNIC_API_BUILD = "multimc";

	QString LEGACY_FTB_CDN_BASE_URL = "https://dist.creeper.host/FTB2/";

	QString ATL_DOWNLOAD_SERVER_URL =
		"https://download.nodecdn.net/containers/atl/";

	/**
	 * \brief Converts the Version to a string.
	 * \return The version number in string format (major.minor.revision.build).
	 */
	QString printableVersionString() const;

	/**
	 * \brief Compiler ID String
	 * \return a string of the form "Name - Version"  of just "Name" if the
	 * version is empty
	 */
	QString compilerID() const;

	/**
	 * \brief System ID String
	 * \return a string of the form "OS Verison Processor"
	 */
	QString systemID() const;
};

extern const Config BuildConfig;
