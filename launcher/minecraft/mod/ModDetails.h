/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include <QList>
#include <QString>
#include <QStringList>

/* One thing a mod's metadata says about another mod: that it needs it,
 * or that it must not run next to it. Only the declarations a loader
 * acts on are kept - "recommends", "suggests" and optional mods.toml
 * entries change nothing about whether the game starts. */
struct ModDependency {
	enum class Kind {
		/// fabric/quilt "depends"; mods.toml mandatory=true or
		/// type="required".
		Required,
		/// fabric/quilt "breaks"; mods.toml type="incompatible". The
		/// loader refuses to start with both present.
		Incompatible,
		/// fabric "conflicts"; mods.toml type="discouraged". The loader
		/// starts, with a warning.
		Discouraged
	};
	/// How `versions` is written.
	enum class Syntax {
		/// fabric.mod.json / quilt.mod.json: `versions` are alternatives,
		/// any of which may match; each is space-separated predicates
		/// (">=1.20 <1.21", "~0.5.3", "1.20.x") that must all match.
		Fabric,
		/// mods.toml versionRange, Maven style: "[1.20,1.21)", "[47,)".
		/// One range string in `versions[0]`.
		Maven
	};

	QString id;
	Kind kind = Kind::Required;
	Syntax syntax = Syntax::Fabric;
	/// Empty means any version.
	QStringList versions;
	/// The metadata it came from: "fabric", "quilt", "forge" or
	/// "neoforge". A jar built for several loaders declares its
	/// dependencies once per loader, and only the set of the loader that
	/// actually reads it applies.
	QString loader;
};

/* Another id a file answers to besides its own mod_id. */
struct ProvidedMod {
	QString id;
	QString version;
};

struct ModDetails {
	QString mod_id;
	QString name;
	QString version;
	QString mcversion;
	QString homeurl;
	QString updateurl;
	QString description;
	QStringList authors;
	QString credits;
	/// Archive-relative path of the logo the metadata declares
	/// (fabric/quilt "icon", mods.toml/mcmod.info "logoFile"), or empty.
	QString icon;

	/// The loaders whose metadata the file carries - "fabric" for
	/// fabric.mod.json, "quilt" for quilt.mod.json, "forge" for
	/// META-INF/mods.toml and mcmod.info, "neoforge" for
	/// META-INF/neoforge.mods.toml (or a mods.toml that requires
	/// "neoforge"), "liteloader" for litemod.json. Empty when it has none
	/// of these: a plain library jar, or a file not yet read.
	QStringList loaders;
	/// What it needs and what it clashes with, from every metadata file
	/// it has (see ModDependency::loader).
	QList<ModDependency> dependencies;
	/// Ids it answers to besides mod_id: fabric/quilt "provides", the
	/// other [[mods]] of a mods.toml that declares several, and every mod
	/// bundled inside it (fabric/quilt "jars", META-INF/jarjar), nested
	/// ones included.
	QList<ProvidedMod> provides;
};
