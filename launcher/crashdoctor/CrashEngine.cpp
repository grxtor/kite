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

#include "CrashEngine.h"

#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <iterator>

#include "CrashActions.h"
#include "Version.h"

namespace CrashDoctor
{
	namespace
	{
		// Memory offers (Facts::suggestedMemoryMb() and lowerMemoryMb()).
		constexpr int kMbPerGb = 1024;
		constexpr int kMemoryStepMb = 512;
		constexpr double kRaiseFactor = 1.5;
		constexpr int kModdedFloorMb = 4096;
		constexpr int kVanillaFloorMb = 2048;
		constexpr double kMaxShareOfRam = 0.6;
		constexpr int kLeaveForSystemMb = 4096;
		constexpr double kLowerFactor = 0.75;
		constexpr double kLowerShareOfRam = 0.4;
		constexpr int kMinMemoryMb = 1024;
		constexpr int kMinClassFileVersion = 45;
		constexpr int kMaxClassFileVersion = 99;
		constexpr int kClassFileToJava = 44;
		constexpr int kModNameLength = 80;
		constexpr int kTextLength = 160;

		const QString kTurkish = QStringLiteral("tr");
		const QString kEnglish = QStringLiteral("en");

		QString localized(const QJsonObject& text, const QString& language)
		{
			const QString wanted =
				language.startsWith(kTurkish, Qt::CaseInsensitive) ? kTurkish : kEnglish;
			const QString value = text.value(wanted).toString();
			return value.isEmpty() ? text.value(kEnglish).toString() : value;
		}

		QString sourceText(const Evidence& evidence, Source source)
		{
			switch (source) {
				case Source::Log:
					return evidence.log;
				case Source::LogTail: {
					const qsizetype end = evidence.log.size();
					qsizetype start = end;
					for (int lines = 0; lines <= kLogTailLines && start > 0; ++lines) {
						start = evidence.log.lastIndexOf(QLatin1Char('\n'), start - 1);
						if (start < 0) {
							return evidence.log;
						}
					}
					return evidence.log.mid(start + 1);
				}
				case Source::CrashReport:
					return evidence.crashReport;
				case Source::JvmError:
					return evidence.jvmError;
			}
			return QString();
		}

		bool strongSource(Source source)
		{
			return source == Source::CrashReport || source == Source::JvmError;
		}

		QString lineAt(const QString& text, qsizetype position)
		{
			const qsizetype start = text.lastIndexOf(QLatin1Char('\n'), position - 1) + 1;
			qsizetype end = text.indexOf(QLatin1Char('\n'), position);
			if (end < 0) {
				end = text.size();
			}
			return text.mid(start, end - start).trimmed().left(kMaxLineLength);
		}

		/// The line of @p match to show as evidence: in a match over several
		/// lines (a Forge "-- MOD x --" section) the first that names the
		/// problem, else the first line.
		QString evidenceLine(const QString& text, const QRegularExpressionMatch& match)
		{
			static const QRegularExpression kTelling(QStringLiteral(
				"Exception|Error|requires|incompatible|missing|failed|FAILED|not installed"));
			const QString first = lineAt(text, match.capturedStart());
			if (kTelling.match(first).hasMatch()) {
				return first;
			}
			const QStringList lines = match.captured(0).split(QLatin1Char('\n'));
			for (const QString& line : lines) {
				if (kTelling.match(line).hasMatch()) {
					return line.trimmed().left(kMaxLineLength);
				}
			}
			return first;
		}

		// ------------------------------------------------------------ types
		QString stripControl(QString value)
		{
			static const QRegularExpression kControl(QStringLiteral("[\\x00-\\x1f]"));
			return value.remove(kControl).trimmed();
		}

