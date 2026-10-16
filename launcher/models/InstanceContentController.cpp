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

#include "InstanceContentController.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>
#include <algorithm>

#include "FileSystem.h"
#include "minecraft/Component.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/VersionFilterData.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "models/ContentFilterModel.h"
#include "models/ContentUpdateController.h"
#include "settings/Setting.h"

namespace
{
	const QString kMods = QStringLiteral("mods");
	const QString kCoreMods = QStringLiteral("coremods");
	const QString kResourcePacks = QStringLiteral("resourcepacks");
	const QString kShaderPacks = QStringLiteral("shaderpacks");
	const QString kTexturePacks = QStringLiteral("texturepacks");
	const QString kDataPacks = QStringLiteral("datapacks");

	const QString kDataPacksEnabled = QStringLiteral("GlobalDataPacksEnabled");
	const QString kDataPacksPath = QStringLiteral("GlobalDataPacksPath");

	/* Mirrors InstanceDetails' startWatchingFresh(): a model somebody else
	 * already watches only re-arms its watcher, so ask for a fresh read. */
	void watch(const std::shared_ptr<ModFolderModel>& model)
	{
		const bool wasValid = model->isValid();
		model->startWatching();
		if (wasValid) {
			model->update();
		}
	}
} // namespace

InstanceContentController::InstanceContentController(
	MinecraftInstance* instance, QObject* parent)
	: QObject(parent), m_instance(instance)
{
	m_updater = new ContentUpdateController(m_instance, this, this);
	if (!m_instance) {
		return;
	}

	connect(m_instance, &BaseInstance::runningStatusChanged, this,
			&InstanceContentController::runningChanged);

	if (auto profile = m_instance->getPackProfile()) {
		/* A component list is a model like any other: a loader appearing
		 * or going shows up as one of these. */
		auto* p = profile.get();
		connect(p, &QAbstractItemModel::rowsInserted, this,
				&InstanceContentController::refreshHasModLoader);
		connect(p, &QAbstractItemModel::rowsRemoved, this,
				&InstanceContentController::refreshHasModLoader);
		connect(p, &QAbstractItemModel::modelReset, this,
				&InstanceContentController::refreshHasModLoader);
		connect(p, &QAbstractItemModel::dataChanged, this,
				&InstanceContentController::refreshHasModLoader);
	}
	refreshHasModLoader();

	/* The instance rebuilds its data pack model when either setting moves
	 * (MinecraftInstance's own connections, made in its constructor and
	 * so run before these), so follow along. */
	auto settings = m_instance->settings();
	for (const QString& id : {kDataPacksEnabled, kDataPacksPath}) {
		if (auto setting = settings ? settings->getSetting(id) : nullptr) {
			connect(setting.get(), &Setting::SettingChanged, this,
					&InstanceContentController::onGlobalDataPacksChanged);
		}
	}
}

InstanceContentController::~InstanceContentController()
{
	for (const auto& model : m_watched) {
		model->stopWatching();
	}
}

QStringList InstanceContentController::kinds() const
{
	QStringList list;
	if (!m_instance) {
		return list;
	}
	list << kMods;
	if (showsCoreMods()) {
		list << kCoreMods;
	}
	list << kResourcePacks << kShaderPacks;
	if (m_instance->traits().contains(QStringLiteral("texturepacks"))) {
		list << kTexturePacks;
	}
	if (globalDataPacksEnabled()) {
		list << kDataPacks;
	}
	return list;
}

bool InstanceContentController::running() const
{
	return m_instance && m_instance->isRunning();
}

QObject* InstanceContentController::updater() const
{
	return m_updater;
}

void InstanceContentController::refreshRows(const QString& kind,
											const QStringList& keys)
{
	if (auto* proxy = filterModel(kind)) {
		proxy->refreshRows(keys);
	}
}

QString InstanceContentController::configsDir() const
{
	return m_instance ? m_instance->instanceConfigFolder() : QString();
}

