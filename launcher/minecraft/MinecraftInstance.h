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
#include "BaseInstance.h"
#include <java/JavaVersion.h>
#include "minecraft/KiteModRules.h"
#include "modbridge/ModBridgeProtocol.h"
#include "minecraft/mod/Mod.h"
#include <QProcess>
#include <QDir>
#include "minecraft/launch/MinecraftServerTarget.h"
#include "minecraft/RuntimeContext.h"

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

	/// The Minecraft version this instance is set to, or empty. Loads the pack
	/// profile if nothing has yet; never touches the network.
	QString minecraftVersion() const;

	QString gameVersion() const override
	{
		return minecraftVersion();
	}

	/// Human name of the mod loader ("Fabric", "Forge", ...), or empty if no
	/// known loader is installed and enabled.
	QString modLoaderName() const override;

	/// Whether the Minecraft version understands --demo (added in 1.3.1; older
	/// clients ignore it and start a broken session).
	bool supportsDemo() const;

	////// Directories and files //////
	QString jarModsDir() const;
	QString resourcePacksDir() const;
	QString texturePacksDir() const;
	QString shaderPacksDir() const;
	// Instance-wide data pack folder, for global data pack loaders (Paxi,
	// OpenLoader, ...). Set by GlobalDataPacksPath; defaults to
	// <gameRoot>/datapacks.
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
	/// The -Dfabric.addMods argument that loads Kite's mod, or nothing if the
	/// mod is off, the loader is not Fabric or no jar is at hand. Not part of
	/// javaArguments(): only the game's JVM wants it.
	QStringList kiteModArguments() const;
	/// Whether Kite's mod is on and the instance is on Fabric.
	bool kiteModApplies() const;
	/// Whether the cached manifest has a build for this Minecraft and Fabric
	/// Loader version; Unknown until a manifest has been fetched.
	KiteMod::Support kiteModSupport() const;
	/// The bridge session of the launch under way: its port goes to the game as
	/// -Dkite.bridge.port (kiteModArguments()), its token as KITE_BRIDGE_TOKEN
	/// (createEnvironment()). Cleared when the instance stops.
	void setBridgeGrant(const ModBridgeProtocol::Grant& grant);
	/// The jar kiteModArguments() would load; empty when there is none.
	QString kiteModJar() const;
	/// The version of this instance's Fabric Loader; empty without one.
	QString kiteModLoaderVersion() const;

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

	/// The OS and architecture of this instance's Java, which decide its
	/// libraries and natives; the architecture Kite was built for until a Java
	/// is checked.
	RuntimeContext runtimeContext() const;

  protected:
	QMap<QString, QString>
	createCensorFilterFromSession(AuthSessionPtr session);
	QString launchMethod();

  protected: // data
	ModBridgeProtocol::Grant m_bridgeGrant;
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