		/// @p raw as @p type, or an invalid QVariant when it does not fit.
		QVariant typed(CaptureType type, const QString& raw)
		{
			static const QRegularExpression kModId(
				QStringLiteral("^[a-z0-9][a-z0-9_.-]{0,63}$"));
			static const QRegularExpression kVersion(QStringLiteral("^[\\w.+\\-]{1,40}$"));
			static const QRegularExpression kClass(QStringLiteral("^[\\w.$]{3,200}$"));
			static const QRegularExpression kJar(
				QStringLiteral("^[^/\\\\:*?\"<>|\\n]{1,160}\\.jar$"));
			static const QRegularExpression kConfig(
				QStringLiteral("^[\\w.\\- /]{1,120}\\.(?:toml|json5?)$"));
			static const QRegularExpression kMixin(QStringLiteral("^[\\w.\\-]{1,120}$"));
			const QString value = raw.trimmed();
			switch (type) {
				case CaptureType::ModId: {
					const QString id = value.toLower();
					return kModId.match(id).hasMatch() ? QVariant(id) : QVariant();
				}
				case CaptureType::ModName: {
					const QString name = stripControl(value).left(kModNameLength);
					return name.isEmpty() ? QVariant() : QVariant(name);
				}
				case CaptureType::Text: {
					const QString text = stripControl(value).left(kTextLength);
					return text.isEmpty() ? QVariant() : QVariant(text);
				}
				case CaptureType::Version:
					return kVersion.match(value).hasMatch() ? QVariant(value) : QVariant();
				case CaptureType::ClassName: {
					QString name = value;
					name.replace(QLatin1Char('/'), QLatin1Char('.'));
					return name.contains(QLatin1Char('.')) && kClass.match(name).hasMatch()
							   ? QVariant(name)
							   : QVariant();
				}
				case CaptureType::ClassFileVersion: {
					bool ok = false;
					const int version = value.toInt(&ok);
					if (!ok || version < kMinClassFileVersion || version > kMaxClassFileVersion) {
						return QVariant();
					}
					return QVariant(version - kClassFileToJava);
				}
				case CaptureType::JavaMajor: {
					bool ok = false;
					const int major = value.toInt(&ok);
					return ok && major >= CrashActions::kMinJavaMajor &&
								   major <= CrashActions::kMaxJavaMajor
							   ? QVariant(major)
							   : QVariant();
				}
				case CaptureType::JarFile: {
					const QString base = value.section(QRegularExpression(QStringLiteral("[/\\\\]")), -1);
					return kJar.match(base).hasMatch() ? QVariant(base) : QVariant();
				}
				case CaptureType::ConfigFile: {
					QString path = value;
					path.replace(QLatin1Char('\\'), QLatin1Char('/'));
					if (path.startsWith(QLatin1Char('/')) ||
						path.split(QLatin1Char('/')).contains(QStringLiteral(".."))) {
						return QVariant();
					}
					return kConfig.match(path).hasMatch() ? QVariant(path) : QVariant();
				}
				case CaptureType::MixinConfig:
					return kMixin.match(value).hasMatch() ? QVariant(value) : QVariant();
			}
			return QVariant();
		}

		/// The installed mod a Mixin config belongs to ("cobblemon-common.mixins.json"
		/// -> "cobblemon"), or "".
		QString configMod(const QString& config, const QSet<QString>& installed)
		{
			static const QRegularExpression kJson(QStringLiteral("\\.json$"));
			static const QRegularExpression kMixins(QStringLiteral("[.\\-_]mixins?$"));
			static const QRegularExpression kSide(
				QStringLiteral("[-_](?:common|fabric|forge|neoforge|client|server)$"));
			QString base = config;
			base.remove(kJson).remove(kMixins);
			const QString first = base.section(QLatin1Char('.'), 0, 0).toLower();
			for (const QString& candidate : {first, QString(first).remove(kSide)}) {
				if (installed.contains(candidate)) {
					return candidate;
				}
			}
			return QString();
		}

		std::optional<ClassHint> hintFor(const QString& className,
										 const QList<ClassHint>& hints)
		{
			std::optional<ClassHint> best;
			for (const ClassHint& hint : hints) {
				const bool covers = className == hint.prefix ||
									className.startsWith(hint.prefix + QLatin1Char('.'));
				if (covers && (!best || hint.prefix.size() > best->prefix.size())) {
					best = hint;
				}
			}
			if (best && best->mod.isEmpty()) {
				return std::nullopt;
			}
			return best;
		}

