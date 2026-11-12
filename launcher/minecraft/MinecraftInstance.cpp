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

#include "MinecraftInstance.h"
#include "crashdoctor/CrashDoctorService.h"
#include "minecraft/launch/CreateGameFolders.h"
#include "minecraft/launch/ExtractNatives.h"
#include "minecraft/launch/PrintInstanceInfo.h"
#include "settings/Setting.h"
#include "settings/SettingsObject.h"
#include "core/LauncherContext.h"
#include <QFileInfo>
#include <QRegularExpression>

#include "MMCStrings.h"
#include "pathmatcher/RegexpMatcher.h"
#include "pathmatcher/MultiMatcher.h"
#include "FileSystem.h"
#include "java/JavaVersion.h"
#include "ProcessOutput.h"

#include "launch/LaunchTask.h"
#include "launch/steps/LookupServerAddress.h"
#include "launch/steps/PostLaunchCommand.h"
#include "launch/steps/Update.h"
#include "launch/steps/PreLaunchCommand.h"
#include "launch/steps/TextPrint.h"
#include "launch/steps/CheckJava.h"

#include "minecraft/KiteModRules.h"
#include "minecraft/launch/KitePartLaunch.h"
#include "minecraft/launch/DirectJavaLaunch.h"
#include "minecraft/launch/ModMinecraftJar.h"
#include "minecraft/launch/PrepareKiteMod.h"
#include "minecraft/launch/PreparePerformance.h"
#include "minecraft/launch/StartModBridge.h"
#include "minecraft/launch/ClaimAccount.h"
#include "minecraft/launch/ReconstructAssets.h"
#include "minecraft/launch/ScanModFolders.h"
#include "minecraft/launch/VerifyJavaInstall.h"

#include "java/JavaUtils.h"

#include "meta/Index.h"
#include "meta/VersionList.h"

#include "icons/IconList.h"

#include "mod/DataPackFolderModel.h"
#include "mod/ModFolderModel.h"
#include "mod/ResourcePackFolderModel.h"
#include "mod/ShaderPackFolderModel.h"
#include "mod/TexturePackFolderModel.h"

#include "WorldList.h"

#include "PackProfile.h"
#include "Version.h"
#include "AssetsUtils.h"
#include "MinecraftUpdate.h"
#include "MinecraftLoadAndCheck.h"
#include "minecraft/gameoptions/GameOptions.h"
#include "minecraft/update/FoldersTask.h"

#define IBUS "@im=ibus"

// all of this because keeping things compatible with deprecated old settings
// if either of the settings {a, b} is true, this also resolves to true
class OrSetting : public Setting
{
	Q_OBJECT
  public:
	OrSetting(QString id, std::shared_ptr<Setting> a,
			  std::shared_ptr<Setting> b)
		: Setting({id}, false), m_a(a), m_b(b)
	{
	}
	virtual QVariant get() const
	{
		bool a = m_a->get().toBool();
		bool b = m_b->get().toBool();
		return a || b;
	}
	virtual void reset() {}
	virtual void set(QVariant value) {}

  private:
	std::shared_ptr<Setting> m_a;
	std::shared_ptr<Setting> m_b;
};

