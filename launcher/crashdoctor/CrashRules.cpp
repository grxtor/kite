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

#include "CrashRules.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

#include "CrashActions.h"
#include "CrashExitCodes.h"

namespace CrashDoctor
{
	namespace
	{
		/// PCRE2 gives up on one match after this many steps (a lowering only:
		/// rules may not set their own, see validPattern()). Every catalog
		/// pattern stays far below it on the corpus.
		constexpr int kMatchLimit = 2000000;
		constexpr int kMaxFixes = 3;
		constexpr int kMaxPatterns = 8;
		constexpr int kMaxTextLength = 400;

		const QHash<QString, Source>& sourceNames()
		{
			static const QHash<QString, Source> names{
				{"log", Source::Log},
				{"logTail", Source::LogTail},
				{"crashReport", Source::CrashReport},
				{"hsErr", Source::JvmError},
			};
			return names;
		}

		const QHash<QString, CaptureType>& captureTypeNames()
		{
			static const QHash<QString, CaptureType> names{
				{"modId", CaptureType::ModId},
				{"modName", CaptureType::ModName},
				{"text", CaptureType::Text},
				{"version", CaptureType::Version},
				{"className", CaptureType::ClassName},
				{"classFileVersion", CaptureType::ClassFileVersion},
				{"javaMajor", CaptureType::JavaMajor},
				{"jarFile", CaptureType::JarFile},
				{"configFile", CaptureType::ConfigFile},
				{"mixinConfig", CaptureType::MixinConfig},
			};
			return names;
		}

		/// Facts a text or parameter may name besides the captures.
		const QSet<QString>& factNames()
		{
			static const QSet<QString> names{
				"mc", "loader", "javaNow", "javaRecommended", "memoryMb",
				"suggestedMemoryMb", "lowerMemoryMb", "systemMemoryMb",
				"exitCode", "count"};
			return names;
		}

		const QSet<QString>& factSets()
		{
			static const QSet<QString> sets{"performanceMods", "installed"};
			return sets;
		}

		const QList<Source> kDefaultSources{Source::CrashReport, Source::JvmError,
											Source::LogTail};

		QStringList strings(const QJsonValue& value)
		{
			QStringList out;
			for (const QJsonValue& item : value.toArray()) {
				out.append(item.toString());
			}
			return out;
		}

		/// The placeholders "{a|b}" of @p text, as alternatives.
		QStringList placeholders(const QString& text)
		{
			static const QRegularExpression kPlaceholder(
				QStringLiteral(R"(\{([A-Za-z][A-Za-z0-9]*(?:\|[A-Za-z][A-Za-z0-9]*)*)\})"));
			QStringList names;
			auto it = kPlaceholder.globalMatch(text);
			while (it.hasNext()) {
				names << it.next().captured(1).split(QLatin1Char('|'));
			}
			return names;
		}

		bool validPattern(const QJsonObject& object, Pattern* out,
						  QSet<QString>* groups, QString* error)
		{
			const QString re = object.value("re").toString();
			if (re.isEmpty() || re.size() > kMaxPatternLength) {
				*error = QStringLiteral("empty or too long pattern");
				return false;
			}
			// No PCRE verbs (they could raise limits) and no inline flags.
			if (re.contains(QLatin1String("(*")) || re.contains(QLatin1String("(?s")) ||
				re.contains(QLatin1String("(?m")) || re.contains(QLatin1String("(?x")) ||
				re.contains(QLatin1String("(?i"))) {
				*error = QStringLiteral("verbs or inline flags in a pattern");
				return false;
			}
			QRegularExpression::PatternOptions options =
				QRegularExpression::MultilineOption;
			if (object.value("i").toBool()) {
				options |= QRegularExpression::CaseInsensitiveOption;
			}
			out->regex = QRegularExpression(
				QStringLiteral("(*LIMIT_MATCH=%1)").arg(kMatchLimit) + re, options);
			if (!out->regex.isValid()) {
				*error = QStringLiteral("pattern does not compile: %1")
							 .arg(out->regex.errorString());
				return false;
			}
			out->regex.optimize();
			for (const QString& name : out->regex.namedCaptureGroups()) {
				if (!name.isEmpty()) {
					groups->insert(name);
				}
			}
			const QJsonValue in = object.value("in");
			if (in.isUndefined()) {
				out->sources = kDefaultSources;
			} else {
				for (const QString& name : strings(in)) {
					if (!sourceNames().contains(name)) {
						*error = QStringLiteral("unknown source %1").arg(name);
						return false;
					}
					out->sources.append(sourceNames().value(name));
				}
			}
			return true;
		}

