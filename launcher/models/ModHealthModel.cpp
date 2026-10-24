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

#include "ModHealthModel.h"

#include <QPair>
#include <QSet>
#include <QVariantMap>

#include "minecraft/PackProfile.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/VersionRange.h"
#include "models/ContentFilterModel.h"
#include "models/ModHealthModelIds.h"

using namespace ModHealthModelIds;

namespace
{
	const QString kMinecraftUid = QStringLiteral("net.minecraft");
	const QString kFabricUid = QStringLiteral("net.fabricmc.fabric-loader");
	const QString kQuiltUid = QStringLiteral("org.quiltmc.quilt-loader");
	const QString kForgeUid = QStringLiteral("net.minecraftforge");
	const QString kNeoForgeUid = QStringLiteral("net.neoforged");

	/* NeoForge's 1.20.1 line still declares itself as "forge". */
	const QString kNeoForgeAsForgeMinecraft = QStringLiteral("1.20.1");
} // namespace

ModHealthModel::ModHealthModel(QObject* parent) : QAbstractListModel(parent) {}

int ModHealthModel::blockingCount() const
{
	int blocking = 0;
	for (const Row& row : m_rows) {
		if (row.issue.blocking) {
			++blocking;
		}
	}
	return blocking;
}

int ModHealthModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant ModHealthModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
		return {};
	}
	const Row& row = m_rows.at(index.row());
	switch (role) {
	case KeyRole:
		return row.issue.key;
	case NameRole:
		return row.issue.name;
	case BlockingRole:
		return row.issue.blocking;
	case TitleRole:
		return row.title;
	case DetailRole:
		return row.detail;
	case ActionRole:
		return row.action;
	case ActionKeyRole:
		return row.actionKey;
	case ActionLabelRole:
		return row.actionLabel;
	case DependencyIdRole:
		return row.issue.otherId;
	case ReplacesKeyRole:
		return row.replacesKey;
	default:
		return {};
	}
}

QHash<int, QByteArray> ModHealthModel::roleNames() const
{
	return {
		{KeyRole, "key"},
		{NameRole, "name"},
		{BlockingRole, "blocking"},
		{TitleRole, "title"},
		{DetailRole, "detail"},
		{ActionRole, "action"},
		{ActionKeyRole, "actionKey"},
		{ActionLabelRole, "actionLabel"},
		{DependencyIdRole, "dependencyId"},
		{ReplacesKeyRole, "replacesKey"},
	};
}

void ModHealthModel::setIssues(const QList<ModHealth::Issue>& issues)
{
	beginResetModel();
	m_all.clear();
	m_rows.clear();

	/* The pairs of files already given a Duplicate row, in the order they
	 * were met: the second copy's issue is the same row seen from the
	 * other side. */
	QSet<QPair<QString, QString>> pairs;
	for (ModHealth::Issue issue : issues) {
		/* The game's version is the environment's to say. */
		if (issue.kind == ModHealth::Kind::WrongMinecraft &&
			issue.foundVersion.isEmpty()) {
			issue.foundVersion = m_environment.minecraft;
		}
		m_all.append(issue);

		if (issue.kind == ModHealth::Kind::Duplicate &&
			!issue.otherKey.isEmpty()) {
			if (pairs.contains({issue.otherKey, issue.key})) {
				continue;
			}
			pairs.insert({issue.key, issue.otherKey});
		}

		Row row;
		row.issue = issue;
		row.title = titleOf(issue);
		row.detail = detailFor(issue, m_environment);
		row.action = actionFor(issue, row.actionKey, row.actionLabel,
							   row.replacesKey);
		m_rows.append(row);
	}

	endResetModel();
	++m_revision;
	emit changed();
}

QStringList ModHealthModel::problemsFor(const QString& key) const
{
	QStringList blocking;
	QStringList warnings;
	for (const ModHealth::Issue& issue : m_all) {
		if (issue.key != key) {
			continue;
		}
		(issue.blocking ? blocking : warnings).append(titleOf(issue));
	}
	return blocking + warnings;
}

bool ModHealthModel::blockingFor(const QString& key) const
{
	for (const ModHealth::Issue& issue : m_all) {
		if (issue.blocking && issue.key == key) {
			return true;
		}
	}
	return false;
}

QVariantList ModHealthModel::installRows() const
{
	QVariantList rows;
	for (const Row& row : m_rows) {
		if (row.action == kActionInstall) {
			rows.append(QVariantMap{
				{QStringLiteral("key"), row.issue.key},
				{QStringLiteral("dependencyId"), row.issue.otherId},
				{QStringLiteral("replacesKey"), row.replacesKey}});
		}
	}
	return rows;
}

QList<ModHealth::Entry> ModHealthModel::entriesFor(const QList<Mod>& mods)
{
	QList<ModHealth::Entry> entries;
	entries.reserve(mods.size());
	for (const Mod& mod : mods) {
		ModHealth::Entry entry;
		entry.key = ContentFilterModel::keyFor(mod);
		entry.name = mod.name().isEmpty() ? mod.filename().fileName()
										  : mod.name();
		entry.enabled = mod.enabled();
		entry.details = mod.details();
		entries.append(entry);
	}
	return entries;
}

ModHealth::Environment ModHealthModel::environmentFor(PackProfile* profile)
{
	ModHealth::Environment environment;
	if (!profile) {
		return environment;
	}

	environment.minecraft = profile->getComponentVersion(kMinecraftUid);
	environment.builtIns.insert(kMinecraftId, environment.minecraft);
	environment.loaders = profile->getModLoaders();

	for (const QString& loader : environment.loaders) {
		if (loader == QStringLiteral("fabric")) {
			environment.builtIns.insert(
				kFabricLoaderId, profile->getComponentVersion(kFabricUid));
		} else if (loader == QStringLiteral("quilt")) {
			environment.builtIns.insert(
				kQuiltLoaderId, profile->getComponentVersion(kQuiltUid));
			/* Quilt answers to Fabric's id, but has no Fabric version to
			 * give; a real Fabric loader's version stands. */
			if (!environment.builtIns.contains(kFabricLoaderId)) {
				environment.builtIns.insert(kFabricLoaderId, QString());
			}
		} else if (loader == kForgeId) {
			environment.builtIns.insert(
				kForgeId, profile->getComponentVersion(kForgeUid));
		} else if (loader == kNeoForgeId) {
			const QString version = profile->getComponentVersion(kNeoForgeUid);
			environment.builtIns.insert(kNeoForgeId, version);
			const auto order = VersionRange::compare(
				environment.minecraft, kNeoForgeAsForgeMinecraft);
			if (order && *order == 0 && !environment.builtIns.contains(kForgeId)) {
				environment.builtIns.insert(kForgeId, version);
			}
		}
	}
	return environment;
}
