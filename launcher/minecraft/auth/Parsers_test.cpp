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

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

#include "minecraft/auth/Parsers.h"

// Parsers::redactTokens(): what the sign-in steps write to the log instead
// of a raw response.
class ParsersTest : public QObject
{
	Q_OBJECT

  private slots:
	// An XSTS reply: the token goes, the claims that help debugging stay.
	void xboxTokenIsRedacted()
	{
		const QByteArray reply =
			R"({"IssueInstant":"2026-10-17T11:08:23Z","Token":"secret-xsts",)"
			R"("DisplayClaims":{"xui":[{"uhs":"1234"}]}})";
		const QByteArray logged = Parsers::redactTokens(reply);
		QVERIFY(!logged.contains("secret-xsts"));
		QVERIFY(logged.contains("1234"));
		QVERIFY(logged.contains("IssueInstant"));
	}

	// login_with_xbox: the Minecraft access token goes; the rest stays.
	void minecraftAccessTokenIsRedacted()
	{
		const QByteArray reply =
			R"({"username":"5b5de63f","access_token":"eyJsecret",)"
			R"("token_type":"Bearer","expires_in":86400})";
		const QByteArray logged = Parsers::redactTokens(reply);
		QVERIFY(!logged.contains("eyJsecret"));
		QVERIFY(logged.contains("Bearer"));
		QVERIFY(logged.contains("86400"));
		const QJsonObject object = QJsonDocument::fromJson(logged).object();
		QVERIFY(object.contains("access_token"));
	}

	// Keys are matched without regard to case, at any depth, in arrays too.
	void nestedAndAnyCase()
	{
		const QByteArray reply =
			R"({"outer":[{"Refresh_Token":"r-secret"},{"id_token":"i-secret"}],)"
			R"("identityToken":"XBL3.0 x=1;secret"})";
		const QByteArray logged = Parsers::redactTokens(reply);
		QVERIFY(!logged.contains("r-secret"));
		QVERIFY(!logged.contains("i-secret"));
		QVERIFY(!logged.contains("XBL3.0"));
	}

	// Not JSON (an error page): logged as it came, cut short.
	void notJsonIsShortened()
	{
		const QByteArray page = QByteArray("<html>") + QByteArray(1000, 'x');
		const QByteArray logged = Parsers::redactTokens(page);
		QVERIFY(logged.startsWith("<html>"));
		QVERIFY(logged.size() < page.size());
	}
};

QTEST_GUILESS_MAIN(ParsersTest)

#include "Parsers_test.moc"