		bool readPatterns(const QJsonValue& value, QList<Pattern>* out,
						  QSet<QString>* groups, QString* error)
		{
			const QJsonArray array = value.toArray();
			if (array.size() > kMaxPatterns) {
				*error = QStringLiteral("too many patterns");
				return false;
			}
			for (const QJsonValue& item : array) {
				Pattern pattern;
				if (!validPattern(item.toObject(), &pattern, groups, error)) {
					return false;
				}
				out->append(pattern);
			}
			return true;
		}

		bool hasBothLanguages(const QJsonObject& text)
		{
			const QString en = text.value("en").toString();
			const QString tr = text.value("tr").toString();
			return !en.isEmpty() && !tr.isEmpty() && en.size() <= kMaxTextLength &&
				   tr.size() <= kMaxTextLength;
		}

		void readConditions(const QJsonObject& when, Conditions* out)
		{
			out->loaders = strings(when.value("loaders"));
			const QJsonObject mc = when.value("minecraft").toObject();
			out->minecraftMin = mc.value("min").toString();
			out->minecraftMax = mc.value("max").toString();
			out->os = strings(when.value("os"));
			out->arch = strings(when.value("arch"));
			out->java = when.value("java").toString();
			if (when.contains("customJvmArgs")) {
				out->customJvmArgs = when.value("customJvmArgs").toBool();
			}
			if (when.contains("memoryRoom")) {
				out->memoryRoom = when.value("memoryRoom").toBool();
			}
			// Signed or unsigned: a Windows status matches in both forms.
			for (const QJsonValue& code : when.value("exitCode").toArray()) {
				if (const auto folded = ExitCodes::fromNumber(code.toInteger())) {
					out->exitCodes.append(*folded);
				}
			}
			out->exitStatus = when.value("exitStatus").toString();
			out->mods = strings(when.value("mods"));
			const QJsonObject inSet = when.value("inSet").toObject();
			for (auto it = inSet.begin(); it != inSet.end(); ++it) {
				out->inSet.insert(it.key(), it.value().toString());
			}
			const QJsonObject installed = when.value("installed").toObject();
			for (auto it = installed.begin(); it != installed.end(); ++it) {
				out->installed.insert(it.key(), it.value().toBool());
			}
		}

		/// A fix whose action and parameters are in the closed set.
		std::optional<FixSpec> readFix(const QJsonObject& object,
									   const QSet<QString>& names,
									   QString* error)
		{
			FixSpec fix;
			fix.action = object.value("action").toString();
			const CrashActions::Spec* spec = CrashActions::find(fix.action);
			if (!spec) {
				*error = QStringLiteral("unknown action %1").arg(fix.action);
				return std::nullopt;
			}
			fix.label = object.value("label").toObject();
			if (!hasBothLanguages(fix.label)) {
				*error = QStringLiteral("%1: label needs en and tr").arg(fix.action);
				return std::nullopt;
			}
			const QJsonObject params = object.value("params").toObject();
			QSet<QString> known;
			for (const CrashActions::ParamSpec& param : spec->params) {
				known.insert(param.name);
				const QJsonValue value = params.value(param.name);
				if (value.isUndefined()) {
					if (!param.optional) {
						*error = QStringLiteral("%1: missing %2").arg(fix.action, param.name);
						return std::nullopt;
					}
					continue;
				}
				if (param.type == CrashActions::ParamType::Choice &&
					!param.choices.contains(value.toString())) {
					*error = QStringLiteral("%1: %2 is no choice").arg(fix.action, param.name);
					return std::nullopt;
				}
				const QStringList values = value.isArray() ? strings(value)
														   : QStringList{value.toString()};
				for (const QString& item : values) {
					for (const QString& placeholder : placeholders(item)) {
						if (!names.contains(placeholder)) {
							*error = QStringLiteral("%1: unknown placeholder %2")
										 .arg(fix.action, placeholder);
							return std::nullopt;
						}
					}
				}
				fix.params.insert(param.name, value.isArray()
												  ? QVariant(values)
												  : QVariant(value.toString()));
			}
			for (auto it = params.begin(); it != params.end(); ++it) {
				if (!known.contains(it.key())) {
					*error = QStringLiteral("%1: unknown parameter %2").arg(fix.action, it.key());
					return std::nullopt;
				}
			}
			return fix;
		}