		// ------------------------------------------------------------ conditions
		bool factsAllow(const Conditions& when, const Facts& facts)
		{
			if (!when.loaders.isEmpty() && !when.loaders.contains(facts.loader)) {
				return false;
			}
			if (!when.minecraftMin.isEmpty() &&
				Version(facts.minecraft) < Version(when.minecraftMin)) {
				return false;
			}
			if (!when.minecraftMax.isEmpty() &&
				Version(facts.minecraft) > Version(when.minecraftMax)) {
				return false;
			}
			if (!when.os.isEmpty() && !when.os.contains(facts.os)) {
				return false;
			}
			if (!when.arch.isEmpty() && !when.arch.contains(facts.arch)) {
				return false;
			}
			if (when.java == QLatin1String("aboveRecommended") &&
				!(facts.javaRecommended > 0 && facts.javaNow > facts.javaRecommended)) {
				return false;
			}
			if (when.java == QLatin1String("belowRecommended") &&
				!(facts.javaNow > 0 && facts.javaNow < facts.javaRecommended)) {
				return false;
			}
			if (when.customJvmArgs && *when.customJvmArgs != facts.customJvmArgs) {
				return false;
			}
			if (when.memoryRoom && *when.memoryRoom != facts.memoryRoom()) {
				return false;
			}
			if (!when.exitCodes.isEmpty() &&
				(!facts.exitCode || !when.exitCodes.contains(*facts.exitCode))) {
				return false;
			}
			if (!when.exitStatus.isEmpty() && when.exitStatus != facts.exitStatus) {
				return false;
			}
			if (!when.mods.isEmpty() &&
				std::none_of(when.mods.begin(), when.mods.end(),
							 [&facts](const QString& id) { return facts.installed.contains(id); })) {
				return false;
			}
			return true;
		}

		bool capturesAllow(const Conditions& when, const QVariantMap& captures,
						   const Facts& facts)
		{
			for (auto it = when.inSet.begin(); it != when.inSet.end(); ++it) {
				const QSet<QString>& set = it.value() == QLatin1String("performanceMods")
											   ? facts.performanceMods
											   : facts.installed;
				if (!set.contains(captures.value(it.key()).toString())) {
					return false;
				}
			}
			for (auto it = when.installed.begin(); it != when.installed.end(); ++it) {
				const QString id = captures.value(it.key()).toString();
				if (id.isEmpty() || facts.installed.contains(id) != it.value()) {
					return false;
				}
			}
			return true;
		}

		// ------------------------------------------------------------ matching
		struct Hit {
			Source source;
			QRegularExpressionMatch match;
			QString text;
		};

		QList<Hit> hits(const Pattern& pattern, const Evidence& evidence, bool every)
		{
			QList<Hit> found;
			for (Source source : pattern.sources) {
				const QString text = sourceText(evidence, source);
				if (text.isEmpty()) {
					continue;
				}
				auto it = pattern.regex.globalMatch(text);
				while (it.hasNext()) {
					found.append({source, it.next(), text});
					if (!every || found.size() >= kMaxFindingsPerRule) {
						return found;
					}
				}
			}
			return found;
		}

		QVariantMap rawCaptures(const Pattern& pattern, const QRegularExpressionMatch& match)
		{
			QVariantMap out;
			for (const QString& name : pattern.regex.namedCaptureGroups()) {
				if (!name.isEmpty() && match.hasCaptured(name)) {
					out.insert(name, match.captured(name));
				}
			}
			return out;
		}

