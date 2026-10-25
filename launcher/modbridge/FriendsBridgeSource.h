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

#include <QPointer>

#include "ModBridgeSource.h"

class FriendsController;

/// The bridge's view of the launcher's FriendsController. Without one (the
/// QML shell never made it, or it is gone) the answers are Friends off.
class FriendsBridgeSource : public ModBridgeSource
{
  public:
	explicit FriendsBridgeSource(FriendsController* controller);

	ModBridgeJson::FriendsView friendsView() override;
	void requestRefresh() override;
	QString facePath(const QString& uuid) override;

  private:
	QPointer<FriendsController> m_controller;
};