		std::optional<Rule> readRule(const QJsonObject& object,
									 QStringList* skipped)
		{
			Rule rule;
			QString error;
			rule.id = object.value("id").toString();
			static const QRegularExpression kId(
				QStringLiteral("^[a-z0-9]+(\\.[a-z0-9-]+)+$"));
			const auto fail = [&](const QString& why) {
				if (skipped) {
					skipped->append(QStringLiteral("%1: %2").arg(
						rule.id.isEmpty() ? QStringLiteral("?") : rule.id, why));
				}
				return std::nullopt;
			};
			if (!kId.match(rule.id).hasMatch()) {
				return fail(QStringLiteral("bad id"));
			}
			rule.priority = object.value("priority").toInt(-1);
			if (rule.priority < 0 || rule.priority > 100) {
				return fail(QStringLiteral("priority out of range"));
			}
			rule.fallback = object.value("fallback").toBool();
			const QJsonValue key = object.value("key");
			rule.key = key.isArray() ? strings(key)
				: key.isString()	 ? QStringList{key.toString()}
									 : QStringList{};
			readConditions(object.value("when").toObject(), &rule.when);
			for (const QString& set : rule.when.inSet) {
				if (!factSets().contains(set)) {
					return fail(QStringLiteral("unknown set %1").arg(set));
				}
			}

			const QJsonObject captures = object.value("captures").toObject();
			for (auto it = captures.begin(); it != captures.end(); ++it) {
				if (!captureTypeNames().contains(it.value().toString())) {
					return fail(QStringLiteral("unknown capture type for %1").arg(it.key()));
				}
				rule.captures.insert(it.key(),
									 captureTypeNames().value(it.value().toString()));
			}

			const QJsonObject match = object.value("match").toObject();
			QSet<QString> groups;
			if (!readPatterns(match.value("any"), &rule.any, &groups, &error) ||
				!readPatterns(match.value("all"), &rule.all, &groups, &error) ||
				!readPatterns(match.value("none"), &rule.none, &groups, &error)) {
				return fail(error);
			}
			if (rule.any.isEmpty() && rule.all.isEmpty()) {
				return fail(QStringLiteral("no patterns"));
			}
			rule.each = match.value("each").toBool();
			for (const QString& group : groups) {
				if (!rule.captures.contains(group)) {
					return fail(QStringLiteral("group %1 has no type").arg(group));
				}
			}

			// Every name a text, key or parameter may use.
			QSet<QString> names(factNames());
			for (auto it = rule.captures.begin(); it != rule.captures.end(); ++it) {
				names.insert(it.key());
				if (it.value() == CaptureType::MixinConfig) {
					names.insert(QStringLiteral("configMod"));
				}
			}
			const QJsonObject hints = object.value("hints").toObject();
			for (auto it = hints.begin(); it != hints.end(); ++it) {
				if (!rule.captures.contains(it.key())) {
					return fail(QStringLiteral("hint for unknown %1").arg(it.key()));
				}
				rule.hints.append(it.key());
				names.insert(it.key() + QStringLiteral("Mod"));
				names.insert(it.key() + QStringLiteral("ModName"));
			}
			const QJsonObject coalesce = object.value("coalesce").toObject();
			for (auto it = coalesce.begin(); it != coalesce.end(); ++it) {
				rule.coalesce.append({it.key(), strings(it.value())});
				names.insert(it.key());
			}
			rule.require = strings(object.value("require"));
			const QJsonObject exclude = object.value("exclude").toObject();
			for (auto it = exclude.begin(); it != exclude.end(); ++it) {
				rule.exclude.insert(it.key(), strings(it.value()));
			}
			for (const QString& name : rule.key + rule.require + rule.exclude.keys() +
										   rule.when.inSet.keys() + rule.when.installed.keys()) {
				if (!names.contains(name)) {
					return fail(QStringLiteral("unknown capture %1").arg(name));
				}
			}

			for (const QJsonValue& item : object.value("suppresses").toArray()) {
				Suppression suppression;
				if (item.isString()) {
					suppression.rule = item.toString();
				} else {
					const QJsonObject s = item.toObject();
					suppression.rule = s.value("rule").toString();
					const QJsonObject same = s.value("same").toObject();
					for (auto it = same.begin(); it != same.end(); ++it) {
						suppression.same.append({it.key(), it.value().toString()});
					}
				}
				rule.suppresses.append(suppression);
			}

			rule.title = object.value("title").toObject();
			rule.groupTitle = object.value("groupTitle").toObject();
			rule.explanation = object.value("explanation").toObject();
			rule.groupExplanation = object.value("groupExplanation").toObject();
			if (!hasBothLanguages(rule.title) || !hasBothLanguages(rule.explanation) ||
				(!rule.groupTitle.isEmpty() && !hasBothLanguages(rule.groupTitle)) ||
				(!rule.groupExplanation.isEmpty() && !hasBothLanguages(rule.groupExplanation))) {
				return fail(QStringLiteral("texts need en and tr"));
			}
			for (const QJsonObject& text :
				 {rule.title, rule.groupTitle, rule.explanation, rule.groupExplanation}) {
				for (const QJsonValue& value : text) {
					for (const QString& placeholder : placeholders(value.toString())) {
						if (!names.contains(placeholder)) {
							return fail(QStringLiteral("unknown placeholder %1").arg(placeholder));
						}
					}
				}
			}

			const QJsonArray fixes = object.value("fixes").toArray();
			for (const QJsonValue& item : fixes) {
				if (rule.fixes.size() == kMaxFixes) {
					break;
				}
				if (auto fix = readFix(item.toObject(), names, &error)) {
					rule.fixes.append(*fix);
				} else if (skipped) {
					skipped->append(QStringLiteral("%1: fix left out, %2").arg(rule.id, error));
				}
			}
			return rule;
		}
	} // namespace

