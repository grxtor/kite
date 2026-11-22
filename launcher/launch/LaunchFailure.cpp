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

#include "LaunchFailure.h"

#include "core/UiHost.h"

LaunchFailure::Notice LaunchFailure::notice(Kind kind,
											const QString& instanceName,
											const QString& reason)
{
	const QString detail = reason.trimmed();
	Notice notice;
	switch (kind) {
		case Kind::Quiet:
			break;
		case Kind::Account: {
			notice.title = tr("Sign in to play");
			notice.text =
				tr("%1 wasn't started because the account can't be used to "
				   "play yet. Sign in on the Accounts page, then press Play "
				   "again.")
					.arg(instanceName);
			notice.labels = {tr("Sign in")};
			notice.actions = {Action::SignIn};
			break;
		}
		case Kind::Login: {
			notice.title = tr("Couldn't sign in");
			notice.text =
				tr("%1 wasn't started because Kite couldn't check your "
				   "account. Check your internet connection and try again.")
					.arg(instanceName);
			notice.labels = {tr("Try again")};
			notice.actions = {Action::Retry};
			break;
		}
		case Kind::BeforeStart: {
			notice.title = tr("Couldn't start %1").arg(instanceName);
			notice.text =
				tr("The game didn't open: Kite ran into a problem while "
				   "getting it ready.");
			notice.labels = {tr("Try again"), tr("Show log")};
			notice.actions = {Action::Retry, Action::ShowLog};
			break;
		}
	}
	if (kind != Kind::Quiet && !detail.isEmpty()) {
		notice.text += QStringLiteral("\n\n") + tr("Reason: %1").arg(detail);
	}
	return notice;
}

LaunchFailure::Action LaunchFailure::present(UiHost& host, Kind kind,
											 const QString& instanceName,
											 const QString& reason)
{
	if (kind == Kind::Quiet) {
		return Action::None;
	}
	const Notice notice = LaunchFailure::notice(kind, instanceName, reason);
	const int picked = host.choose(notice.title, notice.text,
								   UiHost::Severity::Warning, notice.labels);
	if (picked < 0 || picked >= notice.actions.size()) {
		return Action::None;
	}
	return notice.actions.at(picked);
}
