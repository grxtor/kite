/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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
#include <QMultiMap>
#include <memory>
#include "QObjectPtr.h"

class MinecraftAccount;
class QNetworkAccessManager;

struct AuthSession {
	bool MakeOffline(QString offline_playername);
	void MakeDemo();

	QString serializeUserProperties();

	enum Status {
		Undetermined,
		RequiresOAuth,
		RequiresPassword,
		RequiresProfileSetup,
		PlayableOffline,
		PlayableOnline,
		GoneOrMigrated
	} status = Undetermined;

	// client token
	QString client_token;
	// account user name
	QString username;
	// combined session ID
	QString session;
	// volatile auth token
	QString access_token;
	// profile name
	QString player_name;
	// profile ID
	QString uuid;
	// 'legacy' or 'mojang', depending on account type
	QString user_type;
	// Did the auth server reply?
	bool auth_server_online = false;
	// Did the user request online mode?
	bool wants_online = true;

	// Whether the game's own files (version data, libraries, assets) may be
	// downloaded for this launch. An offline account is never "online" above
	// but still needs the game; MinecraftAccount::fillSession() sets this from
	// the user's request and the network. Signing-in accounts decide through
	// `status` (PlayableOnline updates, PlayableOffline does not).
	bool fetch_game_files = false;

	// Is this a demo session?
	bool demo = false;

	/*
	 * Optional user properties payload, filled by plugins through
	 * MMCO_HOOK_SESSION_FILL: a JSON object literal (e.g.
	 * `{"textures":["..."]}`) or empty. Consumed by serializeUserProperties()
	 * for the JVM command line; authlib-injector and Yggdrasil-compatible
	 * servers use it for signed cape/skin texture URLs.
	 */
	QString user_properties;
};

typedef std::shared_ptr<AuthSession> AuthSessionPtr;
