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

#pragma once

#include <QString>

/* The ids and action names ModHealthModel.cpp and ModHealthText.cpp both
 * speak. Private to the two. */
namespace ModHealthModelIds
{
	inline const QString kActionEnable = QStringLiteral("enable");
	inline const QString kActionDisable = QStringLiteral("disable");
	inline const QString kActionInstall = QStringLiteral("install");

	/* The ids a mod's metadata uses for the game and its loaders. */
	inline const QString kMinecraftId = QStringLiteral("minecraft");
	inline const QString kFabricLoaderId = QStringLiteral("fabricloader");
	inline const QString kQuiltLoaderId = QStringLiteral("quilt_loader");
	inline const QString kForgeId = QStringLiteral("forge");
	inline const QString kNeoForgeId = QStringLiteral("neoforge");
} // namespace ModHealthModelIds
