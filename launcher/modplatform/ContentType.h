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

#pragma once

#include <QString>

namespace ModPlatform
{

	/* No TexturePack entry: legacy texture packs and resource packs are the
	 * same product on both platforms and differ only in on-disk folder, which
	 * the folder model decides. The legacy page runs as ResourcePack. */
	enum class ContentType { Mod, ResourcePack, ShaderPack, DataPack };

	inline QString contentTypeFolderName(ContentType type)
	{
		switch (type) {
			case ContentType::Mod:
				return "mods";
			case ContentType::ResourcePack:
				return "resourcepacks";
			case ContentType::ShaderPack:
				return "shaderpacks";
			case ContentType::DataPack:
				return "datapacks";
		}
		return "mods";
	}

	/* Whether this kind of content is loader-specific. Resource, shader and
	 * data packs are not; sending a loader filter for them returns an empty
	 * result on Modrinth. */
	inline bool contentTypeUsesLoader(ContentType type)
	{
		return type == ContentType::Mod;
	}

	inline int contentTypeToCurseForgeClassId(ContentType type)
	{
		switch (type) {
			case ContentType::Mod:
				return 6; // Mods
			case ContentType::ResourcePack:
				return 12; // Resource Packs
			case ContentType::ShaderPack:
				return 6552; // Shaders
			case ContentType::DataPack:
				return 6945; // Data Packs
		}
		return 6;
	}

	inline QString contentTypeToModrinthFacet(ContentType type)
	{
		switch (type) {
			case ContentType::Mod:
				return "mod";
			case ContentType::ResourcePack:
				return "resourcepack";
			case ContentType::ShaderPack:
				return "shader";
			case ContentType::DataPack:
				return "datapack";
		}
		return "mod";
	}

	// CurseForge modLoaderType: 1=Forge, 4=Fabric, 5=Quilt, 6=NeoForge
	inline int loaderToCurseForgeModLoaderType(const QString& loader)
	{
		if (loader == "forge")
			return 1;
		if (loader == "fabric")
			return 4;
		if (loader == "quilt")
			return 5;
		if (loader == "neoforge")
			return 6;
		return 0; // unknown / any
	}

} // namespace ModPlatform
