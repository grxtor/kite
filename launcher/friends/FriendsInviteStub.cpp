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

/*
 * Where the socket's invite and invite.cancelled frames go. The invites
 * themselves (their list, Join and Dismiss) are another change's; until it
 * lands this does nothing and the next poll is all an invite gets. That change
 * replaces this file.
 */

#include "FriendsController.h"

void FriendsController::applyInviteEvent(const QJsonObject& event)
{
	Q_UNUSED(event);
}
