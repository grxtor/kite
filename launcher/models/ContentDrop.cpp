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

#include "ContentDrop.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>

#include "FileSystem.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/WorldList.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "models/ContentLinkInstall.h"
#include "models/InstanceContentController.h"
#include "models/ModHealthModel.h"
#include "models/WorldCopyTask.h"
#include "models/WorldTools.h"
#include "tasks/TaskWatcher.h"

namespace
{
	const QString kMods = QStringLiteral("mods");
	const QString kResourcePacks = QStringLiteral("resourcepacks");
	const QString kShaderPacks = QStringLiteral("shaderpacks");

	bool isWebLink(const QString& text)
	{
		const QUrl url(text);
		return url.scheme() == QLatin1String("http") ||
			   url.scheme() == QLatin1String("https");
	}

	/* "name.jar" -> "name (2).jar" */
	QString keptName(const QString& fileName)
	{
		const QFileInfo info(fileName);
		const QString suffix = info.suffix();
		return info.completeBaseName() + QStringLiteral(" (2)") +
			   (suffix.isEmpty() ? QString() : QLatin1Char('.') + suffix);
	}

	QString withoutDisabled(QString fileName)
	{
		if (fileName.endsWith(QLatin1String(".disabled"))) {
			fileName.chop(int(QString(".disabled").size()));
		}
		return fileName;
	}
} // namespace

ContentDrop::ContentDrop(InstancePtr instance, QObject* parent)
	: QObject(parent), m_instance(std::move(instance))
{
	m_mc = dynamic_cast<MinecraftInstance*>(m_instance.get());
}

ContentDrop::~ContentDrop() = default;

std::shared_ptr<ModFolderModel> ContentDrop::folderFor(const QString& kind) const
{
	if (!m_mc) {
		return nullptr;
	}
	if (kind == kMods) {
		return m_mc->loaderModList();
	}
	if (kind == kResourcePacks) {
		return m_mc->resourcePackList();
	}
	if (kind == kShaderPacks) {
		return m_mc->shaderPackList();
	}
	return nullptr;
}

bool ContentDrop::changesAllowed(const QString& kind) const
{
	return m_mc && (!m_mc->isRunning() ||
					InstanceContentController::kindAllowsChangesWhileRunning(kind));
}

QString ContentDrop::describe(const QStringList& urls) const
{
	DroppedContent::Counts counts;
	for (const QString& text : urls) {
		if (isWebLink(text)) {
			const auto link = DroppedContent::parseProjectLink(QUrl(text));
			if (link.valid()) {
				counts.add(link.contentType == kMods
							   ? DroppedContent::Kind::Mod
						   : link.contentType == kResourcePacks
							   ? DroppedContent::Kind::ResourcePack
						   : link.contentType == kShaderPacks
							   ? DroppedContent::Kind::ShaderPack
							   : DroppedContent::Kind::DataPack);
			}
			continue;
		}
		counts.add(DroppedContent::classify(text).kind);
	}
	return DroppedContent::describe(counts);
}

QVariantList ContentDrop::worldChoices() const
{
	QVariantList choices;
	if (!m_mc) {
		return choices;
	}
	WorldList worlds(m_mc->worldDir());
	worlds.update();
	for (const World& world : worlds.allWorlds()) {
		QVariantMap choice;
		choice.insert(QStringLiteral("folder"), world.folderName());
		choice.insert(QStringLiteral("name"), world.name().isEmpty()
												  ? world.folderName()
												  : world.name());
		choices.append(choice);
	}
	return choices;
}

QVariantMap ContentDrop::prepare(const QStringList& urls)
{
	cancel();
	QVariantMap result;
	if (!m_mc) {
		result.insert(QStringLiteral("empty"), true);
		return result;
	}

	const auto profile = m_mc->getPackProfile();
	DroppedContent::Target target;
	target.environment = ModHealthModel::environmentFor(profile.get());
	const auto dirOf = [this](const QString& kind) {
		const auto folder = folderFor(kind);
		return folder ? folder->dir().absolutePath() : QString();
	};
	target.modsDir = dirOf(kMods);
	target.resourcePacksDir = dirOf(kResourcePacks);
	target.shaderPacksDir = dirOf(kShaderPacks);

	const DroppedContent::Plan found = DroppedContent::plan(urls, target);
	m_pending = found.entries;
	m_unknown = found.unknown;

	bool hasDataPack = false;
	bool hasMod = false;
	QVariantList conflicts;
	for (const Pending& pending : m_pending) {
		hasDataPack = hasDataPack ||
					  pending.item.kind == DroppedContent::Kind::DataPack;
		hasMod = hasMod || pending.item.kind == DroppedContent::Kind::Mod;
		if (!pending.sameMod.isEmpty()) {
			QVariantMap conflict;
			conflict.insert(QStringLiteral("name"), pending.item.name);
			conflict.insert(QStringLiteral("existing"),
							withoutDisabled(pending.sameMod));
			conflicts.append(conflict);
		}
	}

	result.insert(QStringLiteral("counts"),
				  DroppedContent::describe(found.counts));
	result.insert(QStringLiteral("modpack"), found.modpacks.value(0));
	result.insert(QStringLiteral("extraModpacks"),
				  qMax(0, int(found.modpacks.size()) - 1));
	result.insert(QStringLiteral("links"), found.projectLinks);
	result.insert(QStringLiteral("otherLinks"), found.otherLinks);
	result.insert(QStringLiteral("unknown"), m_unknown);
	result.insert(QStringLiteral("needsWorld"), hasDataPack);
	result.insert(QStringLiteral("worldChoices"),
				  hasDataPack ? worldChoices() : QVariantList());
	result.insert(QStringLiteral("hasWorld"), found.counts.worlds > 0);
	result.insert(QStringLiteral("conflicts"), conflicts);
	result.insert(QStringLiteral("needsLoader"),
				  hasMod && profile && profile->getModLoaders().isEmpty());
	result.insert(QStringLiteral("running"), m_mc->isRunning());
	result.insert(QStringLiteral("empty"), m_pending.isEmpty());
	return result;
}

