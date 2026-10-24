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

#include <QLocale>
#include <QStringList>

#include "minecraft/Component.h"
#include "minecraft/mod/VersionRange.h"
#include "models/ModHealthModelIds.h"

using namespace ModHealthModelIds;

namespace
{
	/* What the other end of an issue is called: the installed file's name,
	 * or for the game and its loaders what a person calls them. */
	QString otherLabel(const ModHealth::Issue& issue)
	{
		if (!issue.otherKey.isEmpty()) {
			return issue.otherName.isEmpty() ? issue.otherKey
											 : issue.otherName;
		}
		return ModHealthModel::builtInName(
			issue.otherName.isEmpty() ? issue.otherId : issue.otherName);
	}

	QString loaderNames(const QStringList& platformIds)
	{
		QStringList names;
		for (const QString& id : platformIds) {
			names.append(ModHealthModel::loaderName(id));
		}
		return QLocale().createSeparatedList(names);
	}
} // namespace

QString ModHealthModel::builtInName(const QString& id)
{
	const QString lower = id.toLower();
	if (lower == kMinecraftId) {
		return tr("Minecraft");
	}
	if (lower == kFabricLoaderId) {
		return tr("Fabric Loader");
	}
	if (lower == kQuiltLoaderId) {
		return tr("Quilt Loader");
	}
	if (lower == kForgeId) {
		return tr("Forge");
	}
	if (lower == kNeoForgeId) {
		return tr("NeoForge");
	}
	return id;
}

QString ModHealthModel::loaderName(const QString& platformId)
{
	for (const ModLoaderInfo& loader : knownModLoaders()) {
		if (loader.platformId == platformId) {
			return loader.brandName;
		}
	}
	/* LiteLoader has no platform id of its own in the table. */
	if (platformId == QStringLiteral("liteloader")) {
		return QStringLiteral("LiteLoader");
	}
	return platformId;
}

QString ModHealthModel::titleFor(const ModHealth::Issue& issue)
{
	switch (issue.kind) {
	case ModHealth::Kind::MissingDependency:
		return tr("“%1” needs %2").arg(issue.name, otherLabel(issue));
	case ModHealth::Kind::DisabledDependency:
		return tr("“%1” needs “%2”, which is turned off")
			.arg(issue.name, otherLabel(issue));
	case ModHealth::Kind::WrongDependencyVersion:
		return tr("“%1” needs a different version of “%2”")
			.arg(issue.name, otherLabel(issue));
	case ModHealth::Kind::WrongMinecraft:
		return issue.foundVersion.isEmpty()
				   ? tr("“%1” is not made for this version of Minecraft")
						 .arg(issue.name)
				   : tr("“%1” is not made for Minecraft %2")
						 .arg(issue.name, issue.foundVersion);
	case ModHealth::Kind::WrongLoaderVersion:
		return tr("“%1” needs a different %2 version")
			.arg(issue.name, otherLabel(issue));
	case ModHealth::Kind::Incompatible:
		return tr("“%1” does not work with “%2”")
			.arg(issue.name, otherLabel(issue));
	case ModHealth::Kind::Discouraged:
		return tr("“%1” may not work well with “%2”")
			.arg(issue.name, otherLabel(issue));
	case ModHealth::Kind::Duplicate:
		return tr("“%1” is installed twice").arg(issue.name);
	case ModHealth::Kind::WrongLoader:
		return tr("“%1” is made for another mod loader").arg(issue.name);
	}
	return {};
}

QString ModHealthModel::titleOf(const ModHealth::Issue& issue) const
{
	if (issue.kind == ModHealth::Kind::WrongLoader) {
		const QStringList loaders = m_fileLoaders.value(issue.key);
		if (!loaders.isEmpty()) {
			return tr("“%1” is made for %2")
				.arg(issue.name, loaderNames(loaders));
		}
	}
	return titleFor(issue);
}

QString ModHealthModel::detailFor(const ModHealth::Issue& issue,
								  const ModHealth::Environment& environment)
{
	const QString asked = VersionRange::describe(issue.dependency);
	switch (issue.kind) {
	case ModHealth::Kind::MissingDependency:
		return asked.isEmpty()
				   ? tr("It is not installed.")
				   : tr("Version %1 is needed, and it is not installed.")
						 .arg(asked);
	case ModHealth::Kind::DisabledDependency:
		return tr("The game will not start until it is turned on.");
	case ModHealth::Kind::WrongDependencyVersion:
		return issue.foundVersion.isEmpty()
				   ? tr("It asks for %1.").arg(asked)
				   : tr("It asks for %1; %2 is installed.")
						 .arg(asked, issue.foundVersion);
	case ModHealth::Kind::WrongMinecraft:
		return tr("It asks for Minecraft %1.").arg(asked);
	case ModHealth::Kind::WrongLoaderVersion:
		return issue.foundVersion.isEmpty()
				   ? tr("It asks for %1.").arg(asked)
				   : tr("It asks for %1; this instance has %2.")
						 .arg(asked, issue.foundVersion);
	case ModHealth::Kind::Incompatible:
		return tr("The game will not start with both turned on.");
	case ModHealth::Kind::Discouraged:
		return tr("The game starts, but its author advises against using "
				  "both.");
	case ModHealth::Kind::Duplicate:
		return issue.otherKey.isEmpty()
				   ? tr("Another file is the same mod.")
				   : tr("%1 and %2 are the same mod.")
						 .arg(issue.key, issue.otherKey);
	case ModHealth::Kind::WrongLoader:
		return environment.loaders.isEmpty()
				   ? tr("This instance's loader does not load it.")
				   : tr("This instance runs %1, so it is not loaded.")
						 .arg(loaderNames(environment.loaders));
	}
	return {};
}

QString ModHealthModel::actionFor(const ModHealth::Issue& issue,
								  QString& actionKey, QString& actionLabel,
								  QString& replacesKey)
{
	actionKey.clear();
	actionLabel.clear();
	replacesKey.clear();
	switch (issue.kind) {
	case ModHealth::Kind::MissingDependency:
		actionLabel = tr("Install");
		return kActionInstall;
	case ModHealth::Kind::DisabledDependency:
		actionKey = issue.otherKey;
		actionLabel = tr("Turn on “%1”").arg(otherLabel(issue));
		return kActionEnable;
	case ModHealth::Kind::WrongDependencyVersion:
		replacesKey = issue.otherKey;
		actionLabel = tr("Update");
		return kActionInstall;
	case ModHealth::Kind::WrongMinecraft:
	case ModHealth::Kind::WrongLoader:
		actionKey = issue.key;
		actionLabel = tr("Turn off “%1”").arg(issue.name);
		return kActionDisable;
	case ModHealth::Kind::WrongLoaderVersion:
		return QString();
	case ModHealth::Kind::Incompatible:
	case ModHealth::Kind::Discouraged:
		/* Against the game or its loader there is no file to turn off but
		 * the mod's own. */
		if (issue.otherKey.isEmpty()) {
			actionKey = issue.key;
			actionLabel = tr("Turn off “%1”").arg(issue.name);
		} else {
			actionKey = issue.otherKey;
			actionLabel = tr("Turn off “%1”").arg(otherLabel(issue));
		}
		return kActionDisable;
	case ModHealth::Kind::Duplicate:
		actionKey = issue.key;
		actionLabel = tr("Turn off %1").arg(issue.key);
		return kActionDisable;
	}
	return QString();
}