MinecraftInstance::MinecraftInstance(SettingsObjectPtr globalSettings,
									 SettingsObjectPtr settings,
									 const QString& rootDir)
	: BaseInstance(globalSettings, settings, rootDir)
{
	// Java Settings
	auto javaOverride = m_settings->registerSetting("OverrideJava", false);
	auto locationOverride =
		m_settings->registerSetting("OverrideJavaLocation", false);
	auto argsOverride = m_settings->registerSetting("OverrideJavaArgs", false);

	// combinations
	auto javaOrLocation = std::make_shared<OrSetting>(
		"JavaOrLocationOverride", javaOverride, locationOverride);
	auto javaOrArgs = std::make_shared<OrSetting>("JavaOrArgsOverride",
												  javaOverride, argsOverride);

	m_settings->registerOverride(globalSettings->getSetting("JavaPath"),
								 javaOrLocation);
	m_settings->registerOverride(globalSettings->getSetting("JvmArgs"),
								 javaOrArgs);

	// special!
	m_settings->registerPassthrough(globalSettings->getSetting("JavaTimestamp"),
									javaOrLocation);
	m_settings->registerPassthrough(globalSettings->getSetting("JavaVersion"),
									javaOrLocation);
	m_settings->registerPassthrough(
		globalSettings->getSetting("JavaArchitecture"), javaOrLocation);
	m_settings->registerPassthrough(globalSettings->getSetting("JavaVendor"),
									javaOrLocation);
	m_settings->registerPassthrough(
		globalSettings->getSetting("JavaRealArchitecture"), javaOrLocation);
	m_settings->registerPassthrough(
		globalSettings->getSetting("JavaCheckedPath"), javaOrLocation);

	// Java auto-download vendor preference
	auto javaVendorOverride =
		m_settings->registerSetting("OverrideJavaAutoDownloadVendor", false);
	m_settings->registerOverride(globalSettings->getSetting("JavaAutoDownload"),
								 javaVendorOverride);
	m_settings->registerOverride(
		globalSettings->getSetting("JavaAutoDownloadVendor"),
		javaVendorOverride);

	// Window Size
	auto windowSetting = m_settings->registerSetting("OverrideWindow", false);
	m_settings->registerOverride(globalSettings->getSetting("LaunchMaximized"),
								 windowSetting);
	m_settings->registerOverride(
		globalSettings->getSetting("MinecraftWinWidth"), windowSetting);
	m_settings->registerOverride(
		globalSettings->getSetting("MinecraftWinHeight"), windowSetting);

	// Memory
	auto memorySetting = m_settings->registerSetting("OverrideMemory", false);
	m_settings->registerOverride(globalSettings->getSetting("MinMemAlloc"),
								 memorySetting);
	m_settings->registerOverride(globalSettings->getSetting("MaxMemAlloc"),
								 memorySetting);
	m_settings->registerOverride(globalSettings->getSetting("PermGen"),
								 memorySetting);

	// Minecraft launch method
	auto launchMethodOverride =
		m_settings->registerSetting("OverrideMCLaunchMethod", false);
	m_settings->registerOverride(globalSettings->getSetting("MCLaunchMethod"),
								 launchMethodOverride);

	// Native library workarounds
	auto nativeLibraryWorkaroundsOverride =
		m_settings->registerSetting("OverrideNativeWorkarounds", false);
	m_settings->registerOverride(globalSettings->getSetting("UseNativeOpenAL"),
								 nativeLibraryWorkaroundsOverride);
	m_settings->registerOverride(globalSettings->getSetting("UseNativeGLFW"),
								 nativeLibraryWorkaroundsOverride);

	// Game time
	auto gameTimeOverride =
		m_settings->registerSetting("OverrideGameTime", false);
	m_settings->registerOverride(globalSettings->getSetting("ShowGameTime"),
								 gameTimeOverride);
	m_settings->registerOverride(globalSettings->getSetting("RecordGameTime"),
								 gameTimeOverride);

	// Join server on launch, this does not have a global override
	m_settings->registerSetting("JoinServerOnLaunch", false);
	m_settings->registerSetting("JoinServerOnLaunchAddress", "");

	/* Kite Performance (PerformanceSetupTask). Excluded and Included are
	 * comma-joined Modrinth project ids, as the config stores strings: defaults
	 * the player unticked, extras they ticked. For is the version and loader
	 * the mods were last matched to ("1.21.5 fabric"), empty before the first
	 * setup. Missing records catalog mods Modrinth had no build of, and when
	 * (PerformanceSetup::missingRecord()); Unmatched lists the mods the last
	 * re-match found no build of. */
	m_settings->registerSetting("KitePerformance", false);
	m_settings->registerSetting("KitePerformanceExcluded", "");
	m_settings->registerSetting("KitePerformanceIncluded", "");
	m_settings->registerSetting("KitePerformanceFor", "");
	m_settings->registerSetting("KitePerformanceMissing", "");
	m_settings->registerSetting("KitePerformanceUnmatched", "");

	/* What the last background check for newer mod versions found
	 * (models/ModUpdateReminder): when (ms since the epoch, 0 for never), how
	 * many files, and for which version and loader ("1.21.1 fabric"). */
	m_settings->registerSetting("ModUpdatesCheckedAt", QVariant(qlonglong(0)));
	m_settings->registerSetting("ModUpdatesFound", 0);
	m_settings->registerSetting("ModUpdatesFor", "");

	// Kite's mod is injected with -Dfabric.addMods; Fabric instances only.
	m_settings->registerSetting("KiteMod", true);

	/* Set by the crash doctor's fixes (crashdoctor/CrashFixer), read at the
	 * next launch. JavaMajorAtLeast raises the Java major this instance needs
	 * (a mod built for a newer Java than its Minecraft); JavaRepick makes
	 * VerifyJavaInstall choose the Java again once, as if none were set;
	 * RepairGameFiles makes LibrariesTask download every library again once;
	 * MacStartOnFirstThread adds -XstartOnFirstThread even when the version
	 * does not ask for it. CrashDoctorUndo holds what undoes the last fix;
	 * CrashDoctorLastFix the rule it was for and when ({"rule", "at"}), so a
	 * crash the same way after it can be sent to Kite as a fix that did not
	 * help. CrashDoctorNativeCrashes counts the native crashes in a row
	 * (crashdoctor/CrashStreak). */
	m_settings->registerSetting("JavaMajorAtLeast", 0);
	m_settings->registerSetting("JavaRepick", false);
	m_settings->registerSetting("RepairGameFiles", false);
	m_settings->registerSetting("MacStartOnFirstThread", false);
	m_settings->registerSetting("CrashDoctorUndo", "");
	m_settings->registerSetting("CrashDoctorLastFix", "");
	m_settings->registerSetting("CrashDoctorNativeCrashes", "");
	// The mod bridge session ends with the game.
	connect(this, &BaseInstance::runningStatusChanged, this,
			[this](bool running) {
				if (!running) {
					m_bridgeGrant = {};
				}
			});

	// Only a loader such as Paxi or OpenLoader reads an instance-wide folder.
	auto dataPacksEnabled =
		m_settings->registerSetting("GlobalDataPacksEnabled", false);
	auto dataPacksPath = m_settings->registerSetting("GlobalDataPacksPath", "");

	// Either setting changes the watched folder, so drop the cached model.
	connect(dataPacksEnabled.get(), &Setting::SettingChanged, this,
			[this] { m_data_pack_list.reset(); });
	connect(dataPacksPath.get(), &Setting::SettingChanged, this,
			[this] { m_data_pack_list.reset(); });

	// Export dialog defaults. ExportName empty = the instance name;
	// ExportRecommendedRAM 0 = not specified.
	m_settings->registerSetting("ExportName", "");
	m_settings->registerSetting("ExportVersion", "1.0.0");
	m_settings->registerSetting("ExportSummary", "");
	m_settings->registerSetting("ExportAuthor", "");
	m_settings->registerSetting("ExportOptionalFiles", true);
	m_settings->registerSetting("ExportRecommendedRAM", 0);

	// DEPRECATED: Read what versions the user configuration thinks should be
	// used
	m_settings->registerSetting({"IntendedVersion", "MinecraftVersion"}, "");
	m_settings->registerSetting("LWJGLVersion", "");
	m_settings->registerSetting("ForgeVersion", "");
	m_settings->registerSetting("LiteloaderVersion", "");

	m_components.reset(new PackProfile(this));
	m_components->setOldConfigVersion(
		"net.minecraft", m_settings->get("IntendedVersion").toString());
	m_components->setOldConfigVersion(
		"org.lwjgl", m_settings->get("LWJGLVersion").toString());
	m_components->setOldConfigVersion(
		"net.minecraftforge", m_settings->get("ForgeVersion").toString());
	m_components->setOldConfigVersion(
		"com.mumfrey.liteloader",
		m_settings->get("LiteloaderVersion").toString());

	// Refreshes readers of gameVersion()/modLoaderName() when the profile
	// changes.
	connect(m_components.get(), &PackProfile::minecraftChanged, this,
			[this] { emit propertiesChanged(this); });
}

