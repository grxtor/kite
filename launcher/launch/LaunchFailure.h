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

#include <QCoreApplication>
#include <QString>
#include <QStringList>

class UiHost;

/*
 * What to tell the player when a launch ends without the game having run:
 * what failed, the reason as the failing step gave it, and the one action
 * that helps. A game that started and then exited is the crash doctor's, not
 * this.
 */
class LaunchFailure
{
	Q_DECLARE_TR_FUNCTIONS(LaunchFailure)

  public:
	enum class Kind {
		/// Nothing to say: the player cancelled, was already told, or the
		/// game itself exited (the crash doctor speaks for that).
		Quiet,
		/// The account cannot play as it is; signing in fixes it.
		Account,
		/// The account could not be refreshed; trying again may work.
		Login,
		/// A launch step failed before the game's process started.
		BeforeStart,
	};

	/// What the player picked in the notice.
	enum class Action { None, SignIn, Retry, ShowLog };

	struct Notice {
		QString title;
		QString text;
		/// One label per entry of `actions`; the first is the main one.
		QStringList labels;
		QList<Action> actions;
	};

	/// The notice for a failure of @p kind launching @p instanceName, with the
	/// failing step's own @p reason kept as the detail. Not for Kind::Quiet.
	static Notice notice(Kind kind, const QString& instanceName,
						 const QString& reason);

	/// Shows notice() through @p host and returns what the player picked;
	/// Action::None for Kind::Quiet (nothing is shown) or a closed dialog.
	static Action present(UiHost& host, Kind kind, const QString& instanceName,
						  const QString& reason);

	/// How often one launch refreshes an account that keeps coming back
	/// Errored before it gives up (LaunchController::login()).
	static constexpr int kMaxAccountRefreshes = 2;
	static bool mayRefreshAgain(int refreshesSoFar)
	{
		return refreshesSoFar < kMaxAccountRefreshes;
	}
};
