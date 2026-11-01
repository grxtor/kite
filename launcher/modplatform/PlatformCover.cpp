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

#include "PlatformCover.h"

#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include "Json.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/modrinth/ModrinthApi.h"

namespace PlatformCover
{
	namespace
	{
		constexpr int kMaxTitles = 3;

		// The id goes into a URL path unescaped, so only plain ones pass.
		bool isSafeId(const QString& id, Provider provider)
		{
			static const QRegularExpression modrinth(
				QStringLiteral("^(?=.*[A-Za-z0-9])[A-Za-z0-9_.\\-]{1,64}$"));
			static const QRegularExpression flame(
				QStringLiteral("^[0-9]{1,12}$"));
			return provider == Provider::CurseForge
					   ? flame.match(id).hasMatch()
					   : modrinth.match(id).hasMatch();
		}

		bool isTrustedImageUrl(Provider provider, const QUrl& url)
		{
			if (url.scheme() != QLatin1String("https")) {
				return false;
			}
			const QString host = url.host().toLower();
			if (provider == Provider::Modrinth) {
				return host == ModrinthApi::cdnHost();
			}
			return host == QLatin1String("forgecdn.net") ||
				   host.endsWith(QLatin1String(".forgecdn.net")) ||
				   host == QLatin1String("curseforge.com") ||
				   host.endsWith(QLatin1String(".curseforge.com"));
		}

		// A URL with no recognisable suffix is let through; the download is
		// checked once it is on disk.
		bool isDecodable(const QUrl& url, const QStringList& formats)
		{
			QString suffix = QFileInfo(url.path()).suffix().toLower();
			if (suffix.isEmpty()) {
				return true;
			}
			if (suffix == QLatin1String("jpg")) {
				suffix = QStringLiteral("jpeg");
			}
			// A GIF is an animation; its first frame says nothing.
			return suffix != QLatin1String("gif") && formats.contains(suffix);
		}

		QString usableUrl(Provider provider, const QJsonValue& value,
						  const QStringList& formats)
		{
			const QUrl url(value.toString());
			return url.isValid() && isTrustedImageUrl(provider, url) &&
						   isDecodable(url, formats)
					   ? url.toString()
					   : QString();
		}

		QString normalised(const QString& title)
		{
			return title.trimmed().toCaseFolded();
		}

		QString uniqueMatch(const QJsonArray& hits, const QString& titleKey,
							const QString& idKey, const QString& wanted)
		{
			const QString want = normalised(wanted);
			if (want.isEmpty()) {
				return QString();
			}
			QSet<QString> ids;
			for (const QJsonValue& value : hits) {
				const QJsonObject hit = value.toObject();
				if (normalised(hit.value(titleKey).toString()) != want) {
					continue;
				}
				const QJsonValue id = hit.value(idKey);
				const QString text =
					id.isString()
						? id.toString()
						: QString::number(static_cast<qint64>(id.toDouble()));
				if (!text.isEmpty() && text != QLatin1String("0")) {
					ids.insert(text);
				}
			}
			return ids.size() == 1 ? *ids.constBegin() : QString();
		}
	} // namespace

	QString slugFromIconKey(const QString& iconKey)
	{
		static const QString prefix = QStringLiteral("modrinth_");
		const QString key = iconKey.trimmed();
		return key.startsWith(prefix) ? key.mid(prefix.size()) : QString();
	}

	QList<Lookup> lookupsFor(const PackFacts& facts)
	{
		Provider provider =
			ManagedPackController::providerFromString(facts.packProvider);
		QString id = facts.packId.trimmed();
		if (provider == Provider::Unknown) {
			provider =
				ManagedPackController::providerFromString(facts.legacyProvider);
			id = facts.legacyId.trimmed();
		}
		const QString iconSlug = slugFromIconKey(facts.iconKey);
		if (provider == Provider::Unknown && !iconSlug.isEmpty()) {
			provider = Provider::Modrinth;
		}

		if (provider == Provider::Modrinth) {
			// A slug is accepted wherever an id is.
			for (const QString& candidate :
				 {id, facts.packSlug.trimmed(), iconSlug}) {
				if (isSafeId(candidate, provider)) {
					return {{provider, candidate, QString()}};
				}
			}
		} else if (provider == Provider::CurseForge &&
				   isSafeId(id, provider)) {
			return {{provider, id, QString()}};
		}

		// The default icons say where the pack came from, not which pack.
		Provider platform = provider;
		const QString icon = facts.iconKey.trimmed();
		if (platform == Provider::Unknown) {
			platform = icon == QLatin1String("modrinth") ? Provider::Modrinth
					   : icon == QLatin1String("flame")	 ? Provider::CurseForge
														 : Provider::Unknown;
		}

		QStringList wanted = facts.packTitles;
		if (platform != Provider::Unknown) {
			wanted.append(facts.instanceName);
		}
		QStringList titles;
		QSet<QString> seen;
		for (const QString& title : wanted) {
			const QString key = normalised(title);
			if (!key.isEmpty() && !seen.contains(key) &&
				titles.size() < kMaxTitles) {
				seen.insert(key);
				titles.append(title.trimmed());
			}
		}

		QList<Lookup> lookups;
		if (platform == Provider::Unknown) {
			return lookups;
		}
		for (const QString& title : titles) {
			lookups.append({platform, QString(), title});
		}
		return lookups;
	}

