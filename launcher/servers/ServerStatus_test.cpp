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

#include <QColor>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTest>

#include "servers/ServerStatus.h"
#include "theme/Contrast.h"

namespace
{
	ServerStatus::Motd motdOf(const QByteArray& json)
	{
		return ServerStatus::formatMotd(
			QJsonDocument::fromJson("[" + json + "]").array().first());
	}

	/// The colours of the <font> tags in @p html, in order.
	QStringList colorsIn(const QString& html)
	{
		QStringList colors;
		static const QRegularExpression font(QStringLiteral("color=\"(#[0-9a-f]{6})\""));
		auto it = font.globalMatch(html);
		while (it.hasNext()) {
			colors.append(it.next().captured(1));
		}
		return colors;
	}
} // namespace

class ServerStatusTest : public QObject
{
	Q_OBJECT

  private slots:
	void readsAFullStatus()
	{
		const QByteArray json = R"({
			"version": { "name": "§cPaper 1.21.4", "protocol": 769 },
			"players": { "max": 200000, "online": 26187, "sample": [] },
			"description": "§aHypixel Network",
			"favicon": "data:image/png;base64,iVBO\nRw0K"
		})";
		ServerStatus::Status status;
		QString error;
		QVERIFY2(ServerStatus::parse(json, &status, &error), qPrintable(error));
		QCOMPARE(status.version, QString("Paper 1.21.4"));
		QCOMPARE(status.protocol, 769);
		QCOMPARE(status.online, 26187);
		QCOMPARE(status.max, 200000);
		QCOMPARE(status.motdText, QString("Hypixel Network"));
		QCOMPARE(status.motd, QString("<font color=\"#55ff55\">Hypixel Network</font>"));
		// Old servers wrap the base64 in lines.
		QCOMPARE(status.favicon, QString("data:image/png;base64,iVBORw0K"));
		QCOMPARE(status.latencyMs, -1);
	}

	void aServerThatHidesItsPlayersSaysSo()
	{
		ServerStatus::Status status;
		QVERIFY(ServerStatus::parse(R"({"description": {"text": "Hi"}})", &status));
		QCOMPARE(status.online, -1);
		QCOMPARE(status.max, -1);
		QCOMPARE(status.motdText, QString("Hi"));
		QVERIFY(status.favicon.isEmpty());
	}

	void onlyAPngDataUrlIsAFavicon()
	{
		ServerStatus::Status status;
		QVERIFY(ServerStatus::parse(R"({"favicon": "data:image/gif;base64,R0lGODlh"})", &status));
		QVERIFY(status.favicon.isEmpty());
		QVERIFY(ServerStatus::parse(R"({"favicon": "data:image/png;base64,"})", &status));
		QVERIFY(status.favicon.isEmpty());
	}

	void refusesWhatIsNotAnObject()
	{
		ServerStatus::Status status;
		status.online = 7;
		QString error;
		QVERIFY(!ServerStatus::parse("not json", &status, &error));
		QVERIFY(!error.isEmpty());
		QVERIFY(!ServerStatus::parse("[1, 2]", &status, &error));
		// Left as it was.
		QCOMPARE(status.online, 7);
	}

	void legacyCodesColourAndFormat()
	{
		const auto motd = motdOf("\"§aHello §lWorld§r!\\n§cSecond\"");
		QCOMPARE(motd.text, QString("Hello World!\nSecond"));
		QCOMPARE(motd.dark,
				 QString("<font color=\"#55ff55\">Hello </font>"
						 "<font color=\"#55ff55\"><b>World</b></font>!"
						 "<br><font color=\"#ff5555\">Second</font>"));
	}

	void aColourCodeClearsTheFormattingBeforeIt()
	{
		const auto motd = motdOf(R"("§l§nBold§eYellow")");
		QCOMPARE(motd.dark, QString("<b><u>Bold</u></b><font color=\"#ffff55\">Yellow</font>"));
	}

	void spigotHexCodesAreColours()
	{
		const auto motd = motdOf(R"("§x§1§2§a§b§c§dHex")");
		QCOMPARE(motd.text, QString("Hex"));
		QCOMPARE(colorsIn(motd.dark).size(), 1);
		QVERIFY(!motd.dark.contains(QChar(0x00A7)));
	}

	void componentsInheritTheirParentsStyle()
	{
		const auto motd = motdOf("{ \"text\": \"\", \"color\": \"gold\", \"extra\": ["
								 "{ \"text\": \"Big\", \"bold\": true }, \" plain\","
								 "{ \"text\": \"#\", \"color\": \"#3AA9FF\", \"underlined\": true,"
								 "  \"strikethrough\": true, \"italic\": true } ] }");
		QCOMPARE(motd.text, QString("Big plain#"));
		QCOMPARE(motd.dark,
				 QString("<font color=\"#ffaa00\"><b>Big</b></font>"
						 "<font color=\"#ffaa00\"> plain</font>"
						 "<font color=\"#3aa9ff\"><i><u><s>#</s></u></i></font>"));
	}

	void anArraysFirstPartStylesTheRest()
	{
		const auto motd = motdOf(R"([{ "text": "A", "color": "red" }, "B", { "text": "C", "color": "aqua" }])");
		QCOMPARE(motd.text, QString("ABC"));
		QCOMPARE(colorsIn(motd.dark), (QStringList{"#ff5555", "#ff5555", "#55ffff"}));
	}

	void obfuscatedTextIsLeftOut()
	{
		const auto motd = motdOf(R"({ "extra": [ { "text": "/ ", "obfuscated": true }, "Mine", "§kxx§r berry" ] })");
		QCOMPARE(motd.text, QString("Mine berry"));
	}

	void translatedPartsUseTheirFallback()
	{
		const auto motd = motdOf(R"({ "extra": [ { "translate": "kite.unknown", "fallback": "Welcome" }, { "translate": "x" } ] })");
		QCOMPARE(motd.text, QString("Welcome"));
	}

	void markupIsEscaped()
	{
		const auto motd = motdOf("\"<img src=\\\"evil.png\\\"> & <b>\"");
		QCOMPARE(motd.text, QString("<img src=\"evil.png\"> & <b>"));
		QVERIFY(!motd.dark.contains("<img"));
		QVERIFY(!motd.dark.contains("<b>"));
		QVERIFY(motd.dark.contains("&lt;img"));
		QVERIFY(motd.dark.contains("&amp;"));
	}

	void twoLinesAtMostEachTrimmed()
	{
		const auto motd = motdOf("{ \"text\": \"     §aCentred   \\n\\n   Second  \\nThird\" }");
		QCOMPARE(motd.text, QString("Centred\nSecond"));
		QCOMPARE(motd.dark.count("<br>"), 1);
	}

	void coloursAreMadeReadableOnTheirSurface()
	{
		const auto motd = motdOf(R"("§0Black §fWhite §6Gold")");
		const QStringList dark = colorsIn(motd.dark);
		const QStringList light = colorsIn(motd.light);
		QCOMPARE(dark.size(), 3);
		QCOMPARE(light.size(), 3);
		// Black is lifted on a dark card; white and gold stay as the server set them.
		QVERIFY(dark.at(0) != "#000000");
		QVERIFY(Contrast::ratio(QColor(dark.at(0)), QColor(0x1C, 0x1C, 0x1F)) >= 3.0);
		QCOMPARE(dark.at(1), QString("#ffffff"));
		QCOMPARE(dark.at(2), QString("#ffaa00"));
		// On a light card white and gold are darkened, black stays.
		QCOMPARE(light.at(0), QString("#000000"));
		QVERIFY(Contrast::ratio(QColor(light.at(1)), QColor(0xFC, 0xFC, 0xFD)) >= 3.0);
		QVERIFY(Contrast::ratio(QColor(light.at(2)), QColor(0xFC, 0xFC, 0xFD)) >= 3.0);
	}

	void deepNestingStopsInsteadOfOverflowing()
	{
		QByteArray json = R"("deep")";
		for (int i = 0; i < 200; ++i) {
			json = R"({"text": "", "extra": [)" + json + "]}";
		}
		const auto motd = motdOf(json);
		QVERIFY(motd.text.isEmpty());
	}

	void stripCodesTakesOutEveryCode()
	{
		QCOMPARE(ServerStatus::stripCodes(QString::fromUtf8("§cMain§ltenance§")),
				 QString("Maintenance"));
	}
};

QTEST_GUILESS_MAIN(ServerStatusTest)

#include "ServerStatus_test.moc"
