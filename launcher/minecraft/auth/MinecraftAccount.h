/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include <QObject>
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QPair>
#include <QMap>
#include <QNetworkInformation>
#include <QPixmap>

#include <memory>

#include "AuthSession.h"
#include "Usable.h"
#include "AccountData.h"
#include "QObjectPtr.h"

class Task;
class AccountTask;
class MinecraftAccount;

typedef shared_qobject_ptr<MinecraftAccount> MinecraftAccountPtr;
Q_DECLARE_METATYPE(MinecraftAccountPtr)

/**
 * A profile within someone's Mojang account.
 *
 * Currently, the profile system has not been implemented by Mojang yet,
 * but we might as well add some things for it in MeshMC right now so
 * we don't have to rip the code to pieces to add it later.
 */
struct AccountProfile {
	QString id;
	QString name;
	bool legacy;
};

/**
 * Object that stores information about a certain Mojang account.
 *
 * Said information may include things such as that account's username, client
 * token, and access token if the user chose to stay logged in.
 */
class MinecraftAccount : public QObject, public Usable
{
	Q_OBJECT
  public: /* construction */
	//! Do not copy accounts. ever.
	explicit MinecraftAccount(const MinecraftAccount& other,
							  QObject* parent) = delete;

	//! Default constructor
	explicit MinecraftAccount(QObject* parent = 0);

	static MinecraftAccountPtr createBlankMSA();

	static MinecraftAccountPtr createOffline(const QString& username);

	static MinecraftAccountPtr loadFromJsonV3(const QJsonObject& json);

	//! Saves a MinecraftAccount to a JSON object and returns it.
	QJsonObject saveToJson() const;

  public: /* manipulation */
	shared_qobject_ptr<AccountTask> loginMSA();

	shared_qobject_ptr<AccountTask> refresh();

	shared_qobject_ptr<AccountTask> currentTask();

  public: /* queries */
	QString internalId() const
	{
		return data.internalId;
	}

	QString accountDisplayString() const
	{
		return data.accountDisplayString();
	}

	QString accessToken() const
	{
		return data.accessToken();
	}

	QString profileId() const
	{
		return data.profileId();
	}

	QString profileName() const
	{
		return data.profileName();
	}

	/*!
	 * The UUID the game is launched with (--uuid, AuthSession::uuid): the
	 * Microsoft profile's id, or for an offline account uuidFromUsername()
	 * of its name -- the same every launch, so the player's advancements,
	 * statistics and pets in a world stay theirs, and the same id an
	 * offline-mode server gives that name. Not internalId(): that is the
	 * account list's own bookkeeping and is made up afresh every run.
	 */
	QString gameUuid() const;

	/*!
	 * Java's UUID.nameUUIDFromBytes("OfflinePlayer:" + @p username) -- the
	 * version 3 UUID vanilla gives an offline player -- as 32 hex digits
	 * without dashes, the form profile ids take.
	 */
	static QString uuidFromUsername(const QString& username);

	bool isActive() const;

	bool isMSA() const
	{
		return data.type == AccountType::MSA;
	}

	bool isOffline() const
	{
		return data.type == AccountType::Offline;
	}

	bool ownsMinecraft() const
	{
		if (data.type == AccountType::Offline)
			return true;
		return data.minecraftEntitlement.ownsMinecraft;
	}

	bool hasProfile() const
	{
		if (data.type == AccountType::Offline)
			return true;
		return data.profileId().size() != 0;
	}

	QString typeString() const
	{
		if (data.type == AccountType::Offline)
			return "offline";
		return "msa";
	}

	QPixmap getFace() const;

	//! Returns the current state of the account
	AccountState accountState() const;

	AccountData* accountData()
	{
		return &data;
	}

	bool shouldRefresh() const;

	void fillSession(AuthSessionPtr session);

	/// Whether an offline account's launch may download the game's files:
	/// the user asked for an online launch and the network is not known to
	/// be down. See AuthSession::fetch_game_files.
	static bool shouldFetchGameFiles(
		bool wantsOnline, QNetworkInformation::Reachability reachability);
	/// The platform's current view of the network (Unknown where it has none).
	static QNetworkInformation::Reachability networkReachability();

	QString lastError() const
	{
		return data.lastError();
	}

  signals:
	/**
	 * This signal is emitted when the account changes
	 */
	void changed();

	void activityChanged(bool active);

	// TODO: better signalling for the various possible state changes -
	// especially errors

  protected: /* variables */
	AccountData data;

	// current task we are executing here
	shared_qobject_ptr<AccountTask> m_currentTask;

  protected: /* methods */
	void incrementUses() override;
	void decrementUses() override;

  private slots:
	void authSucceeded();
	void authFailed(QString reason);
};
