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

#include <QJsonArray>
#include <QJsonDocument>

#include <QDebug>

#include "minecraft/VersionFile.h"
#include "minecraft/Library.h"
#include "minecraft/PackProfile.h"
#include "ParseUtils.h"

#include <Version.h>

static bool isMinecraftVersion(const QString& uid)
{
	return uid == "net.minecraft";
}

void VersionFile::applyTo(LaunchProfile* profile,
						  const RuntimeContext& context)
{
	// Only real Minecraft can set those. Don't let anything override them.
	if (isMinecraftVersion(uid)) {
		profile->applyMinecraftVersion(minecraftVersion);
		profile->applyMinecraftVersionType(type);
		// HACK: ignore assets from other version files than Minecraft
		// workaround for stupid assets issue caused by amazon:
		// https://www.theregister.co.uk/2017/02/28/aws_is_awol_as_s3_goes_haywire/
		profile->applyMinecraftAssets(mojangAssetIndex);
	}

	profile->applyMainJar(mainJar);
	profile->applyMainClass(mainClass);
	profile->applyAppletClass(appletClass);
	profile->applyMinecraftArguments(minecraftArguments);
	profile->applyTweakers(addTweakers);
	profile->applyJarMods(jarMods);
	profile->applyMods(mods);
	profile->applyTraits(traits);

	for (auto library : libraries) {
		profile->applyLibrary(library, context);
	}
	for (auto mavenFile : mavenFiles) {
		profile->applyMavenFile(mavenFile, context);
	}
	profile->applyProblemSeverity(getProblemSeverity());
}

/*
	auto theirVersion = profile->getMinecraftVersion();
	if (!theirVersion.isNull() && !dependsOnMinecraftVersion.isNull())
	{
		if (QRegExp(dependsOnMinecraftVersion, Qt::CaseInsensitive,
   QRegExp::Wildcard).indexIn(theirVersion) == -1)
		{
			throw MinecraftVersionMismatch(uid, dependsOnMinecraftVersion,
   theirVersion);
		}
	}
*/
