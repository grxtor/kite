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

#pragma once

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <optional>

/*
 * The crash doctor's catalog (crash-rules.json, formatVersion 1): what a crash
 * looks like in the game's output and what to offer for it. Data only, served
 * by the meta server and embedded as :/kite/crash-rules.json. A rule can pick
 * words and actions from the closed set in CrashActions, nothing else: it
 * names no command, path, JVM argument or URL. The format and the engine's
 * steps are written down in docs/crash-doctor.md.
 */
namespace CrashDoctor
{
	inline constexpr int kFormatVersion = 1;

	/// Where a pattern looks. LogTail is the last lines of the live log, so a
	/// generic exception counts only near the end.
	enum class Source { Log, LogTail, CrashReport, JvmError };

	/// What a named capture may hold; a value that does not fit is dropped.
	enum class CaptureType {
		ModId,
		ModName,
		Text,
		Version,
		ClassName,
		/// A class file version ("65"); the value becomes the Java major (21).
		ClassFileVersion,
		JavaMajor,
		/// A file name ending in .jar, folders stripped.
		JarFile,
		/// A path below the game's config folder: .toml, .json or .json5.
		ConfigFile,
		/// A Mixin config file ("create.mixins.json"); adds "configMod" when an
		/// installed mod matches it.
		MixinConfig
	};

	struct Pattern {
		QRegularExpression regex;
		QList<Source> sources;
	};

	/// Drops another rule's finding: all of it, or (with `same`) only its
	/// instances whose capture first equals one of ours named second.
	struct Suppression {
		QString rule;
		QList<QPair<QString, QString>> same;
	};

	struct FixSpec {
		QString action;
		/// Strings or lists of strings; "{capture}" and "{a|b}" are filled in.
		QVariantMap params;
		QJsonObject label;
	};

	struct Conditions {
		QStringList loaders;
		QString minecraftMin;
		QString minecraftMax;
		QStringList os;
		QStringList arch;
		/// "aboveRecommended", "belowRecommended" or empty.
		QString java;
		std::optional<bool> customJvmArgs;
		std::optional<bool> memoryRoom;
		QList<int> exitCodes;
		/// "normal", "crash" or empty.
		QString exitStatus;
		/// Any of these mods is installed (ids).
		QStringList mods;
		/// Per instance: capture -> fact set ("performanceMods", "installed").
		QHash<QString, QString> inSet;
		/// Per instance: capture -> whether it must be an installed mod.
		QHash<QString, bool> installed;
	};

	struct Rule {
		QString id;
		int priority = 0;
		/// Says what Kite saw (a native frame, an exit code) without knowing
		/// why: the crash still counts as one Kite could not explain, so it can
		/// be sent to Kite and is read again with newer rules.
		bool fallback = false;
		/// Captures that tell instances apart; all captures when empty.
		QStringList key;
		Conditions when;
		QList<Pattern> any;
		QList<Pattern> all;
		QList<Pattern> none;
		bool each = false;
		QHash<QString, CaptureType> captures;
		/// Captures looked up in the class hints (adds <name>Mod, <name>ModName).
		QStringList hints;
		QList<QPair<QString, QStringList>> coalesce;
		QStringList require;
		QHash<QString, QStringList> exclude;
		QList<Suppression> suppresses;
		QJsonObject title;
		QJsonObject groupTitle;
		QJsonObject explanation;
		QJsonObject groupExplanation;
		QList<FixSpec> fixes;
	};

	/// A package prefix of a class and the mod that ships it; an empty mod
	/// means "known, not a mod" (org.lwjgl).
	struct ClassHint {
		QString prefix;
		QString mod;
		QString name;
	};

	struct Catalog {
		int catalogVersion = 0;
		QList<Rule> rules;
		QList<ClassHint> classHints;

		/// The catalog in @p json; nothing (with @p error) for unreadable JSON or
		/// another formatVersion. A broken rule is left out and named in
		/// @p skipped; a fix with an unknown action or parameter is left out of
		/// its rule the same way.
		static std::optional<Catalog> parse(const QByteArray& json,
											QString* error = nullptr,
											QStringList* skipped = nullptr);
		const Rule* rule(const QString& id) const;
	};

	/// The longest PCRE pattern a rule may carry.
	inline constexpr int kMaxPatternLength = 600;
} // namespace CrashDoctor
