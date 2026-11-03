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

#include "ServerStatus.h"

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>

#include "theme/Contrast.h"

namespace
{
	const QChar kSection(0x00A7);
	const QString kFaviconPrefix = QStringLiteral("data:image/png;base64,");
	/// Text components nest; a hostile server must not run the stack out.
	constexpr int kMaxDepth = 32;
	/// §x§r§r§g§g§b§b, the Spigot spelling of a hex colour.
	constexpr int kHexDigits = 6;
	constexpr int kHexCodeLength = 2 + kHexDigits * 2;
	/// Colours are lifted or darkened until they reach this against the
	/// surface they sit on (WCAG's large-text minimum).
	constexpr qreal kMinContrast = 3.0;
	constexpr qreal kLightnessStep = 0.02;
	/// Stand-ins for the themes' card surfaces.
	const QColor kDarkSurface(0x1C, 0x1C, 0x1F);
	const QColor kLightSurface(0xFC, 0xFC, 0xFD);

	struct NamedColor {
		QChar code;
		const char* name;
		QRgb rgb;
	};
	/// The game's sixteen colours, in § code order.
	const NamedColor kColors[] = {
		{'0', "black", 0x000000},		 {'1', "dark_blue", 0x0000AA},
		{'2', "dark_green", 0x00AA00},	 {'3', "dark_aqua", 0x00AAAA},
		{'4', "dark_red", 0xAA0000},	 {'5', "dark_purple", 0xAA00AA},
		{'6', "gold", 0xFFAA00},		 {'7', "gray", 0xAAAAAA},
		{'8', "dark_gray", 0x555555},	 {'9', "blue", 0x5555FF},
		{'a', "green", 0x55FF55},		 {'b', "aqua", 0x55FFFF},
		{'c', "red", 0xFF5555},			 {'d', "light_purple", 0xFF55FF},
		{'e', "yellow", 0xFFFF55},		 {'f', "white", 0xFFFFFF},
	};

	struct Style {
		/// Invalid: the text's own colour.
		QColor color;
		bool bold = false;
		bool italic = false;
		bool underlined = false;
		bool strikethrough = false;
		bool obfuscated = false;
	};

	struct Run {
		QString text;
		Style style;
	};
	using Line = QList<Run>;

	QColor colorOfCode(QChar code)
	{
		for (const auto& color : kColors) {
			if (color.code == code) {
				return QColor(color.rgb);
			}
		}
		return {};
	}

	QColor colorOfName(const QString& value)
	{
		for (const auto& color : kColors) {
			if (value == QLatin1String(color.name)) {
				return QColor(color.rgb);
			}
		}
		const bool hex = value.size() == kHexDigits + 1 &&
						 value.startsWith(QLatin1Char('#'));
		return hex && QColor::isValidColorName(value) ? QColor(value) : QColor();
	}

	bool isHexDigit(QChar c)
	{
		return c.isDigit() || (c.toLower() >= QLatin1Char('a') &&
							   c.toLower() <= QLatin1Char('f'));
	}

	/// The colour of a §x§r§r§g§g§b§b starting at @p at, or invalid.
	QColor hexCodeAt(const QString& text, int at)
	{
		if (at + kHexCodeLength > text.size()) {
			return {};
		}
		QString digits = QStringLiteral("#");
		for (int i = at + 2; i < at + kHexCodeLength; i += 2) {
			if (text.at(i) != kSection || !isHexDigit(text.at(i + 1))) {
				return {};
			}
			digits += text.at(i + 1);
		}
		return QColor(digits);
	}