void ContentDrop::chooseWorld(const QString& folderName)
{
	m_world = folderName;
}

void ContentDrop::resolveConflicts(const QString& mode)
{
	m_replace = mode != QLatin1String("keep");
}

void ContentDrop::cancel()
{
	m_pending.clear();
	m_unknown.clear();
	m_world.clear();
	m_replace = true;
}

QString ContentDrop::dataPackDir() const
{
	if (!m_mc || m_world.isEmpty() || m_world.contains(QLatin1Char('/')) ||
		m_world.contains(QLatin1Char('\\')) || m_world == QLatin1String("..")) {
		return QString();
	}
	return FS::PathCombine(m_mc->worldDir(), m_world, QStringLiteral("datapacks"));
}

bool ContentDrop::installContent(const Pending& pending, const QString& kind,
								 DroppedContent::Outcome& outcome)
{
	const DroppedContent::Item& item = pending.item;
	const auto folder = folderFor(kind);
	if (!folder || !changesAllowed(kind)) {
		outcome.locked << item.name;
		return false;
	}
	if (!pending.identical.isEmpty()) {
		++outcome.identical;
		return false;
	}

	QString source = item.path;
	std::unique_ptr<QTemporaryDir> renamed;
	if (!pending.sameMod.isEmpty()) {
		if (m_replace) {
			const QString old = folder->dir().filePath(pending.sameMod);
			if (QFile::remove(old)) {
				if (const auto index = folder->metadataIndex()) {
					index->remove(withoutDisabled(pending.sameMod));
				}
				++outcome.replaced;
			}
		} else if (withoutDisabled(pending.sameMod) == item.fileName) {
			// Same file name, other contents: keep the old one under its name.
			renamed = std::make_unique<QTemporaryDir>();
			const QString copy = renamed->filePath(keptName(item.fileName));
			if (!renamed->isValid() || !QFile::copy(item.path, copy)) {
				outcome.failed << item.name;
				return false;
			}
			source = copy;
		}
	}

	if (!folder->installMod(source)) {
		outcome.failed << item.name;
		return false;
	}
	outcome.added.add(item.kind);
	if (!pending.warning.isEmpty()) {
		outcome.warnings << pending.warning;
	}
	return true;
}

bool ContentDrop::installDataPack(const Pending& pending,
								  DroppedContent::Outcome& outcome)
{
	const DroppedContent::Item& item = pending.item;
	const QString dir = dataPackDir();
	if (dir.isEmpty()) {
		outcome.failed << item.name;
		return false;
	}
	if (m_mc->isRunning()) {
		outcome.locked << item.name;
		return false;
	}
	if (!FS::ensureFolderPathExists(dir)) {
		outcome.failed << item.name;
		return false;
	}
	if (!DroppedContent::identicalFileIn(dir, item).isEmpty()) {
		++outcome.identical;
		return false;
	}
	const QString target = QDir(dir).filePath(item.fileName);
	const bool copied =
		item.isFolder ? FS::copy(item.path, target)()
					  : (QFile::remove(target) || !QFile::exists(target)) &&
							QFile::copy(item.path, target);
	if (!copied) {
		outcome.failed << item.name;
		return false;
	}
	outcome.added.add(item.kind);
	return true;
}

QObject* ContentDrop::startWorldInstall(const QStringList& paths)
{
	const QString to = m_mc->worldDir();
	auto* task = new WorldCopyTask(
		tr("Adding world…"),
		[paths, to]() {
			bool all = true;
			for (const QString& path : paths) {
				all = WorldTools::installWorld(path, to) && all;
			}
			return all;
		});
	auto* watcher = new TaskWatcher(Task::Ptr(task), this);
	watcher->setTitle(tr("Add world"));
	task->start();
	return watcher;
}

QVariantMap ContentDrop::commit()
{
	DroppedContent::Outcome outcome;
	outcome.unknown = m_unknown;
	QStringList worldPaths;

	for (const Pending& pending : m_pending) {
		switch (pending.item.kind) {
			case DroppedContent::Kind::World:
				worldPaths << pending.item.path;
				break;
			case DroppedContent::Kind::DataPack:
				installDataPack(pending, outcome);
				break;
			default:
				installContent(pending,
							   DroppedContent::contentKind(pending.item.kind),
							   outcome);
		}
	}

	QObject* worldTask = nullptr;
	if (!worldPaths.isEmpty() && m_mc) {
		worldTask = startWorldInstall(worldPaths);
		for (int i = 0; i < worldPaths.size(); ++i) {
			outcome.added.add(DroppedContent::Kind::World);
		}
	}

	QVariantMap result;
	QString tone;
	result.insert(QStringLiteral("text"),
				  DroppedContent::summarize(outcome, &tone));
	result.insert(QStringLiteral("tone"), tone);
	result.insert(QStringLiteral("worldTask"), QVariant::fromValue(worldTask));
	result.insert(QStringLiteral("worlds"), int(worldPaths.size()));
	cancel();
	return result;
}

QObject* ContentDrop::installLink(const QString& url)
{
	const auto link = DroppedContent::parseProjectLink(QUrl(url));
	if (!m_mc || !link.valid()) {
		return nullptr;
	}
	auto* install = new ContentLinkInstall(m_mc, link, this);
	install->start();
	return install;
}