	std::optional<Catalog> Catalog::parse(const QByteArray& json, QString* error,
										  QStringList* skipped)
	{
		QJsonParseError parseError;
		const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
		const QJsonObject root = document.object();
		const auto fail = [error](const QString& why) {
			if (error) {
				*error = why;
			}
			return std::nullopt;
		};
		if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
			return fail(QStringLiteral("not a JSON object: %1").arg(parseError.errorString()));
		}
		if (root.value("formatVersion").toInt() != kFormatVersion) {
			return fail(QStringLiteral("unknown formatVersion"));
		}
		Catalog catalog;
		catalog.catalogVersion = root.value("catalogVersion").toInt();
		for (const QJsonValue& item : root.value("classHints").toArray()) {
			const QJsonObject hint = item.toObject();
			const QString prefix = hint.value("prefix").toString();
			if (!prefix.isEmpty()) {
				catalog.classHints.append(
					{prefix, hint.value("mod").toString(), hint.value("name").toString()});
			}
		}
		QSet<QString> ids;
		for (const QJsonValue& item : root.value("rules").toArray()) {
			auto rule = readRule(item.toObject(), skipped);
			if (!rule) {
				continue;
			}
			if (ids.contains(rule->id)) {
				if (skipped) {
					skipped->append(QStringLiteral("%1: duplicate id").arg(rule->id));
				}
				continue;
			}
			ids.insert(rule->id);
			catalog.rules.append(*rule);
		}
		return catalog;
	}

	const Rule* Catalog::rule(const QString& id) const
	{
		for (const Rule& candidate : rules) {
			if (candidate.id == id) {
				return &candidate;
			}
		}
		return nullptr;
	}
} // namespace CrashDoctor