	/// @p text with its § codes applied over @p base, appended to @p runs.
	/// A colour code clears the formatting before it, as in the game.
	void appendLegacy(const QString& text, const Style& base, QList<Run>& runs)
	{
		Style style = base;
		QString current;
		const auto flush = [&] {
			if (!current.isEmpty()) {
				runs.append({current, style});
				current.clear();
			}
		};
		for (int i = 0; i < text.size(); ++i) {
			if (text.at(i) != kSection) {
				current += text.at(i);
				continue;
			}
			flush();
			if (i + 1 >= text.size()) {
				break;
			}
			const QChar code = text.at(i + 1).toLower();
			const QColor named = colorOfCode(code);
			const QColor hex =
				code == QLatin1Char('x') ? hexCodeAt(text, i) : QColor();
			if (named.isValid() || hex.isValid()) {
				style = Style();
				style.color = named.isValid() ? named : hex;
				i += hex.isValid() ? kHexCodeLength - 1 : 1;
				continue;
			}
			switch (code.unicode()) {
				case 'k': style.obfuscated = true; break;
				case 'l': style.bold = true; break;
				case 'm': style.strikethrough = true; break;
				case 'n': style.underlined = true; break;
				case 'o': style.italic = true; break;
				case 'r': style = base; break;
				default: break;
			}
			++i;
		}
		flush();
	}

	Style styled(const QJsonObject& component, const Style& parent)
	{
		Style style = parent;
		const QColor color = colorOfName(component.value("color").toString());
		if (color.isValid()) {
			style.color = color;
		}
		const auto flag = [&component](const char* key, bool& value) {
			const QJsonValue set = component.value(QLatin1String(key));
			if (set.isBool()) {
				value = set.toBool();
			}
		};
		flag("bold", style.bold);
		flag("italic", style.italic);
		flag("underlined", style.underlined);
		flag("strikethrough", style.strikethrough);
		flag("obfuscated", style.obfuscated);
		return style;
	}

	QString scalarText(const QJsonValue& value)
	{
		if (value.isString()) {
			return value.toString();
		}
		if (value.isDouble()) {
			return QString::number(value.toDouble());
		}
		return QString();
	}

	void collect(const QJsonValue& value, const Style& parent,
				 QList<Run>& runs, int depth)
	{
		if (depth > kMaxDepth) {
			return;
		}
		if (value.isArray()) {
			/* [a, b, c] is a with b and c as its extra. */
			const QJsonArray parts = value.toArray();
			if (parts.isEmpty()) {
				return;
			}
			const QJsonValue first = parts.first();
			const Style inherited =
				first.isObject() ? styled(first.toObject(), parent) : parent;
			collect(first, parent, runs, depth + 1);
			for (int i = 1; i < parts.size(); ++i) {
				collect(parts.at(i), inherited, runs, depth + 1);
			}
			return;
		}
		if (!value.isObject()) {
			appendLegacy(scalarText(value), parent, runs);
			return;
		}
		const QJsonObject component = value.toObject();
		const Style style = styled(component, parent);
		QString text = scalarText(component.value("text"));
		if (text.isEmpty() && component.contains("translate")) {
			text = component.value("fallback").toString();
		}
		appendLegacy(text, style, runs);
		for (const QJsonValue& extra : component.value("extra").toArray()) {
			collect(extra, style, runs, depth + 1);
		}
	}

	/// The runs split into lines; obfuscated text is left out.
	QList<Line> linesOf(const QList<Run>& runs)
	{
		QList<Line> lines(1);
		for (const Run& run : runs) {
			if (run.style.obfuscated) {
				continue;
			}
			QString text = run.text;
			text.remove(QLatin1Char('\r'));
			const QStringList parts = text.split(QLatin1Char('\n'));
			for (int i = 0; i < parts.size(); ++i) {
				if (i > 0) {
					lines.append(Line());
				}
				if (!parts.at(i).isEmpty()) {
					lines.last().append({parts.at(i), run.style});
				}
			}
		}
		return lines;
	}

	/// @p line without the spaces servers pad it with to centre it in game.
	Line trimmed(Line line)
	{
		while (!line.isEmpty()) {
			Run& first = line.first();
			int lead = 0;
			while (lead < first.text.size() && first.text.at(lead).isSpace()) {
				++lead;
			}
			first.text.remove(0, lead);
			if (!first.text.isEmpty()) {
				break;
			}
			line.removeFirst();
		}
		while (!line.isEmpty()) {
			Run& last = line.last();
			int end = last.text.size();
			while (end > 0 && last.text.at(end - 1).isSpace()) {
				--end;
			}
			last.text.truncate(end);
			if (!last.text.isEmpty()) {
				break;
			}
			line.removeLast();
		}
		return line;
	}

