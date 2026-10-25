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

#include "BundleCatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QUrl>
#include <algorithm>

#include "BuildConfig.h"
#include "InstanceList.h"
#include "Logging.h"
#include "Sys.h"
#include "core/LauncherContext.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/PresetDefaults.h"
#include "minecraft/mod/ModFolderModel.h"
#include "models/ContentInstallReview.h"
#include "net/Download.h"
#include "net/HttpMetaCache.h"
#include "settings/SettingsObject.h"
#include "translations/TranslationsModel.h"

namespace
{
	constexpr int kFormatVersion = 1;
	const QString kEmbeddedCatalog = QStringLiteral(":/kite/bundles.json");
	const QString kCatalogPath = QStringLiteral("kite/bundles.json");
	const QString kTurkish = QStringLiteral("tr");
	const QString kEnglish = QStringLiteral("en");
	const QString kPerformanceBundle = QStringLiteral("performance");

	bool hasText(const QJsonObject& object, const QString& key)
	{
		return !object.value(key).toString().isEmpty();
	}

	/* What is wrong with @p root, or "" when it is a catalog. */
	QString problemWith(const QJsonObject& root)
	{
		if (root.value("formatVersion").toInt() != kFormatVersion) {
			return QStringLiteral("Unsupported catalog format.");
		}
		if (!root.value("bundles").isArray() ||
			!root.value("presets").isArray()) {
			return QStringLiteral("The catalog has no bundles or presets.");
		}
		QSet<QString> bundleIds;
		for (const QJsonValue& bundleValue : root.value("bundles").toArray()) {
			const QJsonObject bundle = bundleValue.toObject();
			if (!hasText(bundle, "id")) {
				return QStringLiteral("A bundle has no id.");
			}
			bundleIds.insert(bundle.value("id").toString());
			for (const QJsonValue& modValue : bundle.value("mods").toArray()) {
				const QJsonObject mod = modValue.toObject();
				if (!hasText(mod, "project") || !hasText(mod, "name")) {
					return QStringLiteral("A mod needs a project and a name.");
				}
			}
		}
		for (const QJsonValue& presetValue : root.value("presets").toArray()) {
			const QJsonObject preset = presetValue.toObject();
			if (!hasText(preset, "id")) {
				return QStringLiteral("A preset has no id.");
			}
			for (const QJsonValue& bundleId : preset.value("bundles").toArray()) {
				if (!bundleIds.contains(bundleId.toString())) {
					return QStringLiteral("A preset names a missing bundle.");
				}
			}
		}
		return QString();
	}

	/* A mod with no `loaders` list is offered everywhere. */
	bool offeredFor(const QJsonObject& mod, const QString& loader)
	{
		const QJsonValue loaders = mod.value("loaders");
		if (!loaders.isArray()) {
			return true;
		}
		return loaders.toArray().contains(loader);
	}

	std::shared_ptr<MinecraftInstance> minecraftInstance(const QString& id)
	{
		auto* context = LAUNCHER;
		if (!context || !context->instances()) {
			return nullptr;
		}
		return std::dynamic_pointer_cast<MinecraftInstance>(
			context->instances()->getInstanceById(id));
	}
} // namespace

BundleCatalog::BundleCatalog(QObject* parent) : QObject(parent)
{
	QFile embedded(kEmbeddedCatalog);
	QString error;
	if (!embedded.open(QIODevice::ReadOnly) ||
		!loadFromJson(embedded.readAll(), &error)) {
		qCWarning(metaLog) << "The built-in bundle catalog is unusable:" << error;
	}
}

bool BundleCatalog::loadFromJson(const QByteArray& json, QString* error)
{
	QJsonParseError parseError;
	const auto document = QJsonDocument::fromJson(json, &parseError);
	QString problem = parseError.error == QJsonParseError::NoError
						  ? problemWith(document.object())
						  : parseError.errorString();
	if (!problem.isEmpty()) {
		if (error) {
			*error = problem;
		}
		return false;
	}
	m_catalog = document.object();
	emit changed();
	return true;
}