bool InstanceContentController::showsCoreMods() const
{
	/* Something already in the folder is shown whatever the instance is
	 * now, so it can at least be seen and removed. */
	const QDir dir(m_instance->coreModsDir());
	if (dir.exists() &&
		!dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)
			 .isEmpty()) {
		return true;
	}

	/* CoreModFolderPage::shouldDisplay(): legacy Forge only, and an
	 * unloaded Minecraft component is a "not yet" rather than a "yes". */
	auto profile = m_instance->getPackProfile();
	if (!profile || !profile->getComponent(QStringLiteral("net.minecraftforge"))) {
		return false;
	}
	Component* minecraft = profile->getComponent(QStringLiteral("net.minecraft"));
	if (!minecraft || !minecraft->m_loaded) {
		return false;
	}
	return minecraft->getReleaseDateTime() < g_VersionFilterData.legacyCutoffDate;
}

bool InstanceContentController::globalDataPacksEnabled() const
{
	return m_instance &&
		   m_instance->settings()->get(kDataPacksEnabled).toBool();
}

std::shared_ptr<ModFolderModel>
InstanceContentController::folderModel(const QString& kind) const
{
	if (!m_instance) {
		return nullptr;
	}
	if (kind == kMods) {
		return m_instance->loaderModList();
	}
	if (kind == kCoreMods) {
		return showsCoreMods() ? m_instance->coreModList() : nullptr;
	}
	if (kind == kResourcePacks) {
		return m_instance->resourcePackList();
	}
	if (kind == kShaderPacks) {
		return m_instance->shaderPackList();
	}
	if (kind == kTexturePacks) {
		return m_instance->traits().contains(QStringLiteral("texturepacks"))
				   ? m_instance->texturePackList()
				   : nullptr;
	}
	if (kind == kDataPacks) {
		// Null while the folder is switched off.
		return m_instance->dataPackList();
	}
	return nullptr;
}

ContentFilterModel* InstanceContentController::filterModel(
	const QString& kind) const
{
	return m_models.value(kind, nullptr);
}

QObject* InstanceContentController::model(const QString& kind)
{
	if (auto* existing = filterModel(kind)) {
		return existing->folderModel() ? existing : nullptr;
	}
	auto folder = folderModel(kind);
	if (!folder) {
		return nullptr;
	}

	/* InstanceDetails already watches the four folders it exposes itself;
	 * the other two are this controller's to watch. */
	if (kind == kCoreMods || kind == kDataPacks) {
		watch(folder);
		m_watched.append(folder);
	}

	auto* proxy = new ContentFilterModel(this);
	proxy->setFolderModel(folder);
	m_models.insert(kind, proxy);
	return proxy;
}

QString InstanceContentController::dir(const QString& kind) const
{
	if (!m_instance) {
		return QString();
	}
	/* Paths, not models: asking for a folder must not create it. */
	if (kind == kMods) {
		return m_instance->loaderModList()->dir().absolutePath();
	}
	if (kind == kCoreMods) {
		return m_instance->coreModsDir();
	}
	if (kind == kResourcePacks) {
		return m_instance->resourcePacksDir();
	}
	if (kind == kShaderPacks) {
		return m_instance->shaderPacksDir();
	}
	if (kind == kTexturePacks) {
		return m_instance->texturePacksDir();
	}
	if (kind == kDataPacks) {
		return globalDataPacksEnabled() ? m_instance->dataPacksDir() : QString();
	}
	return QString();
}

bool InstanceContentController::kindAllowsChangesWhileRunning(
	const QString& kind)
{
	/* ModFolderPage::allowsChangesWhileRunning(): the game holds mods open
	 * while it runs; packs are picked up again on reload. Texture packs
	 * are a pack, but their folder model refuses changes while running
	 * (MinecraftInstance::texturePackList()), so they count as locked. */
	return kind == kResourcePacks || kind == kShaderPacks || kind == kDataPacks;
}

bool InstanceContentController::changesAllowed(const QString& kind) const
{
	return !running() || kindAllowsChangesWhileRunning(kind);
}