	/// @p color lightened or darkened until it reads on @p surface.
	QColor readableOn(QColor color, const QColor& surface)
	{
		const bool darken = surface.lightnessF() > 0.5;
		const QColor hsl = color.toHsl();
		const qreal hue = qMax<qreal>(0, hsl.hslHueF());
		qreal lightness = hsl.lightnessF();
		const qreal step = darken ? -kLightnessStep : kLightnessStep;
		while (Contrast::ratio(color, surface) < kMinContrast &&
			   (darken ? lightness > 0 : lightness < 1)) {
			lightness = qBound<qreal>(0, lightness + step, 1);
			color = QColor::fromHslF(hue, hsl.hslSaturationF(), lightness);
		}
		return color;
	}

	QString html(const QList<Line>& lines, const QColor& surface)
	{
		QStringList out;
		for (const Line& line : lines) {
			QString markup;
			for (const Run& run : line) {
				QString text = run.text.toHtmlEscaped();
				if (run.style.strikethrough) {
					text = QStringLiteral("<s>%1</s>").arg(text);
				}
				if (run.style.underlined) {
					text = QStringLiteral("<u>%1</u>").arg(text);
				}
				if (run.style.italic) {
					text = QStringLiteral("<i>%1</i>").arg(text);
				}
				if (run.style.bold) {
					text = QStringLiteral("<b>%1</b>").arg(text);
				}
				if (run.style.color.isValid()) {
					text = QStringLiteral("<font color=\"%1\">%2</font>")
							   .arg(readableOn(run.style.color, surface)
										.name(QColor::HexRgb),
									text);
				}
				markup += text;
			}
			out.append(markup);
		}
		return out.join(QStringLiteral("<br>"));
	}
} // namespace

namespace ServerStatus
{
	Motd formatMotd(const QJsonValue& description)
	{
		QList<Run> runs;
		collect(description, Style(), runs, 0);
		QList<Line> shown;
		for (const Line& line : linesOf(runs)) {
			const Line kept = trimmed(line);
			if (!kept.isEmpty() && shown.size() < kMotdLines) {
				shown.append(kept);
			}
		}
		QStringList plain;
		for (const Line& line : shown) {
			QString text;
			for (const Run& run : line) {
				text += run.text;
			}
			plain.append(text);
		}
		return {html(shown, kDarkSurface), html(shown, kLightSurface),
				plain.join(QLatin1Char('\n'))};
	}

	QString stripCodes(const QString& text)
	{
		QString out;
		for (int i = 0; i < text.size(); ++i) {
			if (text.at(i) == kSection) {
				++i;
				continue;
			}
			out += text.at(i);
		}
		return out;
	}

	bool parse(const QByteArray& json, Status* status, QString* error)
	{
		QJsonParseError parseError;
		const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
		if (parseError.error != QJsonParseError::NoError ||
			!document.isObject()) {
			if (error) {
				*error = parseError.error != QJsonParseError::NoError
							 ? parseError.errorString()
							 : QStringLiteral("The status is not an object.");
			}
			return false;
		}
		const QJsonObject root = document.object();
		Status read;
		const QJsonObject version = root.value("version").toObject();
		read.version = stripCodes(version.value("name").toString()).trimmed();
		read.protocol = version.value("protocol").toInt(-1);
		const QJsonObject players = root.value("players").toObject();
		read.online = qMax(-1, players.value("online").toInt(-1));
		read.max = qMax(-1, players.value("max").toInt(-1));
		const Motd motd = formatMotd(root.value("description"));
		read.motd = motd.dark;
		read.motdLight = motd.light;
		read.motdText = motd.text;
		/* Old servers wrap the base64 in lines. */
		QString favicon = root.value("favicon").toString();
		favicon.remove(QLatin1Char('\n')).remove(QLatin1Char('\r'));
		if (favicon.startsWith(kFaviconPrefix) &&
			favicon.size() > kFaviconPrefix.size()) {
			read.favicon = favicon;
		}
		if (status) {
			*status = read;
		}
		return true;
	}
} // namespace ServerStatus