void BundleCatalog::setLanguage(const QString& language)
{
	m_language = language;
}

QString BundleCatalog::language() const
{
	if (!m_language.isNull()) {
		return m_language;
	}
	auto* context = LAUNCHER;
	auto translations = context ? context->translations() : nullptr;
	return translations ? translations->selectedLanguage() : QString();
}

QString BundleCatalog::localized(const QJsonValue& value) const
{
	const QJsonObject strings = value.toObject();
	const QString wanted =
		language().startsWith(kTurkish, Qt::CaseInsensitive) ? kTurkish : kEnglish;
	for (const QString& key : {wanted, kEnglish}) {
		if (hasText(strings, key)) {
			return strings.value(key).toString();
		}
	}
	for (const QJsonValue& any : strings) {
		if (!any.toString().isEmpty()) {
			return any.toString();
		}
	}
	return QString();
}

QJsonObject BundleCatalog::presetById(const QString& presetId) const
{
	for (const QJsonValue& preset : m_catalog.value("presets").toArray()) {
		if (preset.toObject().value("id").toString() == presetId) {
			return preset.toObject();
		}
	}
	return {};
}

QVariantList BundleCatalog::presets() const
{
	QVariantList result;
	for (const QJsonValue& value : m_catalog.value("presets").toArray()) {
		const QJsonObject preset = value.toObject();
		result.append(QVariantMap{
			{"id", preset.value("id").toString()},
			{"name", localized(preset.value("name"))},
			{"summary", localized(preset.value("summary"))},
			{"minecraft", preset.value("minecraft").toString()},
			{"loader", preset.value("loader").toString()},
			{"bundles", preset.value("bundles").toVariant().toStringList()},
			{"options", preset.value("options").toString()},
		});
	}
	return result;
}

QVariantList BundleCatalog::mods(const QStringList& bundleIds,
								 const QString& loader) const
{
	QVariantList result;
	/* Vanilla has nothing to load a mod with. */
	if (loader.isEmpty()) {
		return result;
	}
	QSet<QString> listed;
	for (const QJsonValue& bundleValue : m_catalog.value("bundles").toArray()) {
		const QJsonObject bundle = bundleValue.toObject();
		if (!bundleIds.contains(bundle.value("id").toString())) {
			continue;
		}
		QHash<QString, QString> categoryNames;
		for (const QJsonValue& category : bundle.value("categories").toArray()) {
			categoryNames.insert(category.toObject().value("id").toString(),
								 localized(category.toObject().value("name")));
		}
		for (const QJsonValue& modValue : bundle.value("mods").toArray()) {
			const QJsonObject mod = modValue.toObject();
			const QString project = mod.value("project").toString();
			if (listed.contains(project) || !offeredFor(mod, loader)) {
				continue;
			}
			listed.insert(project);
			const QString category = mod.value("category").toString();
			result.append(QVariantMap{
				{"project", project},
				{"name", mod.value("name").toString()},
				{"summary", localized(mod.value("summary"))},
				{"bundle", bundle.value("id").toString()},
				{"bundleName", localized(bundle.value("name"))},
				{"category", category},
				{"categoryName", categoryNames.value(category)},
				{"isDefault", mod.value("default").toBool()},
				{"serverSafe", mod.value("serverSafe").toBool(true)},
				{"icon", mod.value("icon").toString()},
				{"requires", mod.value("requires").toVariant().toStringList()},
			});
		}
	}
	return result;
}

QObject* BundleCatalog::preview(const QStringList& projectIds,
								const QString& mcVersion, const QString& loader)
{
	if (projectIds.isEmpty()) {
		return nullptr;
	}
	QList<DependencyResolver::WantedProject> wanted;
	for (const QString& id : projectIds) {
		wanted.append({id, {}});
	}
	auto* review = ContentInstallReview::missingDependencies(
		{}, tr("Recommended mods"), mcVersion, loader, nullptr, this);
	review->setWantedProjects(wanted);
	review->start();
	return review;
}

