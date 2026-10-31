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

#pragma once
#include <QString>
#include <QByteArray>
#include <QList>
#include <QVector>
#include <katabasis/Bits.h>
#include <QJsonObject>

struct Skin {
	QString id;
	QString url;
	QString variant;

	QByteArray data;
};

struct Cape {
	QString id;
	QString url;
	QString alias;

	QByteArray data;
};

struct MinecraftEntitlement {
	bool ownsMinecraft = false;
	bool canPlayMinecraft = false;
	Katabasis::Validity validity = Katabasis::Validity::None;
};

struct MinecraftProfile {
	QString id;
	QString name;
	Skin skin;
	QString currentCape;

	// Capes the account owns, in the order the profile service returned them. A
	// list, not a map by id: the order is the cape picker's order, and a map
	// would replace it with arbitrary (UUID) order. The stored JSON is an array
	// either way.
	QList<Cape> capes;

	// Looks a cape up by id, or nullptr if the account does not own it.
	const Cape* capeById(const QString& id) const
	{
		for (const Cape& cape : capes) {
			if (cape.id == id) {
				return &cape;
			}
		}
		return nullptr;
	}

	Katabasis::Validity validity = Katabasis::Validity::None;
};

enum class AccountType { MSA, Offline };

enum class AccountState {
	Unchecked,
	Offline,
	Working,
	Online,
	Errored,
	Expired,
	Gone
};

struct AccountData {
	QJsonObject saveState() const;
	bool resumeStateFromV3(QJsonObject data);

	//! Gamertag for MSA
	QString accountDisplayString() const;

	//! Yggdrasil access token, as passed to the game.
	QString accessToken() const;

	QString profileId() const;
	QString profileName() const;

	QString lastError() const;

	AccountType type = AccountType::MSA;

	QString offlineUsername;

	Katabasis::Token msaToken;
	Katabasis::Token userToken;
	Katabasis::Token xboxApiToken;
	Katabasis::Token mojangservicesToken;

	Katabasis::Token yggdrasilToken;
	MinecraftProfile minecraftProfile;
	MinecraftEntitlement minecraftEntitlement;
	Katabasis::Validity validity_ = Katabasis::Validity::None;

	// runtime only information (not saved with the account)
	QString internalId;
	QString errorString;
	AccountState accountState = AccountState::Unchecked;
};
