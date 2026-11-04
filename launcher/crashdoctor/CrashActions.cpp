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

#include "CrashActions.h"

namespace CrashActions
{
	namespace
	{
		ParamSpec param(const QString& name, ParamType type,
						bool optional = false, QStringList choices = {})
		{
			return ParamSpec{name, type, optional, std::move(choices)};
		}

		Spec fixer(const QString& name, QList<ParamSpec> params = {},
				   bool confirm = false)
		{
			Spec spec;
			spec.name = name;
			spec.runner = Runner::Fixer;
			spec.params = std::move(params);
			spec.confirm = confirm;
			return spec;
		}

		Spec flow(const QString& name, QList<ParamSpec> params = {},
				  bool batch = false)
		{
			Spec spec;
			spec.name = name;
			spec.runner = Runner::Flow;
			spec.params = std::move(params);
			spec.batch = batch;
			return spec;
		}

		Spec navigation(const QString& name, QList<ParamSpec> params = {})
		{
			Spec spec;
			spec.name = name;
			spec.runner = Runner::Navigation;
			spec.params = std::move(params);
			return spec;
		}

		QList<Spec> makeAll()
		{
			const QStringList contentKinds{"mods", "datapacks", "resourcepacks",
										   "shaderpacks"};
			QList<Spec> specs{
				// Java: settings the next launch's VerifyJavaInstall acts on.
				fixer("useJavaForInstance", {param("major", ParamType::JavaMajor)}),
				fixer("useRecommendedJava"),
				fixer("useMatchingArchJava"),
				fixer("installJava", {param("major", ParamType::JavaMajor)}),
				fixer("setMemory", {param("maxMb", ParamType::MemoryMb)}),
				fixer("clearCustomJvmArgs", {}, true),
				fixer("setMacFirstThread"),
				// Mods folder and game files: renamed aside, never deleted.
				fixer("disableMod", {param("target", ParamType::ModRef)}, true),
				fixer("removeDuplicateMod", {param("mod", ParamType::ModId)}, true),
				fixer("turnOffKiteMod"),
				fixer("resetConfigFile", {param("file", ParamType::ConfigFile)}, true),
				fixer("resetOptions", {}, true),
				fixer("turnOffShaders"),
				fixer("setGraphicsBackend",
					  {param("backend", ParamType::Choice, false, {"opengl", "default"})}),
				fixer("useDedicatedGpu"),
				fixer("repairGameFiles"),
				// Existing flows, which show what they will install first.
				flow("installMissingDependency", {param("mod", ParamType::ModId)}, true),
				flow("updateMod", {param("mods", ParamType::ModRefList)}, true),
				flow("updateLoader"),
				flow("rematchPerformanceMods"),
				flow("uploadLog"),
				navigation("openModsFolder"),
				navigation("openInstanceFolder"),
				navigation("openWorldsFolder"),
				navigation("openBackups"),
				navigation("openContentTab",
						   {param("kind", ParamType::Choice, true, contentKinds)}),
				navigation("openJavaSettings"),
				navigation("openMemorySettings"),
				navigation("openLog"),
			};
			for (Spec& spec : specs) {
				if (spec.name == QLatin1String("useDedicatedGpu")) {
					spec.windowsOnly = true;
				}
				if (spec.name == QLatin1String("disableMod")) {
					spec.batch = true;
				}
				if (spec.name == QLatin1String("turnOffShaders")) {
					spec.needsShaders = true;
				}
			}
			return specs;
		}
	} // namespace

	const QList<Spec>& all()
	{
		static const QList<Spec> specs = makeAll();
		return specs;
	}

	const Spec* find(const QString& name)
	{
		for (const Spec& spec : all()) {
			if (spec.name == name) {
				return &spec;
			}
		}
		return nullptr;
	}
} // namespace CrashActions
