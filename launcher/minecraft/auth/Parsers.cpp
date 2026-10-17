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

#include "Parsers.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

#include "Logging.h"
#include "minecraft/skins/DefaultSkins.h"

namespace Parsers
{

	bool getDateTime(QJsonValue value, QDateTime& out)
	{
		if (!value.isString()) {
			return false;
		}
		out = QDateTime::fromString(value.toString(), Qt::ISODate);
		return out.isValid();
	}

	bool getString(QJsonValue value, QString& out)
	{
		if (!value.isString()) {
			return false;
		}
		out = value.toString();
		return true;
	}

	bool getNumber(QJsonValue value, double& out)
	{
		if (!value.isDouble()) {
			return false;
		}
		out = value.toDouble();
		return true;
	}

	bool getNumber(QJsonValue value, int64_t& out)
	{
		if (!value.isDouble()) {
			return false;
		}
		out = (int64_t)value.toDouble();
		return true;
	}

	bool getBool(QJsonValue value, bool& out)
	{
		if (!value.isBool()) {
			return false;
		}
		out = value.toBool();
		return true;
	}

	/*
	{
	   "IssueInstant":"2020-12-07T19:52:08.4463796Z",
	   "NotAfter":"2020-12-21T19:52:08.4463796Z",
	   "Token":"token",
	   "DisplayClaims":{
		  "xui":[
			 {
				"uhs":"userhash"
			 }
		  ]
	   }
	 }
	*/
	// TODO: handle error responses ...
	/*
	{
		"Identity":"0",
		"XErr":2148916238,
		"Message":"",
		"Redirect":"https://start.ui.xboxlive.com/AddChildToFamily"
	}
	// 2148916233 = missing XBox account
	// 2148916238 = child account not linked to a family
	*/

	namespace
	{
		bool isTokenKey(const QString& key)
		{
			static const QStringList keys = {
				QStringLiteral("token"), QStringLiteral("access_token"),
				QStringLiteral("refresh_token"), QStringLiteral("id_token"),
				QStringLiteral("identitytoken")};
			return keys.contains(key.toLower());
		}

		QJsonValue redacted(const QJsonValue& value)
		{
			if (value.isArray()) {
				QJsonArray out;
				for (const QJsonValue& item : value.toArray()) {
					out.append(redacted(item));
				}
				return out;
			}
			if (!value.isObject()) {
				return value;
			}
			QJsonObject out = value.toObject();
			for (auto it = out.begin(); it != out.end(); ++it) {
				it.value() = isTokenKey(it.key())
								 ? QJsonValue(QStringLiteral("<redacted>"))
								 : redacted(it.value());
			}
			return out;
		}
	} // namespace

	QByteArray redactTokens(const QByteArray& data)
	{
		// Enough of an error page to tell what it was.
		constexpr int maxRawBytes = 256;
		QJsonParseError error;
		const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
		if (error.error != QJsonParseError::NoError) {
			return data.left(maxRawBytes);
		}
		if (doc.isArray()) {
			return QJsonDocument(redacted(doc.array()).toArray())
				.toJson(QJsonDocument::Compact);
		}
		return QJsonDocument(redacted(doc.object()).toObject())
			.toJson(QJsonDocument::Compact);
	}

	bool parseXTokenResponse(QByteArray& data, Katabasis::Token& output,
							 QString name)
	{
		qCDebug(minecraftauthLog) << "Parsing" << name << ":";
#ifndef NDEBUG
		qCDebug(minecraftauthLog) << redactTokens(data);
#endif
		QJsonParseError jsonError;
		QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error) {
			qWarning() << "Failed to parse response from "
						  "user.auth.xboxlive.com as JSON: "
					   << jsonError.errorString();
			return false;
		}