void MinecraftInstance::saveNow()
{
	m_components->saveNow();
}

QString MinecraftInstance::typeName() const
{
	return "Minecraft";
}

std::shared_ptr<PackProfile> MinecraftInstance::getPackProfile() const
{
	return m_components;
}

QSet<QString> MinecraftInstance::traits() const
{
	auto components = getPackProfile();
	if (!components) {
		return {"version-incomplete"};
	}
	auto profile = components->getProfile();
	if (!profile) {
		return {"version-incomplete"};
	}
	return profile->getTraits();
}

QString MinecraftInstance::minecraftVersion() const
{
	auto components = getPackProfile();
	if (!components) {
		return QString();
	}

	QString version = components->getComponentVersion("net.minecraft");
	if (!version.isEmpty()) {
		return version;
	}

	// Profile not read yet: load it offline so a menu never waits on the
	// network.
	components->reload(Net::Mode::Offline);
	return components->getComponentVersion("net.minecraft");
}

QString MinecraftInstance::modLoaderName() const
{
	auto components = getPackProfile();
	if (!components) {
		return QString();
	}

	// Relies on minecraftVersion()'s lazy load.
	minecraftVersion();

	const int rows = components->rowCount();
	for (int i = 0; i < rows; ++i) {
		Component* component = components->getComponent(i);
		if (!component || !component->isEnabled()) {
			continue;
		}
		if (const ModLoaderInfo* loader = modLoaderForUid(component->getID())) {
			return loader->brandName;
		}
	}
	return QString();
}

bool MinecraftInstance::supportsDemo() const
{
	const QString version = minecraftVersion();
	if (version.isEmpty()) {
		// Unknown version (broken profile): offer demo and let the launch fail.
		return true;
	}
	// Pre-1.0 alpha/beta names are not recognised.
	return Version(version) >= Version("1.3.1");
}

QString MinecraftInstance::gameRoot() const
{
	QFileInfo mcDir(FS::PathCombine(instanceRoot(), "minecraft"));
	QFileInfo dotMCDir(FS::PathCombine(instanceRoot(), ".minecraft"));

	if (mcDir.exists() && !dotMCDir.exists())
		return mcDir.filePath();
	else
		return dotMCDir.filePath();
}

QString MinecraftInstance::binRoot() const
{
	return FS::PathCombine(gameRoot(), "bin");
}

QString MinecraftInstance::getNativePath() const
{
	QDir natives_dir(FS::PathCombine(instanceRoot(), "natives/"));
	return natives_dir.absolutePath();
}

QString MinecraftInstance::getLocalLibraryPath() const
{
	QDir libraries_dir(FS::PathCombine(instanceRoot(), "libraries/"));
	return libraries_dir.absolutePath();
}

QString MinecraftInstance::jarModsDir() const
{
	QDir jarmods_dir(FS::PathCombine(instanceRoot(), "jarmods/"));
	return jarmods_dir.absolutePath();
}

QString MinecraftInstance::modsRoot() const
{
	return FS::PathCombine(gameRoot(), "mods");
}

QString MinecraftInstance::coreModsDir() const
{
	return FS::PathCombine(gameRoot(), "coremods");
}

QString MinecraftInstance::resourcePacksDir() const
{
	return FS::PathCombine(gameRoot(), "resourcepacks");
}

QString MinecraftInstance::texturePacksDir() const
{
	return FS::PathCombine(gameRoot(), "texturepacks");
}

QString MinecraftInstance::shaderPacksDir() const
{
	return FS::PathCombine(gameRoot(), "shaderpacks");
}

QString MinecraftInstance::dataPacksDir() const
{
	// Data pack loaders disagree on the folder name, so the user picks it.
	QString relativePath = settings()->get("GlobalDataPacksPath").toString();
	if (relativePath.isEmpty()) {
		relativePath = QStringLiteral("datapacks");
	}
	return FS::PathCombine(gameRoot(), relativePath);
}

QString MinecraftInstance::instanceConfigFolder() const
{
	return FS::PathCombine(gameRoot(), "config");
}

QString MinecraftInstance::libDir() const
{
	return FS::PathCombine(gameRoot(), "lib");
}

QString MinecraftInstance::worldDir() const
{
	return FS::PathCombine(gameRoot(), "saves");
}

QString MinecraftInstance::resourcesDir() const
{
	return FS::PathCombine(gameRoot(), "resources");
}

QDir MinecraftInstance::jarmodsPath() const
{
	return QDir(jarModsDir());
}

QStringList MinecraftInstance::getClassPath() const
{
	QStringList jars, nativeJars;
	auto profile = m_components->getProfile();
	profile->getLibraryFiles(runtimeContext(), jars, nativeJars,
							 getLocalLibraryPath(), binRoot());
	return jars;
}

QString MinecraftInstance::getMainClass() const
{
	auto profile = m_components->getProfile();
	return profile->getMainClass();
}

QStringList MinecraftInstance::getNativeJars() const
{
	QStringList jars, nativeJars;
	auto profile = m_components->getProfile();
	profile->getLibraryFiles(runtimeContext(), jars, nativeJars,
							 getLocalLibraryPath(), binRoot());
	return nativeJars;
}

QStringList MinecraftInstance::extraArguments() const
{
	auto list = BaseInstance::extraArguments();
	auto version = getPackProfile();
	if (!version)
		return list;
	auto jarMods = getJarMods();
	if (!jarMods.isEmpty()) {
		list.append({"-Dfml.ignoreInvalidMinecraftCertificates=true",
					 "-Dfml.ignorePatchDiscrepancies=true"});
	}
	return list;
}