		/// The typed, completed captures of one hit; nothing when the rule's
		/// exclude, require or capture conditions turn it down.
		std::optional<QVariantMap> complete(const Rule& rule, const QVariantMap& raw,
											const Catalog& catalog, const Facts& facts)
		{
			QVariantMap captures;
			for (auto it = raw.begin(); it != raw.end(); ++it) {
				const QVariant value = typed(rule.captures.value(it.key()), it.value().toString());
				if (value.isValid()) {
					captures.insert(it.key(), value);
				}
			}
			for (auto it = rule.captures.begin(); it != rule.captures.end(); ++it) {
				if (it.value() == CaptureType::MixinConfig && captures.contains(it.key())) {
					const QString mod = configMod(captures.value(it.key()).toString(), facts.installed);
					if (!mod.isEmpty()) {
						captures.insert(QStringLiteral("configMod"), mod);
					}
				}
			}
			for (const QString& name : rule.hints) {
				if (!captures.contains(name)) {
					continue;
				}
				if (const auto hint = hintFor(captures.value(name).toString(), catalog.classHints)) {
					captures.insert(name + QStringLiteral("Mod"), hint->mod);
					captures.insert(name + QStringLiteral("ModName"), hint->name);
				}
			}
			for (const auto& [name, alternatives] : rule.coalesce) {
				if (captures.contains(name)) {
					continue;
				}
				for (const QString& alternative : alternatives) {
					if (captures.contains(alternative)) {
						captures.insert(name, captures.value(alternative));
						break;
					}
				}
			}
			for (auto it = rule.exclude.begin(); it != rule.exclude.end(); ++it) {
				if (it.value().contains(captures.value(it.key()).toString())) {
					return std::nullopt;
				}
			}
			for (const QString& name : rule.require) {
				if (!captures.contains(name)) {
					return std::nullopt;
				}
			}
			if (!capturesAllow(rule.when, captures, facts)) {
				return std::nullopt;
			}
			return captures;
		}

		struct Match {
			const Rule* rule = nullptr;
			QList<Finding> findings;
		};

		std::optional<Match> matchRule(const Rule& rule, const Catalog& catalog,
									   const Evidence& evidence, const Facts& facts)
		{
			if (!factsAllow(rule.when, facts)) {
				return std::nullopt;
			}
			for (const Pattern& pattern : rule.none) {
				if (!hits(pattern, evidence, false).isEmpty()) {
					return std::nullopt;
				}
			}
			QVariantMap shared;
			QList<Hit> sharedHits;
			for (const Pattern& pattern : rule.all) {
				const QList<Hit> found = hits(pattern, evidence, false);
				if (found.isEmpty()) {
					return std::nullopt;
				}
				shared.insert(rawCaptures(pattern, found.first().match));
				sharedHits.append(found.first());
			}
			QList<QPair<Hit, QVariantMap>> candidates;
			for (const Pattern& pattern : rule.any) {
				const QList<Hit> found = hits(pattern, evidence, rule.each);
				for (const Hit& hit : found) {
					QVariantMap raw = shared;
					raw.insert(rawCaptures(pattern, hit.match));
					candidates.append({hit, raw});
				}
				if (!found.isEmpty() && !rule.each) {
					break;
				}
			}
			if (rule.any.isEmpty() && !sharedHits.isEmpty()) {
				candidates.append({sharedHits.first(), shared});
			}

			Match result;
			result.rule = &rule;
			QHash<QString, int> seen;
			for (const auto& [hit, raw] : candidates) {
				const auto captures = complete(rule, raw, catalog, facts);
				if (!captures) {
					continue;
				}
				const QStringList keyNames = rule.key.isEmpty() ? captures->keys() : rule.key;
				QStringList keyValues;
				for (const QString& name : keyNames) {
					keyValues << name + QLatin1Char('=') + captures->value(name).toString();
				}
				const QString key = keyValues.join(QLatin1Char('\n'));
				const Source source = sharedHits.isEmpty() || !strongSource(sharedHits.first().source)
										  ? hit.source
										  : sharedHits.first().source;
				if (seen.contains(key)) {
					Finding& existing = result.findings[seen.value(key)];
					if (strongSource(source)) {
						existing.source = source;
					}
					continue;
				}
				seen.insert(key, int(result.findings.size()));
				result.findings.append({*captures, evidenceLine(hit.text, hit.match), source});
				if (result.findings.size() >= kMaxFindingsPerRule) {
					break;
				}
			}
			if (result.findings.isEmpty()) {
				return std::nullopt;
			}
			return result;
		}

		void suppress(QList<Match>& matches)
		{
			for (const Match& by : matches) {
				for (const Suppression& suppression : by.rule->suppresses) {
					for (Match& target : matches) {
						if (target.rule == by.rule || target.rule->id != suppression.rule) {
							continue;
						}
						if (suppression.same.isEmpty()) {
							target.findings.clear();
							continue;
						}
						target.findings.erase(
							std::remove_if(target.findings.begin(), target.findings.end(),
										   [&](const Finding& finding) {
											   return std::any_of(
												   by.findings.begin(), by.findings.end(),
												   [&](const Finding& ours) {
													   for (const auto& [theirs, mine] : suppression.same) {
														   const QVariant value = finding.captures.value(theirs);
														   if (!value.isValid() ||
															   value != ours.captures.value(mine)) {
															   return false;
														   }
													   }
													   return true;
												   });
										   }),
							target.findings.end());
					}
				}
			}
			matches.erase(std::remove_if(matches.begin(), matches.end(),
										 [](const Match& match) { return match.findings.isEmpty(); }),
						  matches.end());
		}

