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

#include <QTest>

#include "FriendsLogRules.h"

using FriendsApi::World;
using FriendsLogRules::Change;

namespace
{
	Change change(const char* line)
	{
		return FriendsLogRules::fromLine(QString::fromUtf8(line));
	}
} // namespace

class FriendsLogRulesTest : public QObject
{
	Q_OBJECT

  private slots:
	// ---- Connecting to a server

	void aServerOn189()
	{
		const auto c = change("[12:00:00] [Client thread/INFO]: Connecting to "
							  "mc.hypixel.net, 25565");
		QCOMPARE(c.kind, Change::Kind::Set);
		QCOMPARE(c.world.type, World::Type::Server);
		QCOMPARE(c.world.host, QString("mc.hypixel.net"));
		QCOMPARE(c.world.port, 25565);
	}

	void aServerOn121()
	{
		const auto c = change(
			"[19:40:00] [Render thread/INFO]: Connecting to play.example.org, "
			"25570");
		QCOMPARE(c.kind, Change::Kind::Set);
		QCOMPARE(c.world.host, QString("play.example.org"));
		QCOMPARE(c.world.port, 25570);
	}

	void aServerOn26()
	{
		const auto c = change("[09:44:12] [Render thread/INFO]: Connecting to "
							  "eu.mineplex.com, 25565");
		QCOMPARE(c.kind, Change::Kind::Set);
		QCOMPARE(c.world.host, QString("eu.mineplex.com"));
	}

	void aModLoaderLineWithADateAndALoggerName()
	{
		const auto c = change(
			"[12Sep2026 19:40:00.544] [Render thread/INFO] "
			"[net.minecraft.client.gui.screens.ConnectScreen/]: Connecting to "
			"atm10aero.moddedmc.net, 25565");
		QCOMPARE(c.kind, Change::Kind::Set);
		QCOMPARE(c.world.host, QString("atm10aero.moddedmc.net"));
	}

	void theHostIsCleanedUp()
	{
		/* A trailing dot, capitals, and a Forge handshake marker. */
		QCOMPARE(change("[12:00:00] [Render thread/INFO]: Connecting to "
						"Play.Example.COM., 25565")
					 .world.host,
				 QString("play.example.com"));
		const QString marked =
			QString("[12:00:00] [Render thread/INFO]: Connecting to "
					"play.example.com") +
			QChar(0) + "FML" + QChar(0) + ", 25565";
		QCOMPARE(FriendsLogRules::fromLine(marked).world.host,
				 QString("play.example.com"));
	}

	void anIpAddressIsAHost()
	{
		const auto c = change(
			"[12:00:00] [Render thread/INFO]: Connecting to 203.0.113.7, 25565");
		QCOMPARE(c.kind, Change::Kind::Set);
		QCOMPARE(c.world.host, QString("203.0.113.7"));
	}

	void anIpv6AddressIsAHost()
	{
		const auto c = change(
			"[12:00:00] [Render thread/INFO]: Connecting to 2001:db8::1, 25565");
		QCOMPARE(c.kind, Change::Kind::Set);
		QCOMPARE(c.world.host, QString("2001:db8::1"));
	}

	void anOddPortIsDropped()
	{
		const auto c = change(
			"[12:00:00] [Render thread/INFO]: Connecting to a.example.com, "
			"99999");
		QCOMPARE(c.kind, Change::Kind::Set);
		QCOMPARE(c.world.port, 0);
	}

	void somethingThatIsNotAHostClearsTheWorld()
	{
		for (const char* line :
			 {"[12:00:00] [Render thread/INFO]: Connecting to bad host!, 25565",
			  "[12:00:00] [Render thread/INFO]: Connecting to 2130706433, 25565",
			  "[12:00:00] [Render thread/INFO]: Connecting to 0x7f.0x1, 25565",
			  "[12:00:00] [Render thread/INFO]: Connecting to a_b.example.com, "
			  "25565"}) {
			QCOMPARE(change(line).kind, Change::Kind::Clear);
		}
	}

	// ---- The integrated server

	void singleplayerOn189()
	{
		const auto c = change("[12:00:00] [Server thread/INFO]: Starting "
							  "integrated minecraft server version 1.8.9");
		QCOMPARE(c.kind, Change::Kind::Set);
		QCOMPARE(c.world.type, World::Type::Singleplayer);
	}

