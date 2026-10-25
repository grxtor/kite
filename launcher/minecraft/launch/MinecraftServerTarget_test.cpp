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

#include "minecraft/launch/MinecraftServerTarget.h"

class MinecraftServerTargetTest : public QObject
{
	Q_OBJECT

  private slots:
	void aNameOrIpv4NeedsNoBrackets()
	{
		QCOMPARE(MinecraftServerTarget::parse("mc.hypixel.net", false)
					 .quickPlayHost(),
				 QString("mc.hypixel.net"));
		QCOMPARE(MinecraftServerTarget::parse("192.168.1.10:25570", false)
					 .quickPlayHost(),
				 QString("192.168.1.10"));
	}

	void anIpv6AddressGoesInBrackets()
	{
		// --quickPlayMultiplayer takes one "host:port": without brackets the
		// game reads 2001:db8::5:25565 as a single host.
		const auto target = MinecraftServerTarget::parse("[2001:db8::5]", false);
		QCOMPARE(target.address, QString("2001:db8::5"));
		QCOMPARE(target.port, quint16(25565));
		QCOMPARE(target.quickPlayHost(), QString("[2001:db8::5]"));
		QCOMPARE(target.quickPlayHost() + ":" + QString::number(target.port),
				 QString("[2001:db8::5]:25565"));
	}

	void theBracketsOfAFriendsAddressSurviveAPortToo()
	{
		// What a friend's Join sends (Format.friendAddress) for a server on
		// a port of its own.
		const auto target =
			MinecraftServerTarget::parse("[2001:db8::5]:25570", false);
		QCOMPARE(target.address, QString("2001:db8::5"));
		QCOMPARE(target.port, quint16(25570));
		QCOMPARE(target.quickPlayHost(), QString("[2001:db8::5]"));
	}
};

QTEST_GUILESS_MAIN(MinecraftServerTargetTest)

#include "MinecraftServerTarget_test.moc"