QString MinecraftInstance::kiteModLoaderVersion() const
{
	auto components = getPackProfile();
	return components ? components->getComponentVersion(KiteMod::kFabricLoaderUid)
					  : QString();
}

bool MinecraftInstance::kiteModApplies() const
{
	auto components = getPackProfile();
	if (!components) {
		return false;
	}
	// Loads the profile if nothing has yet.
	minecraftVersion();
	return KiteMod::appliesTo(m_settings->get("KiteMod").toBool(),
							  components->primaryModLoader(),
							  kiteModLoaderVersion());
}

KiteMod::Support MinecraftInstance::kiteModSupport() const
{
	auto* context = LAUNCHER;
	if (!context) {
		return KiteMod::Support::Unknown;
	}
	const QString loaderVersion = kiteModLoaderVersion();
	return KiteMod::cachedSupport(context->metacache()->getBasePath("general"),
								  minecraftVersion(), loaderVersion);
}

// The jar of Kite's mod to inject, or empty. Never fetches (PrepareKiteMod
// does); KITE_MOD_JAR overrides it for mod development.
QString MinecraftInstance::kiteModJar() const
{
	QString jar;
	const QFileInfo devJar(qEnvironmentVariable("KITE_MOD_JAR"));
	if (devJar.isFile()) {
		jar = devJar.absoluteFilePath();
	} else if (auto* context = LAUNCHER) {
		const QString cacheRoot = context->metacache()->getBasePath("general");
		const auto build = KiteMod::cachedBuild(cacheRoot, minecraftVersion(),
												kiteModLoaderVersion());
		if (build && KiteMod::isDownloaded(cacheRoot, *build)) {
			jar = KiteMod::jarPath(cacheRoot, *build);
		}
	}
	// The jar has the last word: one whose own metadata rules this Minecraft
	// out stops Fabric from starting the game, so it is left out
	// (PrepareKiteMod logs why).
	if (!jar.isEmpty() &&
		KiteMod::jarFileAcceptsMinecraft(jar, minecraftVersion()) ==
			VersionRange::Match::No) {
		return QString();
	}
	return jar;
}

QStringList MinecraftInstance::kiteModArguments() const
{
	if (!kiteModApplies()) {
		return {};
	}
	const QString jar = kiteModJar();
	if (jar.isEmpty()) {
		return {};
	}
	QStringList arguments{KiteMod::addModsArgument(extraArguments(), jar)};
	if (m_bridgeGrant.valid()) {
		// Port only: the token travels in the environment, as argv is logged.
		arguments << QStringLiteral("-D%1=%2")
						 .arg(QString::fromLatin1(
								  ModBridgeProtocol::kPortProperty),
							  QString::number(m_bridgeGrant.port));
	}
	// Performance instances open on the server list by default.
	if (m_settings->get("KitePerformance").toBool()) {
		arguments << QStringLiteral("-Dkite.performance=true");
	}
	return arguments;
}

void MinecraftInstance::setBridgeGrant(const ModBridgeProtocol::Grant& grant)
{
	m_bridgeGrant = grant;
}

QStringList MinecraftInstance::javaArguments() const
{
	QStringList args;

	// custom args go first. we want to override them if we have our own here.
	args.append(extraArguments());

	// OSX dock icon and name
#ifdef Q_OS_MAC
	args << "-Xdock:icon=icon.png";
	args << QString("-Xdock:name=\"%1\"").arg(windowTitle());
#endif
	auto traits_ = traits();
	// HACK: fix issues on macOS with 1.13 snapshots
	// NOTE: Oracle Java option. if there are alternate jvm implementations,
	// this would be the place to customize this for them
#ifdef Q_OS_MAC
	if (traits_.contains("FirstThreadOnMacOS") ||
		m_settings->get("MacStartOnFirstThread").toBool()) {
		args << QString("-XstartOnFirstThread");
	}
#endif

	// HACK: Stupid hack for Intel drivers. See:
	// https://mojang.atlassian.net/browse/MCL-767
#ifdef Q_OS_WIN32
	args << QString(
		"-XX:HeapDumpPath=MojangTricksIntelDriversForPerformance_javaw.exe_"
		"minecraft.exe.heapdump");
#endif

	int min = settings()->get("MinMemAlloc").toInt();
	int max = settings()->get("MaxMemAlloc").toInt();
	if (min < max) {
		args << QString("-Xms%1m").arg(min);
		args << QString("-Xmx%1m").arg(max);
	} else {
		args << QString("-Xms%1m").arg(max);
		args << QString("-Xmx%1m").arg(min);
	}

	// No PermGen in newer java.
	JavaVersion javaVersion = getJavaVersion();
	if (javaVersion.requiresPermGen()) {
		auto permgen = settings()->get("PermGen").toInt();
		if (permgen != 64) {
			args << QString("-XX:PermSize=%1m").arg(permgen);
		}
	}

	args << "-Duser.language=en";
	// The game prints UTF-8, which is how LoggedProcess reads it, whatever the
	// system code page (cp1254 on a Turkish Windows).
	args << ProcessOutput::jvmArguments(javaVersion.major());

	return args;
}

QMap<QString, QString> MinecraftInstance::getVariables() const
{
	QMap<QString, QString> out;
	out.insert("INST_NAME", name());
	out.insert("INST_ID", id());
	out.insert("INST_DIR", QDir(instanceRoot()).absolutePath());
	out.insert("INST_MC_DIR", QDir(gameRoot()).absolutePath());
	out.insert("INST_JAVA", settings()->get("JavaPath").toString());
	out.insert("INST_JAVA_ARGS", javaArguments().join(' '));
	return out;
}

QProcessEnvironment MinecraftInstance::createEnvironment()
{
	// prepare the process environment
	QProcessEnvironment env = CleanEnviroment();

	// export some infos
	auto variables = getVariables();
	for (auto it = variables.begin(); it != variables.end(); ++it) {
		env.insert(it.key(), it.value());
	}
	if (m_bridgeGrant.valid()) {
		env.insert(QString::fromLatin1(ModBridgeProtocol::kTokenEnvVar),
				   m_bridgeGrant.token);
	}
	return env;
}