QObject* BundleCatalog::installInto(QObject* review, const QString& instanceId)
{
	auto* installReview = qobject_cast<ContentInstallReview*>(review);
	auto instance = minecraftInstance(instanceId);
	auto folder = instance ? instance->loaderModList() : nullptr;
	if (!installReview || !folder) {
		return nullptr;
	}
	installReview->setFolder(folder);
	return installReview->install();
}

bool BundleCatalog::applyPresetDefaults(const QString& instanceId,
										const QString& presetId)
{
	const QJsonObject preset = presetById(presetId);
	auto instance = minecraftInstance(instanceId);
	if (preset.isEmpty() || !instance) {
		return false;
	}

	/* The global minimum stays unless the preset's maximum is lower. */
	const int maxMemory =
		PresetDefaults::maxMemoryMiB(Sys::getSystemRam() / Sys::mebibyte);
	const int minMemory = std::min(
		LAUNCHER->settings()->get("MinMemAlloc").toInt(), maxMemory);
	auto settings = instance->settings();
	settings->set("OverrideMemory", true);
	settings->set("MaxMemAlloc", maxMemory);
	settings->set("MinMemAlloc", minMemory);

	const QString mcVersion =
		instance->getPackProfile()->getComponentVersion("net.minecraft");
	const bool legacy = PresetDefaults::usesLegacyOptions(mcVersion);
	const QJsonObject keys =
		m_catalog.value("options")
			.toObject()
			.value(preset.value("options").toString())
			.toObject()
			.value(legacy ? "legacy" : "modern")
			.toObject();
	QMap<QString, QString> options;
	for (auto it = keys.begin(); it != keys.end(); ++it) {
		options.insert(it.key(), it.value().toVariant().toString());
	}
	PresetDefaults::writeOptions(instance->gameRoot(), options);

	if (preset.value("bundles").toArray().contains(kPerformanceBundle)) {
		settings->set("KitePerformance", true);
	}
	return true;
}

bool BundleCatalog::loadCached()
{
	auto* context = LAUNCHER;
	auto entry =
		context ? context->metacache()->getEntry("general", kCatalogPath) : nullptr;
	QFile file(entry ? entry->getFullPath() : QString());
	QString error;
	if (!entry || !file.open(QIODevice::ReadOnly)) {
		return false;
	}
	if (!loadFromJson(file.readAll(), &error)) {
		qCWarning(metaLog) << "Ignoring the cached bundle catalog:" << error;
		return false;
	}
	return true;
}

void BundleCatalog::refresh()
{
	auto* context = LAUNCHER;
	if (m_refreshed || !context) {
		return;
	}
	m_refreshed = true;

	auto entry = context->metacache()->resolveEntry("general", kCatalogPath);
	/* Asked again every run, so a new catalog shows up without waiting. */
	entry->setStale(true);
	m_refreshJob.reset(new NetJob(tr("Recommended mods"), context->network()));
	m_refreshJob->addNetAction(Net::Download::makeCached(
		QUrl(BuildConfig.META_URL + kCatalogPath), entry));
	connect(m_refreshJob.get(), &NetJob::succeeded, this,
			[this, entry] { onRefreshed(entry->getFullPath()); });
	/* The server may not have a catalog yet; the built-in one stays. */
	connect(m_refreshJob.get(), &NetJob::failed, this, [](QString reason) {
		qCDebug(metaLog) << "No newer bundle catalog:" << reason;
	});
	m_refreshJob->start();
}

void BundleCatalog::onRefreshed(const QString& path)
{
	QFile file(path);
	QString error;
	if (!file.open(QIODevice::ReadOnly) ||
		!loadFromJson(file.readAll(), &error)) {
		qCWarning(metaLog) << "Ignoring the downloaded bundle catalog:" << error;
	}
}
