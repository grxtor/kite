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

#include "ShareCodesController.h"

#include <QJsonDocument>
#include <QMetaObject>

#include <algorithm>

#include "FriendsController.h"
#include "InstanceList.h"
#include "core/LauncherContext.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PerformanceSetupRules.h"
#include "modplatform/modrinth/ModrinthApi.h"
#include "net/JsonPost.h"

namespace
{
	const QByteArray kGet = "GET";
	const QByteArray kPost = "POST";
	const QByteArray kDelete = "DELETE";
	constexpr int kBadRequest = 400;
	constexpr int kNotFound = 404;
	constexpr int kConflict = 409;
	constexpr int kTooManyRequests = 429;
	constexpr int kSecondsPerMinute = 60;
} // namespace

ShareCodesController::ShareCodesController(FriendsController* friends,
										   QObject* parent)
	: QObject(parent), m_friends(friends)
{
	connect(m_friends, &FriendsController::changed, this,
			&ShareCodesController::availableChanged);
}

ShareCodesController::~ShareCodesController() = default;

bool ShareCodesController::available() const
{
	return m_friends->enabled();
}

QVariantList ShareCodesController::codes() const
{
	return m_codes;
}

bool ShareCodesController::codesLoaded() const
{
	return m_codesLoaded;
}

void ShareCodesController::finishAction(const QString& action, bool ok,
										const QString& message)
{
	QMetaObject::invokeMethod(
		this, [=]() { emit actionFinished(action, ok, message); },
		Qt::QueuedConnection);
}

QString ShareCodesController::errorText(const FriendsApi::Reply& reply,
										bool lookingUp) const
{
	if (!reply.failureText.isEmpty()) {
		return reply.failureText;
	}
	if (reply.networkFailed()) {
		return tr("Kite's friends server can't be reached.");
	}
	const int minutes = std::max(1, (reply.retryAfterSeconds + kSecondsPerMinute - 1) /
										 kSecondsPerMinute);
	if (reply.status == kTooManyRequests) {
		return lookingUp ? tr("You looked up many codes; try again in %1 min").arg(minutes)
						 : tr("You made many codes; try again in %1 min").arg(minutes);
	}
	if (reply.status == kNotFound) {
		return lookingUp ? tr("There is no such code. It may have run out or been deleted.")
						 : tr("That code is gone already");
	}
	if (reply.status == kConflict) {
		return tr("You have 50 codes, the most there can be. Delete one in "
				  "Settings › Friends to make another.");
	}
	if (reply.status == kBadRequest) {
		return lookingUp ? tr("That is not a share code")
						 : tr("Kite's friends server did not take this instance's list");
	}
	return tr("Something went wrong (%1).").arg(FriendsApi::parseError(reply.body).code);
}

void ShareCodesController::prepare(const QString& instanceId)
{
	auto base = LAUNCHER ? LAUNCHER->instances()->getInstanceById(instanceId) : nullptr;
	auto* instance = dynamic_cast<MinecraftInstance*>(base.get());
	const auto source = instance ? ShareManifest::sourceOf(*instance) : std::nullopt;
	if (!source || !source->unsupportedLoader.isEmpty()) {
		const QString error =
			!source ? tr("This instance has no Minecraft version to share")
					: tr("Instances with %1 can't be shared as a code").arg(source->unsupportedLoader);
		QMetaObject::invokeMethod(
			this, [=]() { emit prepared(instanceId, {{"error", error}}); },
			Qt::QueuedConnection);
		return;
	}
	m_prepared.insert(instanceId, ShareManifest::build(*source));
	const QStringList hashes = ShareManifest::hashesToLookUp(m_prepared.value(instanceId));
	if (hashes.isEmpty()) {
		QMetaObject::invokeMethod(this, [=]() { emitPrepared(instanceId); },
								  Qt::QueuedConnection);
		return;
	}
	m_hashLookup = std::make_unique<Net::JsonPost>(
		tr("Identifying content"), ModrinthApi::versionsByHashesUrl(),
		PerformanceSetup::hashLookupBody(hashes));
	Net::JsonPost* lookup = m_hashLookup.get();
	connect(lookup, &Task::finished, this, [this, lookup, instanceId]() {
		/* A failed lookup only leaves those files out. */
		if (lookup->wasSuccessful() && m_prepared.contains(instanceId)) {
			ShareManifest::applyHashLookup(
				m_prepared[instanceId],
				QJsonDocument::fromJson(lookup->response()).object());
		}
		emitPrepared(instanceId);
	});
	lookup->start();
}

void ShareCodesController::emitPrepared(const QString& instanceId)
{
	emit prepared(instanceId, ShareManifest::toMap(m_prepared.value(instanceId)));
}

void ShareCodesController::share(const QString& instanceId)
{
	if (!m_prepared.contains(instanceId)) {
		finishAction("share", false, tr("Kite is still reading this instance"));
		return;
	}
	const QByteArray body =
		ShareCodes::manifestBody(m_prepared.value(instanceId).manifest);
	m_friends->send(kPost, ShareCodes::Path::codes(), body,
					[this, instanceId](const FriendsApi::Reply& reply) {
						const auto made = reply.ok() ? ShareCodes::parseCreated(reply.body)
													 : std::nullopt;
						if (!made) {
							finishAction("share", false, errorText(reply, false));
							return;
						}
						emit shared(instanceId, ShareCodes::display(made->code));
						finishAction("share", true, QString());
						refreshCodes();
					});
}

void ShareCodesController::refreshCodes()
{
	m_friends->send(kGet, ShareCodes::Path::codes(), QByteArray(),
					[this](const FriendsApi::Reply& reply) {
						const auto codes = reply.ok() ? ShareCodes::parseList(reply.body)
													  : std::nullopt;
						if (!codes) {
							finishAction("refreshCodes", false, errorText(reply, false));
							return;
						}
						m_codes.clear();
						for (const auto& code : *codes) {
							m_codes.append(ShareCodes::toMap(code));
						}
						m_codesLoaded = true;
						emit codesChanged();
						finishAction("refreshCodes", true, QString());
					});
}

void ShareCodesController::deleteCode(const QString& raw)
{
	const QString code = ShareCodes::normalize(raw);
	if (code.isEmpty()) {
		finishAction("deleteCode", false, tr("That is not a share code"));
		return;
	}
	m_friends->send(kDelete, ShareCodes::Path::code(code), QByteArray(),
					[this, code](const FriendsApi::Reply& reply) {
						const bool ok = reply.ok() || reply.status == kNotFound;
						finishAction("deleteCode", ok,
									 ok ? tr("Deleted %1").arg(ShareCodes::display(code))
										: errorText(reply, false));
						refreshCodes();
					});
}

QString ShareCodesController::display(const QString& raw) const
{
	return ShareCodes::display(raw);
}

void ShareCodesController::lookUp(const QString& raw)
{
	const QString code = ShareCodes::normalize(raw);
	if (code.isEmpty()) {
		finishAction("lookUp", false, tr("That is not a share code"));
		return;
	}
	m_friends->send(kGet, ShareCodes::Path::code(code), QByteArray(),
					[this](const FriendsApi::Reply& reply) {
						const auto found = reply.ok() ? ShareCodes::parseFetched(reply.body)
													  : std::nullopt;
						if (!found) {
							finishAction("lookUp", false, errorText(reply, true));
							return;
						}
						m_lookedUp = found;
						emit lookedUp(ShareCodes::toMap(*found));
						finishAction("lookUp", true, QString());
					});
}
