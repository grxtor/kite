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

#pragma once

#include <QString>

#include "ModBridgeJson.h"

/*
 * Where the bridge gets the friends' data from. FriendsController is the
 * one source in the launcher (FriendsBridgeSource); the interface keeps
 * ModBridge testable over a real socket without one.
 */
class ModBridgeSource
{
  public:
	virtual ~ModBridgeSource() = default;

	/// The cache as it is now; never asks the friends server anything.
	virtual ModBridgeJson::FriendsView friendsView() = 0;
	/// Polls the friends server soon (the source coalesces).
	virtual void requestRefresh() = 0;
	/// The file of the cached face of friend @p uuid (with dashes); empty
	/// when there is none, or when @p uuid is not a friend.
	virtual QString facePath(const QString& uuid) = 0;
};