		auto obj = doc.object();
		if (!getDateTime(obj.value("IssueInstant"), output.issueInstant)) {
			qWarning() << "User IssueInstant is not a timestamp";
			return false;
		}
		if (!getDateTime(obj.value("NotAfter"), output.notAfter)) {
			qWarning() << "User NotAfter is not a timestamp";
			return false;
		}
		if (!getString(obj.value("Token"), output.token)) {
			qWarning() << "User Token is not a string";
			return false;
		}
		auto arrayVal = obj.value("DisplayClaims").toObject().value("xui");
		if (!arrayVal.isArray()) {
			qWarning() << "Missing xui claims array";
			return false;
		}
		bool foundUHS = false;
		for (auto item : arrayVal.toArray()) {
			if (!item.isObject()) {
				continue;
			}
			auto claimObject = item.toObject();
			if (claimObject.contains("uhs")) {
				foundUHS = true;
			} else {
				continue;
			}
			// consume all 'display claims' ... whatever that means
			for (auto iter = claimObject.begin(); iter != claimObject.end();
				 iter++) {
				QString claim;
				if (!getString(claimObject.value(iter.key()), claim)) {
					qWarning() << "display claim " << iter.key()
							   << " is not a string...";
					return false;
				}
				output.extra[iter.key()] = claim;
			}

			break;
		}
		if (!foundUHS) {
			qWarning() << "Missing uhs";
			return false;
		}
		output.validity = Katabasis::Validity::Certain;
		qCDebug(minecraftauthLog) << name << "is valid.";
		return true;
	}

	bool parseMinecraftProfile(QByteArray& data, MinecraftProfile& output)
	{
		qCDebug(minecraftauthLog) << "Parsing Minecraft profile...";
#ifndef NDEBUG
		qCDebug(minecraftauthLog) << redactTokens(data);
#endif

		QJsonParseError jsonError;
		QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error) {
			qWarning() << "Failed to parse response from "
						  "user.auth.xboxlive.com as JSON: "
					   << jsonError.errorString();
			return false;
		}

		auto obj = doc.object();
		if (!getString(obj.value("id"), output.id)) {
			qWarning() << "Minecraft profile id is not a string";
			return false;
		}

		if (!getString(obj.value("name"), output.name)) {
			qWarning() << "Minecraft profile name is not a string";
			return false;
		}

		/* Reset for the same reason as the capes below: without an ACTIVE
		 * skin in this response, the previous refresh's skin would stay. */
		output.skin = Skin();
		auto skinsArray = obj.value("skins").toArray();
		for (auto skin : skinsArray) {
			auto skinObj = skin.toObject();
			Skin skinOut;
			if (!getString(skinObj.value("id"), skinOut.id)) {
				continue;
			}
			QString state;
			if (!getString(skinObj.value("state"), state)) {
				continue;
			}
			if (state != "ACTIVE") {
				continue;
			}
			if (!getString(skinObj.value("url"), skinOut.url)) {
				continue;
			}
			if (!getString(skinObj.value("variant"), skinOut.variant)) {
				continue;
			}
			// we deal with only the active skin
			output.skin = skinOut;
			break;
		}
		auto capesArray = obj.value("capes").toArray();

		/* `output` is the account's stored profile, parsed into again on every
		 * refresh: without this the cape list grew by a full copy each time
		 * (and was saved that way). */
		output.capes.clear();
		QString currentCape;
		for (auto cape : capesArray) {
			auto capeObj = cape.toObject();
			Cape capeOut;
			if (!getString(capeObj.value("id"), capeOut.id)) {
				continue;
			}
			QString state;
			if (!getString(capeObj.value("state"), state)) {
				continue;
			}
			if (state == "ACTIVE") {
				currentCape = capeOut.id;
			}
			if (!getString(capeObj.value("url"), capeOut.url)) {
				continue;
			}
			if (!getString(capeObj.value("alias"), capeOut.alias)) {
				continue;
			}

			/* Appended, not keyed: the service's order is the order the cape
			 * picker shows. */
			output.capes.append(capeOut);
		}
		output.currentCape = currentCape;
		output.validity = Katabasis::Validity::Certain;
		return true;
	}

	bool parseMinecraftEntitlements(QByteArray& data,
									MinecraftEntitlement& output)
	{
		qCDebug(minecraftauthLog) << "Parsing Minecraft entitlements...";
#ifndef NDEBUG
		qCDebug(minecraftauthLog) << redactTokens(data);
#endif

		QJsonParseError jsonError;
		QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error) {
			qWarning() << "Failed to parse response from "
						  "user.auth.xboxlive.com as JSON: "
					   << jsonError.errorString();
			return false;
		}

		auto obj = doc.object();
		output.canPlayMinecraft = false;
		output.ownsMinecraft = false;

		auto itemsArray = obj.value("items").toArray();
		for (auto item : itemsArray) {
			auto itemObj = item.toObject();
			QString name;
			if (!getString(itemObj.value("name"), name)) {
				continue;
			}
			if (name == "game_minecraft") {
				output.canPlayMinecraft = true;
			}
			if (name == "product_minecraft") {
				output.ownsMinecraft = true;
			}
		}
		output.validity = Katabasis::Validity::Certain;
		return true;
	}

	bool parseRolloutResponse(QByteArray& data, bool& result)
	{
		qCDebug(minecraftauthLog) << "Parsing Rollout response...";
#ifndef NDEBUG
		qCDebug(minecraftauthLog) << redactTokens(data);
#endif

		QJsonParseError jsonError;
		QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error) {
			qWarning() << "Failed to parse response from "
						  "https://api.minecraftservices.com/rollout/v1/"
						  "msamigration as JSON: "
					   << jsonError.errorString();
			return false;
		}

		auto obj = doc.object();
		QString feature;
		if (!getString(obj.value("feature"), feature)) {
			qWarning() << "Rollout feature is not a string";
			return false;
		}
		if (feature != "msamigration") {
			qWarning() << "Rollout feature is not what we expected "
						  "(msamigration), but is instead \""
					   << feature << "\"";
			return false;
		}
		if (!getBool(obj.value("rollout"), result)) {
			qWarning() << "Rollout feature is not a string";
			return false;
		}
		return true;
	}

	bool parseMojangResponse(QByteArray& data, Katabasis::Token& output)
	{
		QJsonParseError jsonError;
		qCDebug(minecraftauthLog) << "Parsing Mojang response...";
#ifndef NDEBUG
		qCDebug(minecraftauthLog) << redactTokens(data);
#endif
		QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error) {
			qWarning() << "Failed to parse response from "
						  "api.minecraftservices.com/launcher/login as JSON: "
					   << jsonError.errorString();
			return false;
		}

		auto obj = doc.object();
		double expires_in = 0;
		if (!getNumber(obj.value("expires_in"), expires_in)) {
			qWarning() << "expires_in is not a valid number";
			return false;
		}
		auto currentTime = QDateTime::currentDateTimeUtc();
		output.issueInstant = currentTime;
		output.notAfter = currentTime.addSecs(expires_in);

		QString username;
		if (!getString(obj.value("username"), username)) {
			qWarning() << "username is not valid";
			return false;
		}

		// TODO: it's a JWT... validate it?
		if (!getString(obj.value("access_token"), output.token)) {
			qWarning() << "access_token is not valid";
			return false;
		}
		output.validity = Katabasis::Validity::Certain;
		qCDebug(minecraftauthLog) << "Mojang response is valid.";
		return true;
	}

	namespace
	{
		/* Which of the two default skins a player without a custom one gets.
		 *
		 * The game decides this from the Java hashCode of the profile UUID:
		 * even means the classic model, odd means slim. Reproducing it means
		 * folding the 128-bit id down the same way Java's UUID.hashCode()
		 * does -- xor the two halves, then xor the halves of that. */
		bool defaultsToClassicModel(QString uuid)
		{
			uuid.remove('-');
			if (uuid.size() != 32) {
				/* Not a UUID we can read; classic is the safer guess, since
				 * it is what the game falls back to as well. */
				return true;
			}

			bool okHigh = false;
			bool okLow = false;
			const quint64 high = uuid.left(16).toULongLong(&okHigh, 16);
			const quint64 low = uuid.right(16).toULongLong(&okLow, 16);
			if (!okHigh || !okLow) {
				return true;
			}

			const quint64 folded = high ^ low;
			const quint32 hashCode = static_cast<quint32>(folded >> 32) ^
									 static_cast<quint32>(folded);
			return hashCode % 2 == 0;
		}

		/* The session server still hands out plain-http texture URLs. Qt will
		 * follow them, but there is no reason to fetch a skin in the clear
		 * when the same host serves it over TLS. */
		QString preferHttps(QString textureUrl)
		{
			return textureUrl.replace(
				QLatin1String("http://textures.minecraft.net"),
				QLatin1String("https://textures.minecraft.net"));
		}

		/* Pull the base64 "textures" property out of a session profile. */
		QByteArray findTexturePayload(const QJsonArray& properties)
		{
			for (const QJsonValue& property : properties) {
				const QJsonObject entry = property.toObject();
				if (entry.value("name").toString() !=
					QLatin1String("textures")) {
					continue;
				}
				const QJsonValue value = entry.value("value");
				if (!value.isString()) {
					continue;
				}
				const QByteArray decoded = QByteArray::fromBase64(
					value.toString().toUtf8(),
					QByteArray::AbortOnBase64DecodingErrors);
				if (!decoded.isEmpty()) {
					return decoded;
				}
			}
			return QByteArray();
		}
	} // namespace

	bool parseMojangSessionProfile(QByteArray& data, MinecraftProfile& output)
	{
		/* This is the *public* profile endpoint
		 * (sessionserver.mojang.com/session/minecraft/profile/<id>), not the
		 * authenticated one parseMinecraftProfile() handles. It describes
		 * somebody else's account, so it carries no entitlements and no cape
		 * ids -- only texture URLs, and those are buried in a base64 blob. */
		qCDebug(minecraftauthLog) << "Parsing Mojang session profile...";

		QJsonParseError jsonError;
		QJsonDocument doc = QJsonDocument::fromJson(data, &jsonError);
		if (jsonError.error) {
			qWarning() << "Failed to parse response from sessionserver as "
						  "JSON:"
					   << jsonError.errorString();
			return false;
		}

		QJsonObject obj = doc.object();
		if (!getString(obj.value("id"), output.id)) {
			qWarning() << "Session profile id is not a string";
			return false;
		}
		if (!getString(obj.value("name"), output.name)) {
			qWarning() << "Session profile name is not a string";
			return false;
		}

		const QByteArray texturePayload =
			findTexturePayload(obj.value("properties").toArray());
		if (texturePayload.isEmpty()) {
			qWarning() << "Session profile carries no texture payload";
			return false;
		}

		doc = QJsonDocument::fromJson(texturePayload, &jsonError);
		if (jsonError.error) {
			qWarning() << "Failed to parse the session texture payload as "
						  "JSON:"
					   << jsonError.errorString();
			return false;
		}

		const QJsonValue textures = doc.object().value("textures");
		if (!textures.isObject()) {
			qWarning() << "Session texture payload has no textures object";
			return false;
		}

		Skin skinOut;
		/* Start from the default this player would be shown with, so that a
		 * profile with no SKIN entry still yields something displayable. */
		const bool classic = defaultsToClassicModel(output.id);
		skinOut.variant = classic ? QStringLiteral("CLASSIC")
								  : QStringLiteral("SLIM");
		/* The session server omits the SKIN texture entirely for players
		 * who never set one, so there is no URL to read: point at the
		 * canonical default skins (MHF_Steve and MHF_Alex), the only way to
		 * show such a player as the game shows them. */
		skinOut.url = DefaultSkins::fallbackUrl(!classic);
		/* The endpoint does not expose texture ids at all. Nothing downstream
		 * of here needs one, so it is left empty rather than invented. */

		Cape capeOut;
		bool hasCape = false;

		const QJsonObject textureObj = textures.toObject();
		for (auto it = textureObj.constBegin(); it != textureObj.constEnd();
			 ++it) {
			if (!it->isObject()) {
				continue;
			}
			const QJsonObject texture = it->toObject();

			if (it.key() == QLatin1String("SKIN")) {
				if (!getString(texture.value("url"), skinOut.url)) {
					qWarning() << "Session profile skin url is not a string";
					return false;
				}
				skinOut.url = preferHttps(skinOut.url);

				/* Present only for slim skins; absent means classic, which
				 * is already what variant holds unless the UUID said
				 * otherwise -- so read it, but do not require it. */
				const QJsonValue metadata = texture.value("metadata");
				if (metadata.isObject()) {
					getString(metadata.toObject().value("model"),
							  skinOut.variant);
				}
			} else if (it.key() == QLatin1String("CAPE")) {
				if (!getString(texture.value("url"), capeOut.url)) {
					qWarning() << "Session profile cape url is not a string";
					return false;
				}
				capeOut.url = preferHttps(capeOut.url);
				/* No id is published for it either. A stable placeholder is
				 * enough: this profile is only ever read to copy a look, and
				 * nothing tries to equip somebody else's cape. */
				capeOut.id = QStringLiteral("cape");
				capeOut.alias = QStringLiteral("cape");
				hasCape = true;
			}
		}

		output.skin = skinOut;
		if (hasCape) {
			output.capes.clear();
			output.capes.append(capeOut);
			output.currentCape = capeOut.id;
		}
		output.validity = Katabasis::Validity::Certain;
		return true;
	}

} // namespace Parsers
