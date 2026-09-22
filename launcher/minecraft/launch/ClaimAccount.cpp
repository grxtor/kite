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

#include "ClaimAccount.h"
#include <launch/LaunchTask.h>

#include "core/LauncherContext.h"
#include "minecraft/auth/AccountList.h"

ClaimAccount::ClaimAccount(LaunchTask* parent, AuthSessionPtr session)
	: LaunchStep(parent)
{
	if (session->status == AuthSession::Status::PlayableOnline &&
		!session->demo) {
		auto accounts = LAUNCHER->accounts();
		m_account = accounts->getAccountByProfileName(session->player_name);
	}
}

void ClaimAccount::executeTask()
{
	if (m_account) {
		lock.reset(new UseLock(m_account));
		emitSucceeded();
	}
}

void ClaimAccount::finalize()
{
	lock.reset();
}