static QString replaceTokensIn(QString text, QMap<QString, QString> with)
{
	QString result;
	QRegularExpression token_regexp("\\$\\{(.+?)\\}");
	QRegularExpressionMatchIterator it = token_regexp.globalMatch(text);
	int tail = 0;
	while (it.hasNext()) {
		QRegularExpressionMatch match = it.next();
		result.append(text.mid(tail, match.capturedStart() - tail));
		QString key = match.captured(1);
		auto iter = with.find(key);
		if (iter != with.end()) {
			result.append(*iter);
		}
		tail = match.capturedEnd();
	}
	result.append(text.mid(tail));
	return result;
}

QStringList MinecraftInstance::processMinecraftArgs(
	AuthSessionPtr session, MinecraftServerTargetPtr serverToJoin) const
{
	auto profile = m_components->getProfile();
	QString args_pattern = profile->getMinecraftArguments();
	for (auto tweaker : profile->getTweakers()) {
		args_pattern += " --tweakClass " + tweaker;
	}

	if (serverToJoin && !serverToJoin->address.isEmpty()) {
		if (profile->hasTrait("feature:is_quick_play_multiplayer")) {
			args_pattern += " --quickPlayMultiplayer " +
							serverToJoin->quickPlayHost() + ":" +
							QString::number(serverToJoin->port);
		} else {
			args_pattern += " --server " + serverToJoin->address;
			args_pattern += " --port " + QString::number(serverToJoin->port);
		}
	} else if (serverToJoin && !serverToJoin->world.isEmpty() &&
			   profile->hasTrait("feature:is_quick_play_singleplayer")) {
		args_pattern += " --quickPlaySingleplayer " + serverToJoin->world;
	}

	QMap<QString, QString> token_mapping;
	// yggdrasil!
	if (session) {
		token_mapping["auth_session"] = session->session;
		token_mapping["auth_access_token"] = session->access_token;
		token_mapping["auth_player_name"] = session->player_name;
		token_mapping["auth_uuid"] = session->uuid;
		token_mapping["user_properties"] = session->serializeUserProperties();
		token_mapping["user_type"] = session->user_type;
		if (session->demo) {
			args_pattern += " --demo";
		}
	}

	// blatant self-promotion.
	token_mapping["profile_name"] = token_mapping["version_name"] = "Kite";

	token_mapping["version_type"] = profile->getMinecraftVersionType();

	QString absRootDir = QDir(gameRoot()).absolutePath();
	token_mapping["game_directory"] = absRootDir;
	QString absAssetsDir = QDir("assets/").absolutePath();
	auto assets = profile->getMinecraftAssets();
	token_mapping["game_assets"] =
		AssetsUtils::getAssetsDir(assets->id, resourcesDir()).absolutePath();

	// 1.7.3+ assets tokens
	token_mapping["assets_root"] = absAssetsDir;
	token_mapping["assets_index_name"] = assets->id;

	QStringList parts = args_pattern.split(' ', Qt::SkipEmptyParts);
	for (int i = 0; i < parts.length(); i++) {
		parts[i] = replaceTokensIn(parts[i], token_mapping);
	}
	return parts;
}

QString
MinecraftInstance::createLaunchScript(AuthSessionPtr session,
									  MinecraftServerTargetPtr serverToJoin)
{
	QString launchScript;

	if (!m_components)
		return QString();
	auto profile = m_components->getProfile();
	if (!profile)
		return QString();

	auto mainClass = getMainClass();
	if (!mainClass.isEmpty()) {
		launchScript += "mainClass " + mainClass + "\n";
	}
	auto appletClass = profile->getAppletClass();
	if (!appletClass.isEmpty()) {
		launchScript += "appletClass " + appletClass + "\n";
	}

	if (serverToJoin && !serverToJoin->address.isEmpty()) {
		// Quick play joins "host:port"; an IPv6 address needs its brackets.
		const bool quickPlay =
			profile->hasTrait("feature:is_quick_play_multiplayer");
		launchScript += "serverAddress " +
						(quickPlay ? serverToJoin->quickPlayHost()
								   : serverToJoin->address) +
						"\n";
		launchScript +=
			"serverPort " + QString::number(serverToJoin->port) + "\n";
	} else if (serverToJoin && !serverToJoin->world.isEmpty()) {
		launchScript += "worldName " + serverToJoin->world + "\n";
	}

	// generic minecraft params
	for (auto param : processMinecraftArgs(
			 session, nullptr /* When using a launch script, the server
								 parameters are handled by it*/
			 )) {
		launchScript += "param " + param + "\n";
	}

	// window size, title and state, legacy
	{
		QString windowParams;
		if (settings()->get("LaunchMaximized").toBool())
			windowParams = "max";
		else
			windowParams =
				QString("%1x%2")
					.arg(settings()->get("MinecraftWinWidth").toInt())
					.arg(settings()->get("MinecraftWinHeight").toInt());
		launchScript += "windowTitle " + windowTitle() + "\n";
		launchScript += "windowParams " + windowParams + "\n";
	}

	// legacy auth
	if (session) {
		launchScript += "userName " + session->player_name + "\n";
		launchScript += "sessionId " + session->session + "\n";
	}

	// libraries and class path.
	{
		QStringList jars, nativeJars;
		profile->getLibraryFiles(runtimeContext(), jars, nativeJars,
								 getLocalLibraryPath(), binRoot());
		for (auto file : jars) {
			launchScript += "cp " + file + "\n";
		}
		for (auto file : nativeJars) {
			launchScript += "ext " + file + "\n";
		}
		launchScript += "natives " + getNativePath() + "\n";
	}

	for (auto trait : profile->getTraits()) {
		launchScript += "traits " + trait + "\n";
	}

	// Legacy (applet) instances run in-process via the OneSix launcher; the
	// rest spawn the game as a child process, so newer Java versions work.
	auto profileTraits = profile->getTraits();
	bool isLegacyApplet = profileTraits.contains("legacyLaunch") ||
						  profileTraits.contains("alphaLaunch");

	if (isLegacyApplet) {
		launchScript += "launcher onesix\n";
	} else {
		// The child process gets the configured Java and the same JVM
		// arguments.
		launchScript +=
			"javaPath " + settings()->get("JavaPath").toString() + "\n";

		for (auto arg : javaArguments()) {
			launchScript += "jvmArg " + arg + "\n";
		}
		// Kite's own mod rides on the game's JVM only, not the helper's.
		for (auto arg : kiteModArguments()) {
			launchScript += "jvmArg " + arg + "\n";
		}

		launchScript += "launcher modern\n";
	}

	return launchScript;
}

