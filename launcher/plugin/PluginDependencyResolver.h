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

#include "plugin/PluginMetadata.h"

#include <QString>
#include <QStringList>
#include <QVector>

/* Produces a load order for discovered .mmco modules that respects their declared dependencies:
 * Kahn's topological sort over the DAG from MMCOModuleInfo::dependencies. Modules in a cycle are
 * disabled with DependencyCycle, modules missing a required dependency with DependencyMissing.
 * Modules the loader already disabled (e.g. for signature reasons) are not mutated: they stay out
 * of the load order and count as missing for anything that depends on them. */
class PluginDependencyResolver
{
  public:
	struct Result {
		/* Indices into the input vector, in load order; disabled modules are not included. */
		QVector<int> loadOrder;
	};

	/* Resolves dependencies for `modules`, mutated in place to set disable reasons/details for
	 * modules that cannot load. Already-disabled modules count as missing dependencies for anything
	 * that needs them. */
	static Result resolve(QVector<PluginMetadata>& modules);

  private:
	/* Compare two version strings using a relaxed semver scheme. Returns
	 * negative if a < b, 0 if equal, positive if a > b. Empty/missing
	 * strings compare equal. */
	static int compareVersions(const QString& a, const QString& b);
};
