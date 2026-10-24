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
 * FriendsController's invokables: add, accept, decline, remove, block, the
 * settings and deleting the account. Each ends with actionFinished(), worded
 * in the selected language.
 */

#include "FriendsController.h"

#include <QMetaObject>

#include "FriendsDisplay.h"
#include "core/LauncherContext.h"
#include "settings/SettingsObject.h"

namespace
{
	const QByteArray kGet = "GET";
	const QByteArray kPost = "POST";
	const QByteArray kPut = "PUT";
	const QByteArray kPatch = "PATCH";
	const QByteArray kDelete = "DELETE";
	constexpr int kCreated = 201;

	bool isErrorAnswer(const FriendsApi::Reply& reply)
	{
		return !reply.ok() && !reply.networkFailed() &&
			   reply.failureText.isEmpty();
	}
} // namespace

void FriendsController::finishAction(const QString& action, bool ok,
									 const QString& message)
{
	QMetaObject::invokeMethod(
		this, [this, action, ok, message]() {
			emit actionFinished(action, ok, message);
		},
		Qt::QueuedConnection);
}

bool FriendsController::canAct(const QString& action, const QString* uuid)
{
	if (!m_enabled) {
		finishAction(action, false, tr("Friends are turned off"));
		return false;
	}
	if (uuid && !FriendsApi::isUuid(*uuid)) {
		finishAction(action, false, tr("That is not a player"));
		return false;
	}
	return true;
}

QString FriendsController::nameOf(const QString& uuid) const
{
	for (const auto& list : {m_friends, m_incoming, m_outgoing, m_blocked}) {
		for (const QVariant& one : list) {
			if (one.toMap().value("uuid").toString() == uuid) {
				return one.toMap().value("name").toString();
			}
		}
	}
	return tr("this player");
}

void FriendsController::act(const QString& action, const QByteArray& verb,
							const QString& path, const QByteArray& body,
							Interpret interpret, bool thenRefresh)
{
	request(verb, path, body,
			[this, action, interpret, thenRefresh](
				const FriendsApi::Reply& reply) {
				bool ok = false;
				const QString message = interpret(reply, &ok);
				finishAction(action, ok, message);
				if (ok && thenRefresh) {
					poll();
				}
			});
}

void FriendsController::refresh()
{
	if (!canAct("refresh", nullptr)) {
		return;
	}
	poll([this](bool ok, const QString& message) {
		finishAction("refresh", ok, message);
	});
}

QString FriendsController::requestErrorText(const FriendsApi::Reply& reply,
											const QString& name) const
{
	if (!isErrorAnswer(reply)) {
		return failureText(reply);
	}
	const QString code = FriendsApi::parseError(reply.body).code;
	if (code == FriendsApi::Code::NotFound) {
		return tr("No Kite player is called %1").arg(name);
	}
	if (code == FriendsApi::Code::AlreadyFriends) {
		return tr("%1 is already your friend").arg(name);
	}
	if (code == FriendsApi::Code::BlockedByYou) {
		return tr("You blocked %1; unblock them first").arg(name);
	}
	if (code == FriendsApi::Code::LimitReached) {
		return tr("You have reached the limit of friends or requests");
	}
	if (code == FriendsApi::Code::BadRequest) {
		return tr("That is not a name you can add");
	}
	return failureText(reply);
}

void FriendsController::sendRequest(const QString& rawName)
{
	if (!canAct("sendRequest", nullptr)) {
		return;
	}
	const QString name = rawName.trimmed();
	if (!FriendsApi::isMinecraftName(name)) {
		finishAction("sendRequest", false,
					 tr("That is not a Minecraft name"));
		return;
	}
	if (name.compare(m_session.name, Qt::CaseInsensitive) == 0) {
		finishAction("sendRequest", false, tr("That is your own name"));
		return;
	}
	act("sendRequest", kPost, FriendsApi::Path::requests(),
		FriendsApi::requestBody(name),
		[this, name](const FriendsApi::Reply& reply, bool* ok) {
			const auto result = reply.ok()
									? FriendsApi::parseRequestResult(reply.body)
									: std::nullopt;
			if (!result) {
				return requestErrorText(reply, name);
			}
			*ok = true;
			if (result->status == "accepted") {
				return tr("%1 is now your friend").arg(result->name);
			}
			return reply.status == kCreated
					   ? tr("Friend request sent to %1").arg(result->name)
					   : tr("You have already sent %1 a request")
							 .arg(result->name);
		});
}

void FriendsController::accept(const QString& uuid)
{
	if (!canAct("accept", &uuid)) {
		return;
	}
	act("accept", kPost, FriendsApi::Path::accept(uuid), QByteArray(),
		[this, uuid](const FriendsApi::Reply& reply, bool* ok) {
			const auto user = reply.ok() ? FriendsApi::parseUser(reply.body)
										 : std::nullopt;
			if (!user) {
				return reply.status == 404
						   ? tr("That friend request is gone")
						   : failureText(reply);
			}
			*ok = true;
			return tr("%1 is now your friend").arg(user->name);
		});
}