QStringList
MinecraftInstance::verboseDescription(AuthSessionPtr session,
									  MinecraftServerTargetPtr serverToJoin)
{
	QStringList out;
	out << "Main Class:" << "  " + getMainClass() << "";
	out << "Native path:" << "  " + getNativePath() << "";

	auto profile = m_components->getProfile();

	auto alltraits = traits();
	if (alltraits.size()) {
		out << "Traits:";
		for (auto trait : alltraits) {
			out << "traits " + trait;
		}
		out << "";
	}

	const auto kiteModArgs = kiteModArguments();
	if (!kiteModArgs.isEmpty()) {
		out << tr("Kite mod: %1").arg(QFileInfo(kiteModJar()).fileName())
			<< "";
	}

	auto settings = this->settings();
	bool nativeOpenAL = settings->get("UseNativeOpenAL").toBool();
	bool nativeGLFW = settings->get("UseNativeGLFW").toBool();
	if (nativeOpenAL || nativeGLFW) {
		if (nativeOpenAL)
			out << "Using system OpenAL.";
		if (nativeGLFW)
			out << "Using system GLFW.";
		out << "";
	}

	// libraries and class path.
	{
		out << "Libraries:";
		QStringList jars, nativeJars;
		profile->getLibraryFiles(runtimeContext(), jars, nativeJars,
								 getLocalLibraryPath(), binRoot());
		auto printLibFile = [&](const QString& path) {
			QFileInfo info(path);
			if (info.exists()) {
				out << "  " + path;
			} else {
				out << "  " + path + " (missing)";
			}
		};
		for (auto file : jars) {
			printLibFile(file);
		}
		out << "";
		out << "Native libraries:";
		for (auto file : nativeJars) {
			printLibFile(file);
		}
		out << "";
	}

	auto printModList = [&](const QString& label, ModFolderModel& model) {
		if (model.size()) {
			out << QString("%1:").arg(label);
			auto modList = model.allMods();
			std::sort(modList.begin(), modList.end(), [](Mod& a, Mod& b) {
				auto aName = a.filename().completeBaseName();
				auto bName = b.filename().completeBaseName();
				return aName.localeAwareCompare(bName) < 0;
			});
			for (auto& mod : modList) {
				if (mod.type() == Mod::MOD_FOLDER) {
					out << "  [📁] " + mod.filename().completeBaseName() +
							   " (folder)";
					continue;
				}

				if (mod.enabled()) {
					out << "  [✔️] " + mod.filename().completeBaseName();
				} else {
					out << "  [❌] " + mod.filename().completeBaseName() +
							   " (disabled)";
				}
			}
			out << "";
		}
	};

	printModList("Mods", *(loaderModList().get()));
	printModList("Core Mods", *(coreModList().get()));

	auto& jarMods = profile->getJarMods();
	if (jarMods.size()) {
		out << "Jar Mods:";
		for (auto& jarmod : jarMods) {
			auto displayname = jarmod->displayName(runtimeContext());
			auto realname = jarmod->filename(runtimeContext());
			if (displayname != realname) {
				out << "  " + displayname + " (" + realname + ")";
			} else {
				out << "  " + realname;
			}
		}
		out << "";
	}

	auto params = processMinecraftArgs(nullptr, serverToJoin);
	out << "Params:";
	out << "  " + params.join(' ');
	out << "";

	QString windowParams;
	if (settings->get("LaunchMaximized").toBool()) {
		out << "Window size: max (if available)";
	} else {
		auto width = settings->get("MinecraftWinWidth").toInt();
		auto height = settings->get("MinecraftWinHeight").toInt();
		out << "Window size: " + QString::number(width) + " x " +
				   QString::number(height);
	}
	out << "";
	return out;
}

QMap<QString, QString>
MinecraftInstance::createCensorFilterFromSession(AuthSessionPtr session)
{
	if (!session) {
		return QMap<QString, QString>();
	}
	auto& sessionRef = *session.get();
	QMap<QString, QString> filter;
	auto addToFilter = [&filter](QString key, QString value) {
		if (key.trimmed().size()) {
			filter[key] = value;
		}
	};
	if (sessionRef.session != "-") {
		addToFilter(sessionRef.session, tr("<SESSION ID>"));
	}
	addToFilter(sessionRef.access_token, tr("<ACCESS TOKEN>"));
	if (sessionRef.client_token.size()) {
		addToFilter(sessionRef.client_token, tr("<CLIENT TOKEN>"));
	}
	addToFilter(sessionRef.uuid, tr("<PROFILE ID>"));

	return filter;
}

