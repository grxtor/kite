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

#include "MetaCatalog.h"

#include <QFile>
#include <QJsonObject>
#include <QUrl>

#include "BuildConfig.h"
#include "Logging.h"
#include "core/LauncherContext.h"
#include "net/Download.h"
#include "net/HttpMetaCache.h"
#include "translations/TranslationsModel.h"

namespace
{
	const QString kTurkish = QStringLiteral("tr");
	const QString kEnglish = QStringLiteral("en");
	const QString kMetaBase = QStringLiteral("general");

	QByteArray readAll(const QString& path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}
} // namespace

namespace MetaCatalog
{
	QString selectedLanguage()
	{
		auto* context = LAUNCHER;
		auto translations = context ? context->translations() : nullptr;
		return translations ? translations->selectedLanguage() : QString();
	}

	QString localized(const QJsonValue& value, const QString& language)
	{
		const QJsonObject strings = value.toObject();
		const QString wanted =
			language.startsWith(kTurkish, Qt::CaseInsensitive) ? kTurkish : kEnglish;
		for (const QString& key : {wanted, kEnglish}) {
			const QString text = strings.value(key).toString();
			if (!text.isEmpty()) {
				return text;
			}
		}
		for (const QJsonValue& any : strings) {
			if (!any.toString().isEmpty()) {
				return any.toString();
			}
		}
		return QString();
	}

	QByteArray cached(const QString& path)
	{
		auto* context = LAUNCHER;
		auto entry =
			context ? context->metacache()->getEntry(kMetaBase, path) : nullptr;
		return entry ? readAll(entry->getFullPath()) : QByteArray();
	}

	NetJob::Ptr refresh(const QString& path, const QString& title,
						QObject* context,
						std::function<void(const QByteArray&)> downloaded)
	{
		auto* launcher = LAUNCHER;
		if (!launcher) {
			return nullptr;
		}
		auto entry = launcher->metacache()->resolveEntry(kMetaBase, path);
		entry->setStale(true);
		NetJob::Ptr job(new NetJob(title, launcher->network()));
		job->addNetAction(Net::Download::makeCached(
			QUrl(BuildConfig.META_URL + path), entry));
		QObject::connect(job.get(), &NetJob::succeeded, context,
						 [entry, downloaded] {
							 downloaded(readAll(entry->getFullPath()));
						 });
		QObject::connect(job.get(), &NetJob::failed, context,
						 [path](QString reason) {
							 qCDebug(metaLog) << "No newer" << path << ":" << reason;
						 });
		job->start();
		return job;
	}
} // namespace MetaCatalog