int InstanceContentController::setEnabled(const QString& kind,
										  const QStringList& keys,
										  bool enabled)
{
	auto* proxy = filterModel(kind);
	auto* folder = proxy ? proxy->folderModel() : nullptr;
	if (!folder || !changesAllowed(kind)) {
		return 0;
	}
	int changed = 0;
	QModelIndexList indexes;
	for (const QModelIndex& index : proxy->sourceIndexesForKeys(keys)) {
		if (folder->at(size_t(index.row())).enabled() != enabled) {
			indexes.append(index);
			++changed;
		}
	}
	if (indexes.isEmpty()) {
		return 0;
	}
	if (!folder->setModStatus(indexes, enabled ? ModFolderModel::Enable
											   : ModFolderModel::Disable)) {
		return 0;
	}
	return changed;
}

int InstanceContentController::remove(const QString& kind,
									  const QStringList& keys)
{
	auto* proxy = filterModel(kind);
	auto* folder = proxy ? proxy->folderModel() : nullptr;
	if (!folder || !changesAllowed(kind)) {
		return 0;
	}
	const QModelIndexList indexes = proxy->sourceIndexesForKeys(keys);
	QStringList paths;
	for (const QModelIndex& index : indexes) {
		paths.append(folder->at(size_t(index.row())).filename().absoluteFilePath());
	}
	if (indexes.isEmpty() || !folder->deleteMods(indexes)) {
		return 0;
	}
	/* deleteMods() does not say which files actually went. */
	int removed = 0;
	for (const QString& path : paths) {
		if (!QFileInfo::exists(path)) {
			++removed;
		}
	}
	folder->update();
	return removed;
}

QString InstanceContentController::localPath(const QString& fileUrlOrPath)
{
	/* Same conversion ModFolderModel's own drop handling uses. */
	const QUrl url(fileUrlOrPath);
	return url.isLocalFile() ? url.toLocalFile() : fileUrlOrPath;
}

bool InstanceContentController::install(const QString& kind,
										const QString& fileUrlOrPath)
{
	if (!changesAllowed(kind)) {
		return false;
	}
	auto folder = folderModel(kind);
	if (!folder) {
		return false;
	}
	/* installMod() creates nothing for a folder that does not exist yet,
	 * but the coremods folder is only made on demand. */
	FS::ensureFolderPathExists(folder->dir().absolutePath());
	return folder->installMod(localPath(fileUrlOrPath));
}

ContentListExport::Item
InstanceContentController::exportItemFor(const Mod& mod,
										 const ModMetadataIndex* index)
{
	ContentListExport::Item item;
	item.name = mod.name();
	item.modId = mod.details().mod_id;
	item.fileName = mod.filename().fileName();
	item.url = ContentFilterModel::homepageFor(mod, index);
	item.authors = mod.authors();

	/* What the archive says about itself first, since that is the version
	 * a person recognises; then the number recorded at install time. The
	 * version id is a platform's own handle and reads like nothing at
	 * all, so it is the last resort. */
	item.version = mod.version();
	if (item.version.isEmpty() && index) {
		const auto entry = index->get(ContentFilterModel::keyFor(mod));
		item.version = !entry.versionNumber.isEmpty() ? entry.versionNumber
													  : entry.versionId;
	}
	return item;
}

ContentListExport::Format
InstanceContentController::formatFromString(const QString& format)
{
	const QString f = format.toLower();
	if (f == QStringLiteral("html")) {
		return ContentListExport::Format::Html;
	}
	if (f == QStringLiteral("markdown")) {
		return ContentListExport::Format::Markdown;
	}
	if (f == QStringLiteral("json")) {
		return ContentListExport::Format::Json;
	}
	if (f == QStringLiteral("csv")) {
		return ContentListExport::Format::Csv;
	}
	if (f == QStringLiteral("custom")) {
		return ContentListExport::Format::Custom;
	}
	return ContentListExport::Format::PlainText;
}

ContentListExport::Fields
InstanceContentController::fieldsFromStrings(const QStringList& fields)
{
	ContentListExport::Fields result = ContentListExport::NoFields;
	for (const QString& field : fields) {
		const QString f = field.toLower();
		if (f == QStringLiteral("version")) {
			result |= ContentListExport::Version;
		} else if (f == QStringLiteral("authors")) {
			result |= ContentListExport::Authors;
		} else if (f == QStringLiteral("url")) {
			result |= ContentListExport::Url;
		} else if (f == QStringLiteral("filename")) {
			result |= ContentListExport::FileName;
		}
	}
	return result;
}