		// ------------------------------------------------------------ rendering
		QVariant factValue(const QString& name, const Facts& facts)
		{
			if (name == QLatin1String("mc"))
				return facts.minecraft;
			if (name == QLatin1String("loader"))
				return facts.loader;
			if (name == QLatin1String("javaNow"))
				return facts.javaNow > 0 ? QVariant(facts.javaNow) : QVariant();
			if (name == QLatin1String("javaRecommended"))
				return facts.javaRecommended > 0 ? QVariant(facts.javaRecommended) : QVariant();
			if (name == QLatin1String("memoryMb"))
				return facts.memoryMb;
			if (name == QLatin1String("suggestedMemoryMb"))
				return facts.suggestedMemoryMb();
			if (name == QLatin1String("lowerMemoryMb"))
				return facts.lowerMemoryMb();
			if (name == QLatin1String("systemMemoryMb"))
				return facts.systemMemoryMb;
			if (name == QLatin1String("exitCode"))
				return facts.exitCode ? QVariant(*facts.exitCode) : QVariant();
			return QVariant();
		}

		QVariant valueOf(const QString& alternatives, const QVariantMap& captures,
						 const Facts& facts, int count)
		{
			for (const QString& name : alternatives.split(QLatin1Char('|'))) {
				if (name == QLatin1String("count")) {
					return count;
				}
				const QVariant value = captures.contains(name) ? captures.value(name)
															   : factValue(name, facts);
				if (value.isValid() && !value.toString().isEmpty()) {
					return value;
				}
			}
			return QVariant();
		}

		const QRegularExpression& placeholder()
		{
			static const QRegularExpression kPlaceholder(
				QStringLiteral(R"(\{([A-Za-z][A-Za-z0-9]*(?:\|[A-Za-z][A-Za-z0-9]*)*)\})"));
			return kPlaceholder;
		}

		/// @p text with its placeholders filled; @p complete is cleared when one
		/// had no value.
		QString fill(const QString& text, const QVariantMap& captures, const Facts& facts,
					 int count, bool* complete = nullptr)
		{
			QString out;
			qsizetype last = 0;
			auto it = placeholder().globalMatch(text);
			while (it.hasNext()) {
				const QRegularExpressionMatch match = it.next();
				out += text.mid(last, match.capturedStart() - last);
				const QVariant value = valueOf(match.captured(1), captures, facts, count);
				if (!value.isValid() && complete) {
					*complete = false;
				}
				out += value.toString();
				last = match.capturedEnd();
			}
			out += text.mid(last);
			return out;
		}

		/// A parameter written as exactly one placeholder keeps its value's type.
		QVariant resolveValue(const QString& text, const QVariantMap& captures,
							  const Facts& facts, bool* complete)
		{
			const QRegularExpressionMatch whole = placeholder().match(text);
			if (whole.hasMatch() && whole.capturedStart() == 0 && whole.capturedEnd() == text.size()) {
				const QVariant value = valueOf(whole.captured(1), captures, facts, 0);
				if (!value.isValid()) {
					*complete = false;
				}
				return value;
			}
			return fill(text, captures, facts, 0, complete);
		}

		bool paramFits(const CrashActions::ParamSpec& spec, const QVariant& value)
		{
			using CrashActions::ParamType;
			const QString text = value.toString();
			switch (spec.type) {
				case ParamType::JavaMajor: {
					bool ok = false;
					const int major = text.toInt(&ok);
					return ok && major >= CrashActions::kMinJavaMajor &&
						   major <= CrashActions::kMaxJavaMajor;
				}
				case ParamType::MemoryMb: {
					bool ok = false;
					return text.toInt(&ok) > 0 && ok;
				}
				case ParamType::ModId:
					return typed(CaptureType::ModId, text).isValid();
				case ParamType::ModRef:
				case ParamType::ModRefList:
					return typed(CaptureType::ModId, text).isValid() ||
						   typed(CaptureType::JarFile, text).isValid();
				case ParamType::ConfigFile:
					return typed(CaptureType::ConfigFile, text).isValid();
				case ParamType::Choice:
					return spec.choices.contains(text);
			}
			return false;
		}

