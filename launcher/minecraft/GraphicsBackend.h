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

/*
 * The game's own "Graphics API" video setting (Minecraft 26.2 and later),
 * read and written straight in options.txt: the game's file is the one
 * source of truth. Plain functions, so they can be tested without an
 * instance.
 */
namespace GraphicsBackend
{
	/// Whether Minecraft @p mcVersion has the setting: 26.2 and later.
	bool supportsVersion(const QString& mcVersion);

	/// The chosen backend in <gameRoot>/options.txt: "default", "opengl" or
	/// "vulkan". "default" too when the file, the key or a known value is
	/// missing.
	QString read(const QString& gameRoot);

	/// Stores @p backend ("default", "opengl" or "vulkan") in
	/// <gameRoot>/options.txt, every other line kept. With no options.txt,
	/// "default" writes nothing and the others create a file with just this
	/// key. False for an unknown backend (nothing written) or a failed write.
	bool write(const QString& gameRoot, const QString& backend);
} // namespace GraphicsBackend
