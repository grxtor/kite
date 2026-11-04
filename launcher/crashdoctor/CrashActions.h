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

#include <QList>
#include <QString>
#include <QStringList>

/*
 * The closed set of things a crash doctor fix can do. A rule names one of
 * these and fills its parameters from what it matched; anything else is left
 * out when the catalog loads. Each is carried out in C++ (CrashFixer) or by
 * the Log tab's existing flows (installs, updates, uploads), never by the
 * catalog.
 */
namespace CrashActions
{
	enum class ParamType {
		/// 8..40.
		JavaMajor,
		/// Clamped to what the computer can give when applied.
		MemoryMb,
		/// A mod id or a jar file name in the instance's mods folder.
		ModRef,
		/// Several ModRefs.
		ModRefList,
		ModId,
		/// Below <gameRoot>/config.
		ConfigFile,
		/// One of ParamSpec::choices.
		Choice
	};

	struct ParamSpec {
		QString name;
		ParamType type = ParamType::ModId;
		bool optional = false;
		QStringList choices;
	};

	/// Who carries the action out.
	enum class Runner {
		/// CrashFixer changes settings or files, with an undo record.
		Fixer,
		/// The Log tab's install / update / upload flows (they ask first).
		Flow,
		/// Opens a page or folder; changes nothing.
		Navigation
	};

	struct Spec {
		QString name;
		Runner runner = Runner::Navigation;
		QList<ParamSpec> params;
		/// Asks before it runs: it turns something off, resets a file or
		/// changes what the game is started with.
		bool confirm = false;
		/// Offered on Windows only.
		bool windowsOnly = false;
		/// Offered only while a shader pack is on (Facts::shadersOn).
		bool needsShaders = false;
		/// Offered only with two or more graphics cards (Facts::severalGpus).
		bool needsSeveralGpus = false;
		/// Several findings of one card can share one run (all ids at once).
		bool batch = false;
	};

	/// The spec of action @p name, or null for a name not in the set.
	const Spec* find(const QString& name);
	const QList<Spec>& all();

	/// The driver download page of graphics card maker @p vendor (a choice of
	/// openDriverPage: "amd", "intel", "nvidia"), or "". The addresses are
	/// Kite's; a rule only picks the maker.
	QString driverPage(const QString& vendor);

	inline constexpr int kMinJavaMajor = 8;
	inline constexpr int kMaxJavaMajor = 40;
} // namespace CrashActions