MessageLevel::Enum MinecraftInstance::guessLevel(const QString& line,
												 MessageLevel::Enum level)
{
	QRegularExpression re(
		"\\[(?<timestamp>[0-9:]+)\\] \\[[^/]+/(?<level>[^\\]]+)\\]");
	auto match = re.match(line);
	if (match.hasMatch()) {
		// New style logs from log4j
		QString timestamp = match.captured("timestamp");
		QString levelStr = match.captured("level");
		if (levelStr == "INFO")
			level = MessageLevel::Message;
		if (levelStr == "WARN")
			level = MessageLevel::Warning;
		if (levelStr == "ERROR")
			level = MessageLevel::Error;
		if (levelStr == "FATAL")
			level = MessageLevel::Fatal;
		if (levelStr == "TRACE" || levelStr == "DEBUG")
			level = MessageLevel::Debug;
	} else {
		// Old style forge logs
		if (line.contains("[INFO]") || line.contains("[CONFIG]") ||
			line.contains("[FINE]") || line.contains("[FINER]") ||
			line.contains("[FINEST]"))
			level = MessageLevel::Message;
		if (line.contains("[SEVERE]") || line.contains("[STDERR]"))
			level = MessageLevel::Error;
		if (line.contains("[WARNING]"))
			level = MessageLevel::Warning;
		if (line.contains("[DEBUG]"))
			level = MessageLevel::Debug;
	}
	if (line.contains("overwriting existing"))
		return MessageLevel::Fatal;
	// NOTE: this diverges from the real regexp. no unicode, the first section
	// is + instead of *
	static const QString javaSymbol =
		"([a-zA-Z_$][a-zA-Z\\d_$]*\\.)+[a-zA-Z_$][a-zA-Z\\d_$]*";
	if (line.contains("Exception in thread") ||
		line.contains(QRegularExpression("\\s+at " + javaSymbol)) ||
		line.contains(QRegularExpression("Caused by: " + javaSymbol)) ||
		line.contains(
			QRegularExpression("([a-zA-Z_$][a-zA-Z\\d_$]*\\.)+[a-zA-Z_$]?[a-zA-"
							   "Z\\d_$]*(Exception|Error|Throwable)")) ||
		line.contains(QRegularExpression("... \\d+ more$")))
		return MessageLevel::Error;
	return level;
}

IPathMatcher::Ptr MinecraftInstance::getLogFileMatcher()
{
	auto combined = std::make_shared<MultiMatcher>();
	combined->add(
		std::make_shared<RegexpMatcher>(".*\\.log(\\.[0-9]*)?(\\.gz)?$"));
	combined->add(std::make_shared<RegexpMatcher>("crash-.*\\.txt"));
	combined->add(std::make_shared<RegexpMatcher>("IDMap dump.*\\.txt$"));
	combined->add(std::make_shared<RegexpMatcher>("ModLoader\\.txt(\\..*)?$"));
	return combined;
}

QString MinecraftInstance::getLogFileRoot()
{
	return gameRoot();
}

Task::Ptr MinecraftInstance::createUpdateTask(Net::Mode mode)
{
	switch (mode) {
		case Net::Mode::Offline: {
			return Task::Ptr(new MinecraftLoadAndCheck(this));
		}
		case Net::Mode::Online: {
			return Task::Ptr(new MinecraftUpdate(this));
		}
	}
	return nullptr;
}

shared_qobject_ptr<LaunchTask>
MinecraftInstance::createLaunchTask(AuthSessionPtr session,
									MinecraftServerTargetPtr serverToJoin)
{
	// FIXME: get rid of shared_from_this ...
	auto process = LaunchTask::create(
		std::dynamic_pointer_cast<MinecraftInstance>(shared_from_this()));
	auto pptr = process.get();

	LAUNCHER->icons()->saveIcon(
		iconKey(), FS::PathCombine(gameRoot(), "icon.png"), "PNG");

	// print a header
	{
		process->appendStep(
			new TextPrint(pptr, "Minecraft folder is:\n" + gameRoot() + "\n\n",
						  MessageLevel::Kite));
	}

	// check java
	{
		process->appendStep(new CheckJava(pptr));
	}

	// check launch method
	QStringList validMethods = {"KitePart", "DirectJava"};
	QString method = launchMethod();
	if (!validMethods.contains(method)) {
		process->appendStep(new TextPrint(
			pptr, "Selected launch method \"" + method + "\" is not valid.\n",
			MessageLevel::Fatal));
		return process;
	}

	// create the .minecraft folder and server-resource-packs (workaround for
	// Minecraft bug MCL-3732)
	{
		process->appendStep(new CreateGameFolders(pptr));
	}

	if (!serverToJoin && m_settings->get("JoinServerOnLaunch").toBool()) {
		QString fullAddress =
			m_settings->get("JoinServerOnLaunchAddress").toString();
		serverToJoin.reset(new MinecraftServerTarget(
			MinecraftServerTarget::parse(fullAddress, false)));
	}

	if (serverToJoin && !serverToJoin->address.isEmpty() &&
		serverToJoin->port == 25565) {
		// Resolve server address to join on launch
		auto* step = new LookupServerAddress(pptr);
		step->setLookupAddress(serverToJoin->address);
		step->setOutputAddressPtr(serverToJoin);
		process->appendStep(step);
	}

	// run pre-launch command if that's needed
	if (getPreLaunchCommand().size()) {
		auto step = new PreLaunchCommand(pptr);
		step->setWorkingDirectory(gameRoot());
		process->appendStep(step);
	}

	// Offline accounts still download a new instance once (fetch_game_files); a
	// signed-in account in offline mode stays strictly offline.
	const bool updateOnline = session->status != AuthSession::PlayableOffline ||
							  session->fetch_game_files;
	if (updateOnline) {
		// Only a signed-in session has anything to claim.
		if (!session->demo && session->status != AuthSession::PlayableOffline) {
			process->appendStep(new ClaimAccount(pptr, session));
		}
		// Performance mods go in before the update resolves the loader.
		if (m_settings->get("KitePerformance").toBool()) {
			process->appendStep(new PreparePerformance(pptr));
		}
		// Kite's mod is fetched here; an offline launch uses what is cached.
		if (kiteModApplies()) {
			process->appendStep(new PrepareKiteMod(pptr));
		}
		process->appendStep(new Update(pptr, [this] {
			return Task::Ptr(new MinecraftUpdate(
				this, MinecraftUpdate::Part::VersionInfo));
		}));
	} else {
		process->appendStep(new Update(pptr, Net::Mode::Offline));
	}

	// Set up the Java this version needs once the version info names it, before
	// anything depends on its architecture or version.
	{
		process->appendStep(new VerifyJavaInstall(pptr));
	}

	if (updateOnline) {
		process->appendStep(new Update(pptr, [this] {
			return Task::Ptr(
				new MinecraftUpdate(this, MinecraftUpdate::Part::GameFiles));
		}));
	}

	// if there are any jar mods
	{
		process->appendStep(new ModMinecraftJar(pptr));
	}

	// Scan mods folders for mods
	{
		process->appendStep(new ScanModFolders(pptr));
	}

	// print some instance info here...
	{
		process->appendStep(new PrintInstanceInfo(pptr, session, serverToJoin));
	}

	// extract native jars if needed
	{
		process->appendStep(new ExtractNatives(pptr));
	}

	// reconstruct assets if needed
	{
		process->appendStep(new ReconstructAssets(pptr));
	}

	// the Kite mod's way to the launcher, once the mod's jar is known
	if (kiteModApplies()) {
		process->appendStep(new StartModBridge(pptr));
	}

	{
		// actually launch the game
		auto method = launchMethod();
		if (method == "KitePart") {
			auto step = new KitePartLaunch(pptr);
			step->setWorkingDirectory(gameRoot());
			step->setAuthSession(session);
			step->setServerToJoin(serverToJoin);
			process->appendStep(step);
		} else if (method == "DirectJava") {
			auto step = new DirectJavaLaunch(pptr);
			step->setWorkingDirectory(gameRoot());
			step->setAuthSession(session);
			step->setServerToJoin(serverToJoin);
			process->appendStep(step);
		}
	}

	// run post-exit command if that's needed
	if (getPostExitCommand().size()) {
		auto step = new PostLaunchCommand(pptr);
		step->setWorkingDirectory(gameRoot());
		process->appendStep(step);
	}
	if (session) {
		process->setCensorFilter(createCensorFilterFromSession(session));
		// Only an online launch names the player, for FriendsGameWatcher.
		if (session->status == AuthSession::PlayableOnline && !session->demo) {
			process->setPlayerProfile(session->uuid);
		}
	}
	m_launchProcess = process;
	// Says why, if this run ends in a crash.
	CrashDoctorService::instance()->watch(m_launchProcess);
	emit launchTaskChanged(m_launchProcess);
	return m_launchProcess;
}

