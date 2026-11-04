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

#include "CrashPayload.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

namespace CrashPayload
{
	namespace
	{
		using Re = QRegularExpression;

		constexpr int kMinJava = 5;
		constexpr int kMaxJava = 99;
		constexpr int kMaxVersionLength = 64;
		constexpr int kMaxShortLength = 32;
		const QString kVanilla = QStringLiteral("vanilla");
		const QString kDot = QStringLiteral(" · ");
		const QStringList kLoaders{QStringLiteral("fabric"), QStringLiteral("quilt"),
								   QStringLiteral("forge"), QStringLiteral("neoforge")};
		// The same patterns the server checks (server/friends/src/crashReports.ts).
		const Re kModId(QStringLiteral("^[a-z0-9][a-z0-9_.-]{0,63}$"));
		const Re kNotVersion(QStringLiteral("[^A-Za-z0-9._+-]"));
		const Re kNotMinecraft(QStringLiteral("[^A-Za-z0-9._+ -]"));
		const Re kRuleId(QStringLiteral("^[a-z0-9]+(\\.[a-z0-9-]+)+$"));

		QString cleaned(QString text, const Re& notAllowed, int max)
		{
			return text.remove(notAllowed).left(max).trimmed();
		}

		QString loaderName(const QString& type)
		{
			static const QHash<QString, QString> names{
				{QStringLiteral("fabric"), QStringLiteral("Fabric")}, {QStringLiteral("quilt"), QStringLiteral("Quilt")},
				{QStringLiteral("forge"), QStringLiteral("Forge")}, {QStringLiteral("neoforge"), QStringLiteral("NeoForge")},
			};
			return names.value(type, QStringLiteral("Vanilla"));
		}

		QString osName(const QString& os)
		{
			return os == QLatin1String("macos")	  ? QStringLiteral("macOS")
				   : os == QLatin1String("windows") ? QStringLiteral("Windows")
													: QStringLiteral("Linux");
		}

		QJsonArray modsOf(const CrashDoctor::Report& report)
		{
			QJsonArray mods;
			QSet<QString> seen;
			for (const CrashDoctor::ModEntry& mod : report.mods) {
				if (mods.size() >= kMaxMods || seen.contains(mod.id) || !kModId.match(mod.id).hasMatch()) {
					continue;
				}
				seen.insert(mod.id);
				mods.append(QJsonObject{{"id", mod.id}, {"version", cleaned(mod.version, kNotVersion, kMaxVersionLength)}});
			}
			return mods;
		}
	} // namespace

	bool sendable(const CrashDoctor::Report& report)
	{
		return !report.lines.lines.isEmpty() &&
			   (!CrashDoctor::firstExplained(report.causes) || report.fixDidNotHelp);
	}

	QJsonObject build(const CrashDoctor::Report& report, const QString& kiteVersion)
	{
		const CrashDoctor::Facts& facts = report.facts;
		const bool modded = kLoaders.contains(facts.loader);
		QJsonObject loader{{"type", modded ? facts.loader : kVanilla}};
		const QString loaderVersion = cleaned(report.loaderVersion, kNotVersion, kMaxVersionLength);
		if (modded && !loaderVersion.isEmpty()) {
			loader.insert("version", loaderVersion);
		}
		QJsonArray rules;
		for (const CrashDoctor::Cause& cause : report.causes) {
			if (rules.size() < kMaxRules && kRuleId.match(cause.ruleId).hasMatch() && !rules.contains(cause.ruleId)) {
				rules.append(cause.ruleId);
			}
		}
		const QString minecraft = cleaned(facts.minecraft, kNotMinecraft, kMaxShortLength);
		QJsonObject payload{
			{"formatVersion", kFormatVersion},
			{"signature", report.signature},
			{"kite", cleaned(kiteVersion, kNotVersion, kMaxShortLength)},
			{"os", facts.os},
			{"arch", facts.arch},
			{"java", facts.javaNow >= kMinJava && facts.javaNow <= kMaxJava ? QJsonValue(facts.javaNow)
																			: QJsonValue(QJsonValue::Null)},
			{"minecraft", minecraft.isEmpty() ? QStringLiteral("unknown") : minecraft},
			{"loader", loader},
			{"rules", rules},
			{"mods", modded ? modsOf(report) : QJsonArray()},
			{"lines", QJsonArray::fromStringList(report.lines.lines)},
		};
		if (modded && kModId.match(report.lines.culprit).hasMatch()) {
			payload.insert("culprit", report.lines.culprit);
		}
		return payload;
	}

	QVariantMap readable(const QJsonObject& payload)
	{
		const QJsonObject loader = payload.value("loader").toObject();
		QStringList versions{QStringLiteral("Kite ") + payload.value("kite").toString(),
							 osName(payload.value("os").toString()) + QLatin1Char(' ') + payload.value("arch").toString()};
		if (payload.value("java").isDouble()) {
			versions << QStringLiteral("Java %1").arg(payload.value("java").toInt());
		}
		QStringList game{QStringLiteral("Minecraft ") + payload.value("minecraft").toString()};
		game << (loaderName(loader.value("type").toString()) + QLatin1Char(' ') + loader.value("version").toString()).trimmed();
		QStringList mods;
		for (const QJsonValue& mod : payload.value("mods").toArray()) {
			mods << (mod["id"].toString() + QLatin1Char(' ') + mod["version"].toString()).trimmed();
		}
		QStringList rules;
		for (const QJsonValue& rule : payload.value("rules").toArray()) {
			rules << rule.toString();
		}
		return {{"versions", versions.join(kDot)},
				{"game", game.join(kDot)},
				{"lines", payload.value("lines").toVariant()},
				{"mods", mods.join(QStringLiteral(", "))},
				{"modCount", int(mods.size())},
				{"rules", rules.join(QStringLiteral(", "))},
				{"culprit", payload.value("culprit").toString()}};
	}
} // namespace CrashPayload