	QStringList decodableFormats()
	{
		QStringList formats;
		for (const QByteArray& format : QImageReader::supportedImageFormats()) {
			const QString name = QString::fromLatin1(format).toLower();
			formats.append(name == QLatin1String("jpg")
							   ? QStringLiteral("jpeg")
							   : name);
		}
		return formats;
	}

	QStringList modrinthImages(const QJsonArray& gallery,
							   const QStringList& formats)
	{
		QList<QJsonObject> ordered;
		for (const bool featured : {true, false}) {
			for (const QJsonValue& value : gallery) {
				const QJsonObject entry = value.toObject();
				if (entry.value(QStringLiteral("featured")).toBool() ==
					featured) {
					ordered.append(entry);
				}
			}
		}
		QStringList urls;
		for (const QString& key :
			 {QStringLiteral("raw_url"), QStringLiteral("url")}) {
			for (const QJsonObject& entry : ordered) {
				const QString url = usableUrl(Provider::Modrinth,
											  entry.value(key), formats);
				if (!url.isEmpty() && !urls.contains(url) &&
					urls.size() < kMaxCandidates) {
					urls.append(url);
				}
			}
		}
		return urls;
	}

	QStringList flameImages(const QJsonArray& screenshots,
							const QStringList& formats)
	{
		QStringList urls;
		for (const QJsonValue& value : screenshots) {
			const QString url =
				usableUrl(Provider::CurseForge,
						  value.toObject().value(QStringLiteral("url")),
						  formats);
			if (!url.isEmpty() && !urls.contains(url) &&
				urls.size() < kMaxCandidates) {
				urls.append(url);
			}
		}
		return urls;
	}

	bool suitableAsCover(const QSize& size)
	{
		return size.isValid() && size.width() >= kMinImageWidth &&
			   static_cast<double>(size.width()) / size.height() >= kMinAspect;
	}

	QString uniqueModrinthMatch(const QJsonArray& hits, const QString& title)
	{
		return uniqueMatch(hits, QStringLiteral("title"),
						   QStringLiteral("project_id"), title);
	}

	QString uniqueFlameMatch(const QJsonArray& hits, const QString& title)
	{
		return uniqueMatch(hits, QStringLiteral("name"), QStringLiteral("id"),
						   title);
	}

	QUrl searchUrl(const Lookup& lookup)
	{
		return lookup.provider == Provider::CurseForge
				   ? FlameApi::modpackSearchUrl(lookup.title, 1, 0)
				   : ModrinthApi::modpackSearchUrl(lookup.title, 0, 0);
	}

	QUrl projectUrl(Provider provider, const QString& projectId)
	{
		return provider == Provider::CurseForge
				   ? FlameApi::get().projectUrl(projectId)
				   : ModrinthApi::get().projectUrl(projectId);
	}

	QString flameManifestName(const QString& instanceRoot)
	{
		QFile file(instanceRoot + QStringLiteral("/flame/manifest.json"));
		if (!file.open(QIODevice::ReadOnly)) {
			return QString();
		}
		const auto document = Json::tryParse(file.readAll());
		return document && document->isObject()
				   ? document->object()
						 .value(QStringLiteral("name"))
						 .toString()
						 .trimmed()
				   : QString();
	}

	bool retryDue(qint64 checkedAt, qint64 now)
	{
		// A time in the future (the clock moved back) must not block for good.
		return checkedAt <= 0 || checkedAt > now ||
			   now - checkedAt >= kRetryAfterSeconds;
	}
} // namespace PlatformCover