		/// @p fix for one finding, or nothing when a parameter is missing or
		/// does not fit its type.
		std::optional<ResolvedFix> resolve(const FixSpec& fix, const CrashActions::Spec& spec,
										   const Finding& finding, const Facts& facts,
										   const QString& language)
		{
			ResolvedFix out;
			out.action = fix.action;
			for (const CrashActions::ParamSpec& param : spec.params) {
				if (!fix.params.contains(param.name)) {
					continue;
				}
				const QVariant raw = fix.params.value(param.name);
				if (param.type == CrashActions::ParamType::ModRefList) {
					QStringList values;
					for (const QString& item : raw.toStringList()) {
						bool complete = true;
						const QVariant value = resolveValue(item, finding.captures, facts, &complete);
						const QString raw = value.toString();
						// Mod ids compare lower case; jar names stay as they are.
						const QString text =
							raw.endsWith(QLatin1String(".jar")) ? raw : raw.toLower();
						if (complete && paramFits(param, text) && !values.contains(text)) {
							values.append(text);
						}
					}
					if (values.isEmpty()) {
						return std::nullopt;
					}
					out.params.insert(param.name, values);
					continue;
				}
				bool complete = true;
				const QVariant value = resolveValue(raw.toString(), finding.captures, facts, &complete);
				if (!complete || !paramFits(param, value)) {
					return std::nullopt;
				}
				out.params.insert(param.name, value);
			}
			bool complete = true;
			out.label = fill(localized(fix.label, language), finding.captures, facts, 1, &complete);
			return out;
		}

		/// The label of a fix that acts on every finding of a card at once.
		QString batchLabel(const QString& action, const QString& language)
		{
			const bool turkish = language.startsWith(kTurkish, Qt::CaseInsensitive);
			if (action == QLatin1String("installMissingDependency"))
				return turkish ? QStringLiteral("Hepsini yükle") : QStringLiteral("Install all");
			if (action == QLatin1String("updateMod"))
				return turkish ? QStringLiteral("Hepsini güncelle") : QStringLiteral("Update all");
			if (action == QLatin1String("disableMod"))
				return turkish ? QStringLiteral("Hepsini kapat") : QStringLiteral("Turn them off");
			return QString();
		}

		QList<ResolvedFix> fixesFor(const Rule& rule, const QList<Finding>& findings,
									const Facts& facts, const QString& language)
		{
			QList<ResolvedFix> fixes;
			const auto known = [&fixes](const ResolvedFix& fix) {
				return std::any_of(fixes.begin(), fixes.end(), [&](const ResolvedFix& other) {
					return other.action == fix.action && other.params == fix.params;
				});
			};
			for (const FixSpec& fix : rule.fixes) {
				const CrashActions::Spec* spec = CrashActions::find(fix.action);
				if (!spec || (spec->windowsOnly && facts.os != QLatin1String("windows")) ||
					(spec->needsShaders && !facts.shadersOn)) {
					continue;
				}
				QList<ResolvedFix> each;
				for (const Finding& finding : findings) {
					if (auto resolved = resolve(fix, *spec, finding, facts, language)) {
						each.append(*resolved);
					}
				}
				if (each.isEmpty()) {
					continue;
				}
				ResolvedFix chosen = each.first();
				if (spec->batch && findings.size() > 1) {
					// One run for the whole card: every id, once.
					for (auto it = chosen.params.begin(); it != chosen.params.end(); ++it) {
						QStringList all;
						for (const ResolvedFix& one : each) {
							for (const QString& value : one.params.value(it.key()).toStringList()) {
								if (!all.contains(value)) {
									all.append(value);
								}
							}
						}
						it.value() = all;
					}
					const QString label = batchLabel(fix.action, language);
					if (!label.isEmpty() && each.size() > 1) {
						chosen.label = label;
					}
				}
				if (!known(chosen)) {
					fixes.append(chosen);
				}
			}
			return fixes;
		}
	} // namespace

