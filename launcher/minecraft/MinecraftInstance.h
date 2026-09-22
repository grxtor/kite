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
#include "BaseInstance.h"
#include <java/JavaVersion.h>
#include "minecraft/mod/Mod.h"
#include <QProcess>
#include <QDir>
#include "minecraft/launch/MinecraftServerTarget.h"

class ModFolderModel;
class WorldList;
class GameOptions;
class LaunchStep;
class PackProfile;

class MinecraftInstance : public BaseInstance
{
	Q_OBJECT
  public:
	MinecraftInstance(SettingsObjectPtr globalSettings,
					  SettingsObjectPtr settings, const QString& rootDir);
	virtual ~MinecraftInstance() {};
	virtual void saveNow() override;

	// FIXME: remove
	QString typeName() const override;
	// FIXME: remove
	QSet<QString> traits() const override;

	bool canEdit() const override
	{
		return true;
	}

	bool canExport() const override
	{
		return true;
	}

	/**
	 * @brief the Minecraft version this instance is set to, or empty
	 *
	 * Reads it from the pack profile, loading that profile first if nothing
	 * has needed it yet. That load is the point of this accessor: the profile
	 * is only read on demand, so anything that asks the components directly
	 * before a launch or an update gets an empty string back and silently
	 * concludes whatever an empty version implies.
	 *
	 * Never touches the network.
	 */
	QString minecraftVersion() const;

	/// BaseInstance::gameVersion() override - same value as minecraftVersion().
	QString gameVersion() const override
	{
		return minecraftVersion();
	}

	/**
	 * Human name of this instance's mod loader ("Fabric", "Forge", ...),
	 * or empty if none of the loaders this launcher knows about is
	 * installed and enabled. See BaseInstance::modLoaderName().
	 */
	QString modLoaderName() const override;

	/**
	 * Whether this instance's Minecraft version understands --demo.
	 *
	 * Demo mode was added in 1.3.1; older clients ignore the argument and
	 * start a normal (unauthenticated, and therefore broken) session, so
	 * offering it there would be a trap.
	 */
	bool supportsDemo() const;

	////// Directories and files //////
	QString jarModsDir() const;
	QString resourcePacksDir() const;
	QString texturePacksDir() const;
	QString shaderPacksDir() const;
	/* Instance-wide data pack folder. Vanilla only loads data packs from
	 * saves/<world>/datapacks, so this folder is only meaningful with a
	 * global data pack loader (Paxi, OpenLoader, ...); its location is
	 * therefore configurable through the GlobalDataPacksPath setting and
	 * defaults to <gameRoot>/datapacks. */
	QString dataPacksDir() const;
	QString modsRoot() const override;
	QString coreModsDir() const;
	QString modsCacheLocation() const;
	QString libDir() const;
	QString worldDir() const;
	QString resourcesDir() const;
	QDir jarmodsPath() const;
	QDir librariesPath() const;
	QDir versionsPath() const;
	QString instanceConfigFolder() const override;

	// Path to the instance's minecraft directory.
	QString gameRoot() const override;

	// Path to the instance's minecraft bin directory.
	QString binRoot() const;

	// where to put the natives during/before launch
	QString getNativePath() const;

	// where the instance-local libraries should be
	QString getLocalLibraryPath() const;

	//////  Profile management //////
	std::shared_ptr<PackProfile> getPackProfile() const;

	//////  Mod Lists  //////
	std::shared_ptr<ModFolderModel> loaderModList() const;
	std::shared_ptr<ModFolderModel> coreModList() const;
	std::shared_ptr<ModFolderModel> resourcePackList() const;
	std::shared_ptr<ModFolderModel> texturePackList() const;
	std::shared_ptr<ModFolderModel> shaderPackList() const;
	std::shared_ptr<ModFolderModel> dataPackList() const;
	std::shared_ptr<WorldList> worldList() const;
	std::shared_ptr<GameOptions> gameOptionsModel() const;

	//////  Launch stuff //////
	Task::Ptr createUpdateTask(Net::Mode mode) override;
	shared_qobject_ptr<LaunchTask>
	createLaunchTask(AuthSessionPtr account,
					 MinecraftServerTargetPtr serverToJoin) override;
	QStringList extraArguments() const override;
	QStringList
	verboseDescription(AuthSessionPtr session,
					   MinecraftServerTargetPtr serverToJoin) override;
	QList<Mod> getJarMods() const;
	QString createLaunchScript(AuthSessionPtr session,
							   MinecraftServerTargetPtr serverToJoin);
	/// get arguments passed to java
	QStringList javaArguments() const;

	/// get variables for launch command variable substitution/environment
	QMap<QString, QString> getVariables() const override;

	/// create an environment for launching processes
	QProcessEnvironment createEnvironment() override;

	/// guess log level from a line of minecraft log
	MessageLevel::Enum guessLevel(const QString& line,
								  MessageLevel::Enum level) override;

	IPathMatcher::Ptr getLogFileMatcher() override;

	QString getLogFileRoot() override;

	QString getStatusbarDescription() override;

	// FIXME: remove
	virtual QStringList getClassPath() const;
	// FIXME: remove
	virtual QStringList getNativeJars() const;
	// FIXME: remove
	virtual QString getMainClass() const;

	// FIXME: remove
	virtual QStringList
	processMinecraftArgs(AuthSessionPtr account,
						 MinecraftServerTargetPtr serverToJoin) const;

	virtual JavaVersion getJavaVersion() const;

  protected:
	QMap<QString, QString>
	createCensorFilterFromSession(AuthSessionPtr session);
	QStringList validLaunchMethods();
	QString launchMethod();

  protected: // data
	std::shared_ptr<PackProfile> m_components;
	mutable std::shared_ptr<ModFolderModel> m_loader_mod_list;
	mutable std::shared_ptr<ModFolderModel> m_core_mod_list;
	mutable std::shared_ptr<ModFolderModel> m_resource_pack_list;
	mutable std::shared_ptr<ModFolderModel> m_shader_pack_list;
	mutable std::shared_ptr<ModFolderModel> m_texture_pack_list;
	mutable std::shared_ptr<ModFolderModel> m_data_pack_list;
	mutable std::shared_ptr<WorldList> m_world_list;
	mutable std::shared_ptr<GameOptions> m_game_options;
};

typedef std::shared_ptr<MinecraftInstance> MinecraftInstancePtr;