QString MinecraftInstance::launchMethod()
{
	return m_settings->get("MCLaunchMethod").toString();
}

JavaVersion MinecraftInstance::getJavaVersion() const
{
	return JavaVersion(settings()->get("JavaVersion").toString());
}

RuntimeContext MinecraftInstance::runtimeContext() const
{
	return RuntimeContext::forJava(
		settings()->get("JavaRealArchitecture").toString(),
		settings()->get("JavaArchitecture").toString());
}

std::shared_ptr<ModFolderModel> MinecraftInstance::loaderModList() const
{
	if (!m_loader_mod_list) {
		m_loader_mod_list.reset(new ModFolderModel(modsRoot()));
		m_loader_mod_list->disableInteraction(isRunning());
		connect(this, &BaseInstance::runningStatusChanged,
				m_loader_mod_list.get(), &ModFolderModel::disableInteraction);
	}
	return m_loader_mod_list;
}

std::shared_ptr<ModFolderModel> MinecraftInstance::coreModList() const
{
	if (!m_core_mod_list) {
		m_core_mod_list.reset(new ModFolderModel(coreModsDir()));
		m_core_mod_list->disableInteraction(isRunning());
		connect(this, &BaseInstance::runningStatusChanged,
				m_core_mod_list.get(), &ModFolderModel::disableInteraction);
	}
	return m_core_mod_list;
}

std::shared_ptr<ModFolderModel> MinecraftInstance::resourcePackList() const
{
	if (!m_resource_pack_list) {
		m_resource_pack_list.reset(
			new ResourcePackFolderModel(resourcePacksDir()));
		// Resource packs can be safely added while Minecraft is running
	}
	return m_resource_pack_list;
}

std::shared_ptr<ModFolderModel> MinecraftInstance::texturePackList() const
{
	if (!m_texture_pack_list) {
		m_texture_pack_list.reset(
			new TexturePackFolderModel(texturePacksDir()));
		m_texture_pack_list->disableInteraction(isRunning());
		connect(this, &BaseInstance::runningStatusChanged,
				m_texture_pack_list.get(), &ModFolderModel::disableInteraction);
	}
	return m_texture_pack_list;
}

std::shared_ptr<ModFolderModel> MinecraftInstance::shaderPackList() const
{
	if (!m_shader_pack_list) {
		m_shader_pack_list.reset(new ShaderPackFolderModel(shaderPacksDir()));
		// Shader packs can be safely added while Minecraft is running
	}
	return m_shader_pack_list;
}

std::shared_ptr<ModFolderModel> MinecraftInstance::dataPackList() const
{
	// Null when the global folder is switched off - callers must check.
	if (!m_data_pack_list &&
		settings()->get("GlobalDataPacksEnabled").toBool()) {
		m_data_pack_list.reset(new DataPackFolderModel(dataPacksDir()));
	}
	return m_data_pack_list;
}

std::shared_ptr<WorldList> MinecraftInstance::worldList() const
{
	if (!m_world_list) {
		m_world_list.reset(new WorldList(worldDir()));
	}
	return m_world_list;
}

std::shared_ptr<GameOptions> MinecraftInstance::gameOptionsModel() const
{
	if (!m_game_options) {
		m_game_options.reset(
			new GameOptions(FS::PathCombine(gameRoot(), "options.txt")));
	}
	return m_game_options;
}

QList<Mod> MinecraftInstance::getJarMods() const
{
	auto profile = m_components->getProfile();
	QList<Mod> mods;
	for (auto jarmod : profile->getJarMods()) {
		QStringList jar, temp1, temp2, temp3;
		jarmod->getApplicableFiles(runtimeContext(), jar, temp1, temp2, temp3,
								   jarmodsPath().absolutePath());
		mods.push_back(Mod(QFileInfo(jar[0])));
	}
	return mods;
}

#include "MinecraftInstance.moc"