QString InstanceContentController::renderExport(
	const QList<ContentListExport::Item>& items, const QString& format,
	const QStringList& fields, const QString& lineTemplate)
{
	const auto f = formatFromString(format);
	if (f == ContentListExport::Format::Custom) {
		return ContentListExport::render(items, lineTemplate);
	}
	return ContentListExport::render(items, f, fieldsFromStrings(fields));
}

QString InstanceContentController::exportText(const QString& kind,
											  const QStringList& keys,
											  const QString& format,
											  const QStringList& fields,
											  const QString& lineTemplate) const
{
	auto* proxy = filterModel(kind);
	auto* folder = proxy ? proxy->folderModel() : nullptr;
	if (!folder) {
		return QString();
	}
	/* Nothing selected means all of it, searched or not - the rule
	 * ModFolderPage::rowsForBulkAction() uses. A selected row the current
	 * search hides still belongs in the list. */
	const auto index = folder->metadataIndex();
	QList<ContentListExport::Item> items;
	if (keys.isEmpty()) {
		for (size_t row = 0; row < folder->size(); ++row) {
			items.append(exportItemFor(folder->at(row), index.get()));
		}
	} else {
		for (const QModelIndex& source : proxy->sourceIndexesForKeys(keys)) {
			items.append(exportItemFor(folder->at(size_t(source.row())),
									   index.get()));
		}
	}
	/* In the order the list shows them. */
	std::sort(items.begin(), items.end(),
			  [](const ContentListExport::Item& a,
				 const ContentListExport::Item& b) {
				  return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
			  });
	return renderExport(items, format, fields, lineTemplate);
}

QString InstanceContentController::exportExtension(const QString& format) const
{
	return ContentListExport::fileExtension(formatFromString(format));
}

QString InstanceContentController::exampleLine(const QString& format) const
{
	return ContentListExport::exampleLine(formatFromString(format));
}

QString InstanceContentController::suggestedExportUrl(const QString& name,
													  const QString& format) const
{
	QString base = FS::RemoveInvalidFilenameChars(name.trimmed());
	if (base.isEmpty()) {
		base = QStringLiteral("content");
	}
	const QString folder =
		QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
	return QUrl::fromLocalFile(
			   FS::PathCombine(folder, base + exportExtension(format)))
		.toString();
}

bool InstanceContentController::saveText(const QString& fileUrlOrPath,
										 const QString& text)
{
	const QString path = localPath(fileUrlOrPath);
	if (path.isEmpty()) {
		setLastError(tr("No file was chosen."));
		return false;
	}
	try {
		FS::write(path, text.toUtf8());
	} catch (const FS::FileSystemException& e) {
		qWarning() << "Could not save the exported list:" << e.cause();
		setLastError(tr("The file could not be written. Check that the "
						"folder exists and can be written to."));
		return false;
	}
	setLastError(QString());
	return true;
}

void InstanceContentController::setLastError(const QString& error)
{
	if (m_lastError == error) {
		return;
	}
	m_lastError = error;
	emit lastErrorChanged();
}

void InstanceContentController::refreshHasModLoader()
{
	auto profile = m_instance ? m_instance->getPackProfile() : nullptr;
	const bool has = profile && profile->hasModLoader();
	if (has == m_hasModLoader) {
		return;
	}
	m_hasModLoader = has;
	emit hasModLoaderChanged();
	/* Forge appearing or going can change whether coremods are shown. */
	emit kindsChanged();
}

void InstanceContentController::onGlobalDataPacksChanged()
{
	if (auto* proxy = filterModel(kDataPacks)) {
		if (auto old = proxy->folderModel()) {
			for (int i = 0; i < m_watched.size(); ++i) {
				if (m_watched.at(i).get() == old) {
					m_watched.at(i)->stopWatching();
					m_watched.removeAt(i);
					break;
				}
			}
		}
		auto folder = m_instance->dataPackList();
		if (folder) {
			watch(folder);
			m_watched.append(folder);
		}
		proxy->setFolderModel(folder);
	}
	emit kindsChanged();
}
