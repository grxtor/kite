/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
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

#include "UpdateManifest.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <monocypher-ed25519.h>

namespace
{
	constexpr auto kFormat = "kite-update-manifest";
	constexpr int kSchema = 1;
	constexpr qsizetype kPublicKeyBytes = 32;
	constexpr qsizetype kSignatureBytes = 64;

	void setError(QString* error, const QString& text)
	{
		if (error)
			*error = text;
	}

	std::optional<QByteArray> decodeBase64(const QByteArray& text,
										   qsizetype expectedBytes)
	{
		const auto decoded = QByteArray::fromBase64Encoding(
			text.trimmed(), QByteArray::AbortOnBase64DecodingErrors);
		if (!decoded || decoded.decoded.size() != expectedBytes)
			return std::nullopt;
		return decoded.decoded;
	}

	//! A plain file name: the updater saves the download under it.
	bool isPlainFileName(const QString& name)
	{
		return !name.isEmpty() && name != QLatin1String(".") &&
			   name != QLatin1String("..") &&
			   !name.contains(QLatin1Char('/')) &&
			   !name.contains(QLatin1Char('\\'));
	}

} // namespace

std::optional<UpdateManifest::Asset>
UpdateManifest::Manifest::asset(const QString& name) const
{
	for (const Asset& entry : assets) {
		if (entry.name == name)
			return entry;
	}
	return std::nullopt;
}

bool UpdateManifest::verifySignature(const QByteArray& manifest,
									 const QByteArray& signature,
									 const QString& publicKey, QString* error)
{
	const auto key = decodeBase64(publicKey.toLatin1(), kPublicKeyBytes);
	if (!key) {
		setError(error, QStringLiteral("the built-in update key is not a base64 "
									   "Ed25519 public key"));
		return false;
	}
	const auto sig = decodeBase64(signature, kSignatureBytes);
	if (!sig) {
		setError(error,
				 QStringLiteral("%1 is not a base64 Ed25519 signature")
					 .arg(QLatin1String(kSignatureName)));
		return false;
	}

	const int status = crypto_ed25519_check(
		reinterpret_cast<const uint8_t*>(sig->constData()),
		reinterpret_cast<const uint8_t*>(key->constData()),
		reinterpret_cast<const uint8_t*>(manifest.constData()),
		static_cast<size_t>(manifest.size()));
	if (status != 0) {
		setError(error, QStringLiteral("the signature on %1 does not verify")
							.arg(QLatin1String(kManifestName)));
		return false;
	}
	return true;
}

std::optional<UpdateManifest::Manifest>
UpdateManifest::load(const QByteArray& manifest, const QByteArray& signature,
					 const QString& publicKey, const QString& expectedTag,
					 QString* error)
{
	if (manifest.size() > kMaxBytes) {
		setError(error, QStringLiteral("%1 is too large to be a manifest")
							.arg(QLatin1String(kManifestName)));
		return std::nullopt;
	}
	if (!verifySignature(manifest, signature, publicKey, error))
		return std::nullopt;

	QJsonParseError parseError;
	const QJsonDocument document = QJsonDocument::fromJson(manifest, &parseError);
	if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
		setError(error, QStringLiteral("%1 is not a JSON object: %2")
							.arg(QLatin1String(kManifestName),
								 parseError.errorString()));
		return std::nullopt;
	}
	const QJsonObject root = document.object();

	if (root.value(QLatin1String("format")).toString() !=
			QLatin1String(kFormat) ||
		root.value(QLatin1String("schema")).toInt(-1) != kSchema) {
		setError(error, QStringLiteral("%1 is not a manifest this updater "
									   "understands (format %2, schema %3)")
							.arg(QLatin1String(kManifestName),
								 root.value(QLatin1String("format")).toString(),
								 QString::number(root.value(QLatin1String("schema"))
													 .toInt(-1))));
		return std::nullopt;
	}

	Manifest parsed;
	parsed.version = root.value(QLatin1String("version")).toString();
	parsed.tag = root.value(QLatin1String("tag")).toString();
	if (parsed.version.isEmpty() || parsed.tag != expectedTag) {
		setError(error, QStringLiteral("%1 belongs to release %2, not %3")
							.arg(QLatin1String(kManifestName),
								 parsed.tag.isEmpty() ? QStringLiteral("(none)")
													  : parsed.tag,
								 expectedTag));
		return std::nullopt;
	}

	static const QRegularExpression sha256Pattern(
		QStringLiteral("^[0-9a-f]{64}$"));
	QSet<QString> seen;
	const QJsonArray assets = root.value(QLatin1String("assets")).toArray();
	for (const QJsonValue& value : assets) {
		const QJsonObject object = value.toObject();
		Asset entry;
		entry.name = object.value(QLatin1String("name")).toString();
		entry.size = object.value(QLatin1String("size")).toInteger(-1);
		entry.sha256 = object.value(QLatin1String("sha256")).toString();

		if (!isPlainFileName(entry.name) || entry.size < 0 ||
			!sha256Pattern.match(entry.sha256).hasMatch()) {
			setError(error, QStringLiteral("%1 has a malformed entry for '%2'")
								.arg(QLatin1String(kManifestName), entry.name));
			return std::nullopt;
		}
		if (seen.contains(entry.name)) {
			setError(error, QStringLiteral("%1 lists '%2' twice")
								.arg(QLatin1String(kManifestName), entry.name));
			return std::nullopt;
		}
		seen.insert(entry.name);
		parsed.assets.append(entry);
	}
	if (parsed.assets.isEmpty()) {
		setError(error, QStringLiteral("%1 lists no files")
							.arg(QLatin1String(kManifestName)));
		return std::nullopt;
	}

	return parsed;
}