void FriendsController::decline(const QString& uuid)
{
	if (!canAct("decline", &uuid)) {
		return;
	}
	act("decline", kPost, FriendsApi::Path::decline(uuid), QByteArray(),
		[this](const FriendsApi::Reply& reply, bool* ok) {
			*ok = reply.ok();
			return *ok ? tr("Request declined")
					   : reply.status == 404
							 ? tr("That friend request is gone")
							 : failureText(reply);
		});
}

void FriendsController::cancelRequest(const QString& uuid)
{
	if (!canAct("cancelRequest", &uuid)) {
		return;
	}
	act("cancelRequest", kDelete, FriendsApi::Path::cancel(uuid), QByteArray(),
		[this](const FriendsApi::Reply& reply, bool* ok) {
			*ok = reply.ok();
			return *ok ? tr("Request cancelled")
					   : reply.status == 404
							 ? tr("That friend request is gone")
							 : failureText(reply);
		});
}

void FriendsController::removeFriend(const QString& uuid)
{
	if (!canAct("removeFriend", &uuid)) {
		return;
	}
	const QString name = nameOf(uuid);
	act("removeFriend", kDelete, FriendsApi::Path::removeFriend(uuid),
		QByteArray(), [this, name](const FriendsApi::Reply& reply, bool* ok) {
			*ok = reply.ok();
			return *ok ? tr("%1 is no longer your friend").arg(name)
					   : reply.status == 404
							 ? tr("%1 is not your friend").arg(name)
							 : failureText(reply);
		});
}

void FriendsController::loadBlocked(
	std::function<void(const FriendsApi::Reply&, bool)> done)
{
	request(kGet, FriendsApi::Path::blocks(), QByteArray(),
			[this, done](const FriendsApi::Reply& reply) {
				const auto blocked = reply.ok()
										 ? FriendsApi::parseBlocked(reply.body)
										 : std::nullopt;
				if (blocked) {
					m_blocked = FriendsDisplay::personList(*blocked);
					emit changed();
				}
				done(reply, blocked.has_value());
			});
}

void FriendsController::refreshBlocked()
{
	if (!canAct("refreshBlocked", nullptr)) {
		return;
	}
	loadBlocked([this](const FriendsApi::Reply& reply, bool ok) {
		finishAction("refreshBlocked", ok, ok ? QString() : failureText(reply));
	});
}

void FriendsController::block(const QString& uuid)
{
	if (!canAct("block", &uuid)) {
		return;
	}
	const QString name = nameOf(uuid);
	act("block", kPut, FriendsApi::Path::block(uuid), QByteArray(),
		[this, name](const FriendsApi::Reply& reply, bool* ok) {
			*ok = reply.ok();
			if (*ok) {
				loadBlocked([](const FriendsApi::Reply&, bool) {});
				return tr("%1 is blocked").arg(name);
			}
			return reply.status == 404
					   ? tr("No Kite player has that name")
					   : requestErrorText(reply, name);
		});
}

void FriendsController::unblock(const QString& uuid)
{
	if (!canAct("unblock", &uuid)) {
		return;
	}
	const QString name = nameOf(uuid);
	act("unblock", kDelete, FriendsApi::Path::block(uuid), QByteArray(),
		[this, name](const FriendsApi::Reply& reply, bool* ok) {
			*ok = reply.ok();
			if (*ok) {
				loadBlocked([](const FriendsApi::Reply&, bool) {});
				return tr("%1 is unblocked").arg(name);
			}
			return reply.status == 404 ? tr("%1 is not blocked").arg(name)
									   : failureText(reply);
		});
}

void FriendsController::setSettings(const QVariantMap& partial)
{
	if (!canAct("setSettings", nullptr)) {
		return;
	}
	const QByteArray body = FriendsApi::settingsBody(partial);
	if (body == "{}") {
		finishAction("setSettings", false, tr("Nothing to change"));
		return;
	}
	act("setSettings", kPatch, FriendsApi::Path::settings(), body,
		[this](const FriendsApi::Reply& reply, bool* ok) {
			const auto settings = reply.ok()
									  ? FriendsApi::parseSettings(reply.body)
									  : std::nullopt;
			if (!settings) {
				return failureText(reply);
			}
			*ok = true;
			m_settings = FriendsDisplay::settingsMap(*settings);
			emit changed();
			/* What friends see depends on these. */
			sendStatus([](bool) {}, true);
			return tr("Saved");
		},
		false);
}

void FriendsController::deleteAccount()
{
	if (!canAct("deleteAccount", nullptr)) {
		return;
	}
	request(kDelete, FriendsApi::Path::me(), QByteArray(),
			[this](const FriendsApi::Reply& reply) {
				if (!reply.ok()) {
					finishAction("deleteAccount", false, failureText(reply));
					return;
				}
				/* Gone on the server: nothing left to tell it. */
				LAUNCHER->settings()->set(kEnabledSetting, false);
				stop(false);
				finishAction("deleteAccount", true,
							 tr("Your friends account and its data were "
								"deleted"));
			});
}