	bool Facts::modded() const
	{
		return !loader.isEmpty() && loader != QLatin1String("vanilla");
	}

	int Facts::suggestedMemoryMb() const
	{
		const int floor = modded() ? kModdedFloorMb : kVanillaFloorMb;
		const double wanted = std::max(memoryMb * kRaiseFactor, double(floor));
		const int rounded = int(std::ceil(wanted / kMbPerGb)) * kMbPerGb;
		if (systemMemoryMb <= 0) {
			return rounded;
		}
		const int cap = std::min(int(systemMemoryMb * kMaxShareOfRam) / kMemoryStepMb * kMemoryStepMb,
								 systemMemoryMb - kLeaveForSystemMb);
		return std::min(rounded, cap);
	}

	int Facts::lowerMemoryMb() const
	{
		double wanted = memoryMb * kLowerFactor;
		if (systemMemoryMb > 0) {
			wanted = std::min(wanted, systemMemoryMb * kLowerShareOfRam);
		}
		return std::max(kMinMemoryMb, int(wanted) / kMemoryStepMb * kMemoryStepMb);
	}

	bool Facts::memoryRoom() const
	{
		return suggestedMemoryMb() > memoryMb;
	}

	QString normalize(const QString& text)
	{
		static const QRegularExpression kAnsi(QStringLiteral("\\x1b\\[[0-9;]*[A-Za-z]"));
		static const QRegularExpression kFormatting(QStringLiteral("§[0-9a-fk-orA-FK-OR]"));
		QString out = text;
		out.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
		out.replace(QLatin1Char('\r'), QLatin1Char('\n'));
		out.remove(kAnsi);
		out.remove(kFormatting);
		return out;
	}

	const Cause* firstExplained(const QList<Cause>& causes)
	{
		const auto it = std::find_if(causes.begin(), causes.end(),
									 [](const Cause& cause) { return !cause.fallback; });
		return it == causes.end() ? nullptr : &*it;
	}

	QList<Cause> shownCauses(const QList<Cause>& causes)
	{
		if (!firstExplained(causes)) {
			return causes;
		}
		QList<Cause> shown;
		std::copy_if(causes.begin(), causes.end(), std::back_inserter(shown),
					 [](const Cause& cause) { return !cause.fallback; });
		return shown;
	}

	QList<Cause> analyse(const Catalog& catalog, const Evidence& raw, const Facts& facts,
						 const QString& language)
	{
		const Evidence evidence{normalize(raw.log), normalize(raw.crashReport),
								normalize(raw.jvmError)};
		QList<Match> matches;
		for (const Rule& rule : catalog.rules) {
			if (auto match = matchRule(rule, catalog, evidence, facts)) {
				matches.append(*match);
			}
		}
		suppress(matches);

		QList<Cause> causes;
		for (const Match& match : matches) {
			const Rule& rule = *match.rule;
			Cause cause;
			cause.ruleId = rule.id;
			cause.fallback = rule.fallback;
			cause.findings = match.findings;
			const bool strong = std::any_of(match.findings.begin(), match.findings.end(),
											[](const Finding& finding) { return strongSource(finding.source); });
			const int count = int(match.findings.size());
			cause.score = rule.priority + (strong ? kEvidenceSourceBonus : 0) +
						  std::min(count - 1, kMaxCountBonus);
			const QVariantMap& first = match.findings.first().captures;
			const bool group = count > 1;
			const QJsonObject& title = group && !rule.groupTitle.isEmpty() ? rule.groupTitle : rule.title;
			const QJsonObject& explanation =
				group && !rule.groupExplanation.isEmpty() ? rule.groupExplanation : rule.explanation;
			cause.title = fill(localized(title, language), first, facts, count);
			cause.explanation = fill(localized(explanation, language), first, facts, count);
			cause.fixes = fixesFor(rule, match.findings, facts, language);
			causes.append(cause);
		}
		std::stable_sort(causes.begin(), causes.end(),
						 [](const Cause& a, const Cause& b) { return a.score > b.score; });
		return causes;
	}
} // namespace CrashDoctor