	void singleplayerOn121()
	{
		const auto c = change("[12:00:00] [Server thread/INFO]: Starting "
							  "minecraft server version 1.21.4");
		QCOMPARE(c.kind, Change::Kind::Set);
		QCOMPARE(c.world.type, World::Type::Singleplayer);
	}

	void singleplayerOn26()
	{
		const auto c = change("[09:42:00] [Server thread/INFO]: Starting "
							  "integrated minecraft server version 26.1.2");
		QCOMPARE(c.kind, Change::Kind::Set);
		QCOMPARE(c.world.type, World::Type::Singleplayer);
	}

	void singleplayerWithALoaderLoggerName()
	{
		const auto c = change(
			"[22Sep2026 17:34:08.851] [Server thread/INFO] "
			"[net.minecraft.client.server.IntegratedServer/]: Starting "
			"integrated minecraft server version 1.20.1");
		QCOMPARE(c.kind, Change::Kind::Set);
		QCOMPARE(c.world.type, World::Type::Singleplayer);
	}

	void leavingASingleplayerWorld()
	{
		const auto c = change(
			"[09:42:06] [Server thread/INFO]: Stopping server");
		QCOMPARE(c.kind, Change::Kind::LeftSingleplayer);
		World inWorld;
		inWorld.type = World::Type::Singleplayer;
		QCOMPARE(FriendsLogRules::apply(c, inWorld).type, World::Type::None);
		/* Not news for someone who is on a server. */
		World onServer;
		onServer.type = World::Type::Server;
		onServer.host = "mc.example.com";
		QCOMPARE(FriendsLogRules::apply(c, onServer), onServer);
	}

	// ---- What must not count

	void otherConnectingLinesAreNotServers()
	{
		for (const char* line :
			 {"[22Sep2026 17:35:23.875] [Render thread/INFO] [voicechat/]: "
			  "[voicechat] Connecting to voice chat server: '10.0.0.2:62985'",
			  "[12Sep2026 19:39:34.719] [Thread-24/INFO] "
			  "[com.hypherionmc.simplerpc.rpcsdk.DiscordRpc/]: Connecting to "
			  "Discord (Attempt 0/10)"}) {
			QCOMPARE(change(line).kind, Change::Kind::None);
		}
	}

	void aChatMessageCannotSetTheWorld()
	{
		QCOMPARE(change("[12:00:00] [Render thread/INFO]: [CHAT] <Griefer> "
						"Connecting to evil.example.com, 25565")
					 .kind,
				 Change::Kind::None);
		QCOMPARE(change("Connecting to evil.example.com, 25565").kind,
				 Change::Kind::None);
	}

	void aWarningIsNotTheGame()
	{
		QCOMPARE(change("[12:00:00] [Render thread/WARN]: Connecting to "
						"evil.example.com, 25565")
					 .kind,
				 Change::Kind::None);
	}

	void theIntegratedServerLineNeedsTheServerThread()
	{
		QCOMPARE(change("[12:00:00] [Render thread/INFO]: Starting integrated "
						"minecraft server version 1.21.4")
					 .kind,
				 Change::Kind::None);
		QCOMPARE(change("[12:00:00] [Render thread/INFO]: Stopping server")
					 .kind,
				 Change::Kind::None);
	}

	void ordinaryLinesSayNothing()
	{
		for (const char* line :
			 {"", "[12:00:00] [Render thread/INFO]: Setting user: Steve",
			  "[12:00:00] [Render thread/INFO]: Narrator library for x86_64 "
			  "successfully loaded",
			  "Kite: Launching Minecraft"}) {
			QCOMPARE(change(line).kind, Change::Kind::None);
		}
	}

	void noChangeKeepsTheWorld()
	{
		World world;
		world.type = World::Type::Server;
		world.host = "mc.example.com";
		QCOMPARE(FriendsLogRules::apply(Change(), world), world);
	}

	void aNewConnectReplacesTheWorld()
	{
		World world;
		world.type = World::Type::Singleplayer;
		const auto c = change(
			"[12:00:00] [Render thread/INFO]: Connecting to b.example.com, "
			"25565");
		QCOMPARE(FriendsLogRules::apply(c, world).host,
				 QString("b.example.com"));
	}
};

QTEST_GUILESS_MAIN(FriendsLogRulesTest)

#include "FriendsLogRules_test.moc"
