project(application)

include (UnitTest)

set(CORE_SOURCES
    # Core sources: no QtWidgets.
    core/LauncherContext.h
    core/LauncherContext.cpp

    DesktopServices.h
    DesktopServices.cpp
    core/AuthRequestDecorator.h
    core/UiHost.h
    core/InstancePathProblems.h
    core/InstancePathProblems.cpp

    theme/Contrast.h
    theme/Contrast.cpp
    theme/ThemePalette.h
    theme/ThemePalette.cpp

    models/InstanceFilterModel.h
    models/InstanceFilterModel.cpp
    models/IdSelectionModel.h
    models/IdSelectionModel.cpp
    models/SettingsAdapter.h
    models/SettingsAdapter.cpp
    models/InstanceDetails.h
    models/InstanceDetails.cpp
    models/ContentBrowser.h
    models/ContentBrowser.cpp
    models/LoaderInstaller.h
    models/LoaderInstaller.cpp
    models/OtherLogsModel.h
    models/OtherLogsModel.cpp
    models/LogUploader.h
    models/LogUploader.cpp
    models/LogFind.h
    models/LogFind.cpp
    models/LauncherLogs.h
    models/LauncherLogs.cpp
    models/ScreenshotUploader.h
    models/ScreenshotUploader.cpp
    models/WorldTools.h
    models/WorldTools.cpp
    models/LegacyUpgrade.h
    models/LegacyUpgrade.cpp
    models/KeyValueFilterModel.h
    models/KeyValueFilterModel.cpp
    models/AccountsController.h
    models/AccountsController.cpp
    models/NewInstanceController.h
    models/NewInstanceController.cpp
    models/RecentWorldsModel.h
    models/RecentWorldsModel.cpp
    models/AboutInfo.h
    models/AboutInfo.cpp
    models/NewsModel.h
    models/NewsModel.cpp
    models/SkinsController.h
    models/SkinsController.cpp
    models/ServersListModel.h
    models/ServersListModel.cpp
    models/BackupController.h
    models/BackupController.cpp
    models/WorldDataPacksController.h
    models/WorldDataPacksController.cpp
    models/ManagedPackController.h
    models/ManagedPackController.cpp
    models/ContentFilterModel.h
    models/ContentFilterModel.cpp
    models/ModHealthModel.h
    models/ModHealthModel.cpp
    models/ModHealthModelIds.h
    models/ModHealthText.cpp
    models/InstanceContentController.h
    models/InstanceContentController.cpp
    models/ContentInstallReview.h
    models/ContentInstallReview.cpp
    models/JavaController.h
    models/JavaController.cpp
    models/JavaDownloadController.h
    models/JavaDownloadController.cpp
    models/FolderSettingsController.h
    models/FolderSettingsController.cpp
    models/UpdateSettingsController.h
    models/UpdateSettingsController.cpp
    models/PlayTimeSummary.h
    models/PlayTimeSummary.cpp
    models/ComponentTools.h
    models/ComponentTools.cpp
    models/CatPackList.h
    models/CatPackList.cpp
    # From ui/themes, but QtCore only.
    ui/themes/CatPack.h
    ui/themes/CatPack.cpp
    models/ContentUpdateController.h
    models/ContentUpdateController.cpp
    models/AtlOptionalModsModel.h
    models/AtlOptionalModsModel.cpp
    models/InstanceExportController.h
    models/InstanceExportController.cpp
    models/InstanceShortcutController.h
    models/InstanceShortcutController.cpp
    models/LibraryController.h
    models/LibraryController.cpp
    models/RichBody.h
    models/RichBody.cpp
    modplatform/BlockedMod.h

    BaseInstaller.h
    BaseInstaller.cpp
    BaseVersionList.h
    BaseVersionList.cpp
    InstanceList.h
    InstanceList.cpp
    InstanceTask.h
    InstanceTask.cpp
    LoggedProcess.h
    LoggedProcess.cpp
    Logging.h
    Logging.cpp
    MessageLevel.cpp
    MessageLevel.h
    BaseVersion.h
    BaseInstance.h
    BaseInstance.cpp
    Sys.cpp
    Sys.h
    NullInstance.h
    QtCompat.h
    ArchiveOpen.h
    MMCZip.h
    MMCZip.cpp
    MMCStrings.h
    MMCStrings.cpp

    archive/ExportToZipTask.h
    archive/ExportToZipTask.cpp
    archive/ExtractZipTask.h
    archive/ExtractZipTask.cpp

    InstanceCreationTask.h
    InstanceCreationTask.cpp
    InstanceCopyTask.h
    InstanceCopyTask.cpp
    InstanceImportTask.h
    InstanceImportTask.cpp

    Usable.h

    SeparatorPrefixTree.h

    FileIgnoreProxy.h
    FileIgnoreProxy.cpp

    InstanceShortcuts.h
    InstanceShortcuts.cpp

    Filter.h
    Filter.cpp

    Json.h
    Json.cpp

    FileSystem.h
    FileSystem.cpp

    Exception.h

    RWStorage.h

    DefaultVariable.h

    QObjectPtr.h

    GZip.h
    GZip.cpp

    Commandline.h
    Commandline.cpp

    Version.h
    Version.cpp

    RecursiveFileSystemWatcher.h
    RecursiveFileSystemWatcher.cpp

    MMCTime.h
    MMCTime.cpp
)

add_unit_test(FileSystem
    SOURCES FileSystem_test.cpp
    LIBS Kite_logic
    DATA testdata
    )

add_unit_test(GZip
    SOURCES GZip_test.cpp
    LIBS Kite_logic
    )

add_unit_test(MMCZip
    SOURCES MMCZip_test.cpp
    LIBS Kite_logic
    )

set(PATHMATCHER_SOURCES
    pathmatcher/FSTreeMatcher.h
    pathmatcher/IPathMatcher.h
    pathmatcher/MultiMatcher.h
    pathmatcher/RegexpMatcher.h
)

set(NET_SOURCES
    net/ByteArraySink.h
    net/ChecksumValidator.h
    net/Download.cpp
    net/Download.h
    net/FileSink.cpp
    net/FileSink.h
    net/HttpMetaCache.cpp
    net/HttpMetaCache.h
    net/MetaCacheSink.cpp
    net/MetaCacheSink.h
    net/NetAction.h
    net/NetJob.cpp
    net/NetJob.h
    net/JsonPost.cpp
    net/JsonPost.h
    net/PasteUpload.cpp
    net/PasteUpload.h
    net/Sink.h
    net/Validator.h
)

add_unit_test(FileSink
    SOURCES net/FileSink_test.cpp
    LIBS Kite_logic
    )

set(LAUNCH_SOURCES
    launch/steps/CheckJava.cpp
    launch/steps/CheckJava.h
    launch/steps/CreateBackup.cpp
    launch/steps/CreateBackup.h
    launch/steps/LookupServerAddress.cpp
    launch/steps/LookupServerAddress.h
    launch/steps/PostLaunchCommand.cpp
    launch/steps/PostLaunchCommand.h
    launch/steps/PreLaunchCommand.cpp
    launch/steps/PreLaunchCommand.h
    launch/steps/TextPrint.cpp
    launch/steps/TextPrint.h
    launch/steps/Update.cpp
    launch/steps/Update.h
    launch/LaunchStep.cpp
    launch/LaunchStep.h
    launch/LaunchTask.cpp
    launch/LaunchTask.h
    launch/LaunchProgressTracker.cpp
    launch/LaunchProgressTracker.h
    launch/LogModel.cpp
    launch/LogModel.h
)

# Sparkle is not packaged in vcpkg: fetch the release archive, hash-checked.
# Probed before the source list: MacSparkleUpdater cannot compile without the framework's headers.
set(Kite_SPARKLE_ENABLED OFF)
if(APPLE AND NOT "${MACOSX_SPARKLE_UPDATE_PUBLIC_KEY}" STREQUAL "" AND NOT "${MACOSX_SPARKLE_UPDATE_FEED_URL}" STREQUAL "")
    if(NOT EXISTS "${MACOSX_SPARKLE_DIR}/Sparkle.framework")
        message(STATUS "Downloading Sparkle from ${MACOSX_SPARKLE_DOWNLOAD_URL}")
        file(DOWNLOAD "${MACOSX_SPARKLE_DOWNLOAD_URL}" "${CMAKE_BINARY_DIR}/Sparkle.tar.xz"
            EXPECTED_HASH SHA256=${MACOSX_SPARKLE_SHA256})
        file(ARCHIVE_EXTRACT INPUT "${CMAKE_BINARY_DIR}/Sparkle.tar.xz"
            DESTINATION "${MACOSX_SPARKLE_DIR}")
    endif()

    find_library(SPARKLE_FRAMEWORK Sparkle PATHS "${MACOSX_SPARKLE_DIR}" NO_DEFAULT_PATH)
    if(SPARKLE_FRAMEWORK)
        set(Kite_SPARKLE_ENABLED ON)
        message(STATUS "Sparkle updater: ${SPARKLE_FRAMEWORK}")
    else()
        message(WARNING "Sparkle.framework was not found under ${MACOSX_SPARKLE_DIR}; macOS updates will be disabled.")
    endif()
endif()

# The launcher drives the standalone kite-updater; macOS uses Sparkle instead.
set(UPDATE_SOURCES
    updater/ExternalUpdater.h
    updater/UpdateLockFile.h
    updater/UpdateLockFile.cpp
)

if(APPLE)
    if(Kite_SPARKLE_ENABLED)
        list(APPEND UPDATE_SOURCES
            updater/MacSparkleUpdater.h
            updater/MacSparkleUpdater.mm
        )
    endif()
else()
    list(APPEND UPDATE_SOURCES
        updater/KiteExternalUpdater.h
        updater/KiteExternalUpdater.cpp
    )
endif()

set(NOTIFICATIONS_SOURCES
    notifications/NotificationChecker.h
    notifications/NotificationChecker.cpp
)

set(NEWS_SOURCES
    news/NewsChecker.h
    news/NewsChecker.cpp
    news/NewsEntry.h
    news/NewsEntry.cpp
)

set(BACKUP_SOURCES
    backup/BackupManager.h
    backup/BackupManager.cpp
    backup/BackupTask.h
    backup/BackupTask.cpp
)

set(ICONS_SOURCES
    icons/IconUtils.h
    icons/IconUtils.cpp
    icons/MMCIcon.h
    icons/MMCIcon.cpp
    icons/IconList.h
    icons/IconList.cpp
    icons/AutoIcons.h
    icons/AutoIcons.cpp
    icons/GameTextureIcons.h
    icons/GameTextureIcons.cpp
    icons/GameTextureRecipes.h
    icons/GameTextureRecipes.cpp
)

set(MINECRAFT_SOURCES
    minecraft/PresetDefaults.h
    minecraft/PresetDefaults.cpp
    minecraft/GraphicsBackend.h
    minecraft/GraphicsBackend.cpp
    minecraft/KiteModRules.h
    minecraft/KiteModRules.cpp
    minecraft/PerformanceSetupRules.h
    minecraft/PerformanceSetupRules.cpp
    minecraft/PerformanceSetupTask.h
    minecraft/PerformanceSetupTask.cpp
    minecraft/PerformanceInstanceTask.h
    minecraft/PerformanceInstanceTask.cpp
    minecraft/auth/AccountData.cpp
    minecraft/auth/AccountData.h
    minecraft/auth/AccountList.cpp
    minecraft/auth/AccountList.h
    minecraft/auth/AccountTask.cpp
    minecraft/auth/AccountTask.h
    minecraft/auth/AuthRequest.cpp
    minecraft/auth/AuthRequest.h
    minecraft/auth/AuthSession.cpp
    minecraft/auth/AuthSession.h
    minecraft/auth/AuthStep.cpp
    minecraft/auth/AuthStep.h
    minecraft/auth/MinecraftAccount.cpp
    minecraft/auth/MinecraftAccount.h
    minecraft/auth/Parsers.cpp
    minecraft/auth/Parsers.h
    minecraft/auth/flows/AuthFlow.cpp
    minecraft/auth/flows/AuthFlow.h
    minecraft/auth/flows/MSA.cpp
    minecraft/auth/flows/MSA.h

    minecraft/auth/steps/EntitlementsStep.cpp
    minecraft/auth/steps/EntitlementsStep.h
    minecraft/auth/steps/GetSkinStep.cpp
    minecraft/auth/steps/GetSkinStep.h
    minecraft/auth/steps/KiteLoginStep.cpp
    minecraft/auth/steps/KiteLoginStep.h
    minecraft/auth/steps/MinecraftProfileStep.cpp
    minecraft/auth/steps/MinecraftProfileStep.h
    minecraft/auth/steps/MSAStep.cpp
    minecraft/auth/steps/MSAStep.h
    minecraft/auth/steps/XboxAuthorizationStep.cpp
    minecraft/auth/steps/XboxAuthorizationStep.h
    minecraft/auth/steps/XboxUserStep.cpp
    minecraft/auth/steps/XboxUserStep.h
    minecraft/gameoptions/GameOptions.h
    minecraft/gameoptions/GameOptions.cpp

    minecraft/update/AssetUpdateTask.h
    minecraft/update/AssetUpdateTask.cpp
    minecraft/update/FMLLibrariesTask.cpp
    minecraft/update/FMLLibrariesTask.h
    minecraft/update/FoldersTask.cpp
    minecraft/update/FoldersTask.h
    minecraft/update/LibrariesTask.cpp
    minecraft/update/LibrariesTask.h

    minecraft/launch/ClaimAccount.cpp
    minecraft/launch/ClaimAccount.h
    minecraft/launch/CreateGameFolders.cpp
    minecraft/launch/CreateGameFolders.h
    minecraft/launch/ModMinecraftJar.cpp
    minecraft/launch/ModMinecraftJar.h
    minecraft/launch/DirectJavaLaunch.cpp
    minecraft/launch/DirectJavaLaunch.h
    minecraft/launch/ExtractNatives.cpp
    minecraft/launch/ExtractNatives.h
    minecraft/launch/KitePartLaunch.cpp
    minecraft/launch/KitePartLaunch.h
    minecraft/launch/MinecraftServerTarget.cpp
    minecraft/launch/MinecraftServerTarget.h
    minecraft/launch/PrepareKiteMod.cpp
    minecraft/launch/PrepareKiteMod.h
    minecraft/launch/StartModBridge.cpp
    minecraft/launch/StartModBridge.h
    minecraft/launch/PreparePerformance.cpp
    minecraft/launch/PreparePerformance.h
    minecraft/launch/PrintInstanceInfo.cpp
    minecraft/launch/PrintInstanceInfo.h
    minecraft/launch/ReconstructAssets.cpp
    minecraft/launch/ReconstructAssets.h
    minecraft/launch/ScanModFolders.cpp
    minecraft/launch/ScanModFolders.h
    minecraft/launch/VerifyJavaInstall.cpp
    minecraft/launch/VerifyJavaInstall.h

    minecraft/legacy/LegacyModList.h
    minecraft/legacy/LegacyModList.cpp
    minecraft/legacy/LegacyInstance.h
    minecraft/legacy/LegacyInstance.cpp
    minecraft/legacy/LegacyUpgradeTask.h
    minecraft/legacy/LegacyUpgradeTask.cpp

    minecraft/GradleSpecifier.h
    minecraft/MinecraftInstance.cpp
    minecraft/MinecraftInstance.h
    minecraft/LaunchProfile.cpp
    minecraft/LaunchProfile.h
    minecraft/Component.cpp
    minecraft/Component.h
    minecraft/PackProfile.cpp
    minecraft/PackProfile.h
    minecraft/ComponentUpdateTask.cpp
    minecraft/ComponentUpdateTask.h
    minecraft/MinecraftLoadAndCheck.h
    minecraft/MinecraftLoadAndCheck.cpp
    minecraft/MinecraftUpdate.h
    minecraft/MinecraftUpdate.cpp
    minecraft/MojangVersionFormat.cpp
    minecraft/MojangVersionFormat.h
    minecraft/Rule.cpp
    minecraft/Rule.h
    minecraft/RuntimeContext.cpp
    minecraft/RuntimeContext.h
    minecraft/OneSixVersionFormat.cpp
    minecraft/OneSixVersionFormat.h
    minecraft/OpSys.cpp
    minecraft/OpSys.h
    minecraft/ParseUtils.cpp
    minecraft/ParseUtils.h
    minecraft/ProfileUtils.cpp
    minecraft/ProfileUtils.h
    minecraft/Library.cpp
    minecraft/Library.h
    minecraft/MojangDownloadInfo.h
    minecraft/VersionFile.cpp
    minecraft/VersionFile.h
    minecraft/VersionFilterData.h
    minecraft/VersionFilterData.cpp
    minecraft/World.h
    minecraft/World.cpp
    minecraft/WorldList.h
    minecraft/WorldList.cpp

    minecraft/mod/Mod.h
    minecraft/mod/Mod.cpp
    minecraft/mod/ModDetails.h
    minecraft/mod/ModFolderModel.h
    minecraft/mod/ModFolderModel.cpp
    minecraft/mod/ModFolderLoadTask.h
    minecraft/mod/ModFolderLoadTask.cpp
    minecraft/mod/LocalModParseTask.h
    minecraft/mod/LocalModParseTask.cpp
    minecraft/mod/ModMetadataReader.h
    minecraft/mod/ModMetadataReader.cpp
    minecraft/mod/ModMetadataFormats.h
    minecraft/mod/ModMetadataToml.cpp
    minecraft/mod/ModMetadataFabricQuilt.cpp
    minecraft/mod/ModMetadataLegacy.cpp
    minecraft/mod/ModIconCache.h
    minecraft/mod/ModIconCache.cpp
    minecraft/mod/ModMetadataIndex.h
    minecraft/mod/ModMetadataIndex.cpp
    minecraft/mod/PackwizSidecar.h
    minecraft/mod/PackwizSidecar.cpp
    minecraft/mod/PackLayout.h
    minecraft/mod/PackLayout.cpp
    minecraft/mod/VersionRange.h
    minecraft/mod/VersionRange.cpp
    minecraft/mod/VersionRangeCommon.h
    minecraft/mod/VersionRangeFabric.h
    minecraft/mod/VersionRangeMaven.h
    minecraft/mod/ModHealth.h
    minecraft/mod/ModHealth.cpp
    minecraft/mod/ModHealthIndex.h
    minecraft/mod/ModHealthLoaders.h
    minecraft/mod/ModHealthLoaders.cpp
    minecraft/mod/ResourcePackFolderModel.h
    minecraft/mod/ResourcePackFolderModel.cpp
    minecraft/mod/TexturePackFolderModel.h
    minecraft/mod/TexturePackFolderModel.cpp
    minecraft/mod/ShaderPackFolderModel.h
    minecraft/mod/ShaderPackFolderModel.cpp
    minecraft/mod/DataPackFolderModel.h
    minecraft/mod/DataPackFolderModel.cpp

    minecraft/AssetsUtils.h
    minecraft/AssetsUtils.cpp

    minecraft/services/CapeChange.cpp
    minecraft/services/CapeChange.h
    minecraft/services/SkinUpload.cpp
    minecraft/services/SkinUpload.h
    minecraft/services/SkinDelete.cpp
    minecraft/services/SkinDelete.h

    minecraft/skins/SkinEntry.cpp
    minecraft/skins/SkinEntry.h
    minecraft/skins/SkinLibrary.cpp
    minecraft/skins/SkinLibrary.h
    minecraft/skins/ProfileSkinImport.cpp
    minecraft/skins/ProfileSkinImport.h
    minecraft/skins/DefaultSkins.cpp
    minecraft/skins/DefaultSkins.h
    minecraft/skins/SkinFace.cpp
    minecraft/skins/SkinFace.h

    mojang/PackageManifest.h
    mojang/PackageManifest.cpp
    )

add_unit_test(GradleSpecifier
    SOURCES minecraft/GradleSpecifier_test.cpp
    LIBS Kite_logic
    )

add_executable(PackageManifest
    mojang/PackageManifest_test.cpp
)
target_link_libraries(PackageManifest
    Kite_logic
    Qt${QT_VERSION_MAJOR}::Test
)
target_include_directories(PackageManifest
    PRIVATE ../cmake/UnitTest/
)
add_test(
    NAME PackageManifest
    COMMAND PackageManifest
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_unit_test(MojangVersionFormat
    SOURCES minecraft/MojangVersionFormat_test.cpp
    LIBS Kite_logic
    DATA minecraft/testdata
    )

add_unit_test(Library
    SOURCES minecraft/Library_test.cpp
    LIBS Kite_logic
    )

add_unit_test(LaunchPreflight
    SOURCES minecraft/LaunchPreflight_test.cpp
    LIBS Kite_core
    )

add_unit_test(MinecraftServerTarget
    SOURCES minecraft/launch/MinecraftServerTarget_test.cpp
    LIBS Kite_core
    )

# FIXME: shares data with FileSystem test
add_unit_test(ModFolderModel
    SOURCES minecraft/mod/ModFolderModel_test.cpp
    DATA testdata
    LIBS Kite_logic
    )

add_unit_test(LocalModParseTask
    SOURCES minecraft/mod/LocalModParseTask_test.cpp
    LIBS Kite_logic
    )

add_unit_test(VersionRange
    SOURCES minecraft/mod/VersionRange_test.cpp
    LIBS Kite_core
    )

add_unit_test(ModHealth
    SOURCES minecraft/mod/ModHealth_test.cpp
    LIBS Kite_core
    )

add_unit_test(GameOptions
    SOURCES minecraft/gameoptions/GameOptions_test.cpp
    LIBS Kite_core
    )

add_unit_test(ParseUtils
    SOURCES minecraft/ParseUtils_test.cpp
    LIBS Kite_logic
    )

add_unit_test(World
    SOURCES minecraft/World_test.cpp
    LIBS Kite_logic
    )

set(SCREENSHOTS_SOURCES
    screenshots/Screenshot.h
    screenshots/ImgurUpload.h
    screenshots/ImgurUpload.cpp
    screenshots/ImgurAlbumCreation.h
    screenshots/ImgurAlbumCreation.cpp
    screenshots/ScreenshotListModel.h
    screenshots/ScreenshotListModel.cpp
)

add_unit_test(ScreenshotListModel
    SOURCES screenshots/ScreenshotListModel_test.cpp
    LIBS Kite_core
    )

set(TASKS_SOURCES
    tasks/Task.h
    tasks/Task.cpp
    tasks/ConcurrentTask.h
    tasks/ConcurrentTask.cpp
    tasks/SequentialTask.h
    tasks/SequentialTask.cpp
    tasks/TaskWatcher.h
    tasks/TaskWatcher.cpp
)

set(SETTINGS_SOURCES
    settings/INIFile.cpp
    settings/INIFile.h
    settings/INISettingsObject.cpp
    settings/INISettingsObject.h
    settings/OverrideSetting.cpp
    settings/OverrideSetting.h
    settings/PassthroughSetting.cpp
    settings/PassthroughSetting.h
    settings/Setting.cpp
    settings/Setting.h
    settings/SettingsObject.cpp
    settings/SettingsObject.h
)

add_unit_test(INIFile
    SOURCES settings/INIFile_test.cpp
    LIBS Kite_logic
    )

set(JAVA_SOURCES
    java/JavaChecker.h
    java/JavaChecker.cpp
    java/JavaCheckerJob.h
    java/JavaCheckerJob.cpp
    java/JavaInstall.h
    java/JavaInstall.cpp
    java/JavaInstallList.h
    java/JavaInstallList.cpp
    java/JavaUtils.h
    java/JavaUtils.cpp
    java/JavaVersion.h
    java/JavaVersion.cpp
    java/JavaArgs.h
    java/JavaArgs.cpp
)

if(NOT Kite_DISABLE_JAVA_DOWNLOADER)
    list(APPEND JAVA_SOURCES
        java/download/JavaRuntime.h
        java/download/JavaDownloadTask.h
        java/download/JavaDownloadTask.cpp
    )
endif()

add_unit_test(JavaVersion
    SOURCES java/JavaVersion_test.cpp
    LIBS Kite_logic
    )

set(TRANSLATIONS_SOURCES
    translations/TranslationsModel.h
    translations/TranslationsModel.cpp
    translations/POTranslator.h
    translations/POTranslator.cpp
)

add_unit_test(TranslationsModel
    SOURCES translations/TranslationsModel_test.cpp
    LIBS Kite_core
    )

set(TOOLS_SOURCES
    tools/BaseExternalTool.cpp
    tools/BaseExternalTool.h
    tools/BaseProfiler.cpp
    tools/BaseProfiler.h
    tools/JProfiler.cpp
    tools/JProfiler.h
    tools/JVisualVM.cpp
    tools/JVisualVM.h
    tools/MCEditTool.cpp
    tools/MCEditTool.h
)

set(META_SOURCES
    meta/JsonFormat.cpp
    meta/JsonFormat.h
    meta/BaseEntity.cpp
    meta/BaseEntity.h
    meta/VersionList.cpp
    meta/VersionList.h
    meta/Version.cpp
    meta/Version.h
    meta/Index.cpp
    meta/Index.h
)

set(FTB_SOURCES
    modplatform/legacy_ftb/PackFetchTask.h
    modplatform/legacy_ftb/PackFetchTask.cpp
    modplatform/legacy_ftb/PackInstallTask.h
    modplatform/legacy_ftb/PackInstallTask.cpp
    modplatform/legacy_ftb/PrivatePackManager.h
    modplatform/legacy_ftb/PrivatePackManager.cpp

    modplatform/legacy_ftb/PackHelpers.h
)

set(FLAME_SOURCES
    modplatform/flame/FlamePackIndex.cpp
    modplatform/flame/FlamePackIndex.h
    modplatform/flame/PackManifest.h
    modplatform/flame/PackManifest.cpp
    modplatform/flame/FileResolvingTask.h
    modplatform/flame/FileResolvingTask.cpp
    modplatform/flame/FlameContentModel.h
    modplatform/flame/FlameContentModel.cpp
    modplatform/flame/FlameApi.h
    modplatform/flame/FlameApi.cpp
    modplatform/flame/FlameFingerprint.h
    modplatform/flame/FlameFingerprint.cpp
    modplatform/flame/FlamePackExportTask.h
    modplatform/flame/FlamePackExportTask.cpp
)

set(MODPACKSCH_SOURCES
    modplatform/modpacksch/FTBPackInstallTask.h
    modplatform/modpacksch/FTBPackInstallTask.cpp
    modplatform/modpacksch/FTBPackManifest.h
    modplatform/modpacksch/FTBPackManifest.cpp
    modplatform/modpacksch/FtbModpackModel.h
    modplatform/modpacksch/FtbModpackModel.cpp
)

set(TECHNIC_SOURCES
    modplatform/technic/SingleZipPackInstallTask.h
    modplatform/technic/SingleZipPackInstallTask.cpp
    modplatform/technic/SolderPackInstallTask.h
    modplatform/technic/SolderPackInstallTask.cpp
    modplatform/technic/TechnicPackProcessor.h
    modplatform/technic/TechnicPackProcessor.cpp
    modplatform/technic/TechnicModpackModel.h
    modplatform/technic/TechnicModpackModel.cpp
)

set(ATLAUNCHER_SOURCES
    modplatform/atlauncher/ATLPackIndex.cpp
    modplatform/atlauncher/ATLPackIndex.h
    modplatform/atlauncher/ATLPackInstallTask.cpp
    modplatform/atlauncher/ATLPackInstallTask.h
    modplatform/atlauncher/ATLPackManifest.cpp
    modplatform/atlauncher/ATLPackManifest.h
    modplatform/atlauncher/AtlModpackModel.h
    modplatform/atlauncher/AtlModpackModel.cpp
)

set(MODRINTH_SOURCES
    modplatform/modrinth/ModrinthPackIndex.cpp
    modplatform/modrinth/ModrinthPackIndex.h
    modplatform/modrinth/ModrinthPackManifest.cpp
    modplatform/modrinth/ModrinthPackManifest.h
    modplatform/modrinth/ModrinthContentModel.h
    modplatform/modrinth/ModrinthContentModel.cpp
    modplatform/modrinth/ModrinthApi.h
    modplatform/modrinth/ModrinthApi.cpp
    modplatform/modrinth/ModrinthPackExportTask.h
    modplatform/modrinth/ModrinthPackExportTask.cpp
    modplatform/modrinth/ModrinthModpackModel.h
    modplatform/modrinth/ModrinthModpackModel.cpp
    modplatform/flame/FlameModpackModel.h
    modplatform/flame/FlameModpackModel.cpp
)

set(CONTENT_DOWNLOAD_SOURCES
    modplatform/ContentApi.h
    modplatform/ManagedPackVersions.h
    modplatform/ManagedPackVersions.cpp
    modplatform/PackContents.h
    modplatform/PackContents.cpp
    modplatform/VersionPicker.h
    modplatform/VersionPicker.cpp
    modplatform/ContentProviderModel.h
    modplatform/ContentProviderModel.cpp
    modplatform/ContentListExport.h
    modplatform/ContentListExport.cpp
    modplatform/ContentType.h
    modplatform/ModDownloadTypes.h
    modplatform/DependencyResolver.h
    modplatform/DependencyResolver.cpp
    modplatform/BundleCatalog.h
    modplatform/BundleCatalog.cpp
    modplatform/ContentDownloadTask.h
    modplatform/ContentDownloadTask.cpp
    modplatform/ModInstallConflictAnalyzer.h
    modplatform/ModInstallConflictAnalyzer.cpp
    modplatform/ModUpdateCheckTask.h
    modplatform/ModUpdateCheckTask.cpp
)

add_unit_test(PackContents
    SOURCES modplatform/PackContents_test.cpp
    LIBS Kite_logic
    )

set(FRIENDS_SOURCES
    friends/FriendsApi.h
    friends/FriendsApi.cpp
    friends/FriendsDisplay.h
    friends/FriendsDisplay.cpp
    friends/FriendsFaces.h
    friends/FriendsFaces.cpp
    friends/FriendsLogRules.h
    friends/FriendsLogRules.cpp
    friends/FriendsSessionStore.h
    friends/FriendsSessionStore.cpp
    friends/FriendsHttp.h
    friends/FriendsHttp.cpp
    friends/FriendsGameWatcher.h
    friends/FriendsGameWatcher.cpp
    friends/FriendsController.h
    friends/FriendsController.cpp
    friends/FriendsSignIn.cpp
    friends/FriendsPoll.cpp
    friends/FriendsActions.cpp
    friends/FriendsBridge.cpp
    modbridge/ModBridgeProtocol.h
    modbridge/ModBridgeProtocol.cpp
    modbridge/ModBridgeAnswers.cpp
    modbridge/ModBridgeJson.h
    modbridge/ModBridgeJson.cpp
    modbridge/ModBridgeSource.h
    modbridge/FriendsBridgeSource.h
    modbridge/FriendsBridgeSource.cpp
    modbridge/ModBridge.h
    modbridge/ModBridge.cpp
    modbridge/ModBridgeRoutes.cpp
)

add_unit_test(FriendsApi
    SOURCES friends/FriendsApi_test.cpp
    LIBS Kite_core
    )

add_unit_test(FriendsLogRules
    SOURCES friends/FriendsLogRules_test.cpp
    LIBS Kite_core
    )

add_unit_test(FriendsGameWatcher
    SOURCES friends/FriendsGameWatcher_test.cpp
    LIBS Kite_core
    )

add_unit_test(FriendsFaces
    SOURCES friends/FriendsFaces_test.cpp
    LIBS Kite_core
    )

add_unit_test(ModBridgeProtocol
    SOURCES modbridge/ModBridgeProtocol_test.cpp
    LIBS Kite_core
    )

add_unit_test(ModBridge
    SOURCES modbridge/ModBridge_test.cpp
    LIBS Kite_core
    )

set(PLUGIN_SOURCES
    plugin/PluginAuthRequestDecorator.h
    plugin/PluginAuthRequestDecorator.cpp

    plugin/MMCOFormat.h
    plugin/PluginHooks.h
    plugin/PluginAPI.h
    plugin/PluginMetadata.h
    plugin/CoreSupersededPlugins.h
    plugin/PluginLoader.h
    plugin/PluginLoader.cpp
    plugin/PluginManager.h
    plugin/PluginManager.cpp
    plugin/PluginUiRenderer.h
    plugin/PluginUiRenderer.cpp
    plugin/PluginHookTask.h
    plugin/PluginHookTask.cpp
    plugin/PluginSignature.h
    plugin/PluginSignature.cpp
    plugin/PluginDependencyResolver.h
    plugin/PluginDependencyResolver.cpp
    plugin/PluginSurfaceModel.h
    plugin/PluginSurfaceModel.cpp
    plugin/PluginListModel.h
    plugin/PluginListModel.cpp
)

add_unit_test(Index
    SOURCES meta/Index_test.cpp
    LIBS Kite_logic
    )

# zlib, libarchive, cmark and toml++ come from the top-level CMakeLists.txt; nbt++ is built here.

set(LOGIC_SOURCES
    ${CORE_SOURCES}
    ${PATHMATCHER_SOURCES}
    ${NET_SOURCES}
    ${LAUNCH_SOURCES}
    ${UPDATE_SOURCES}
    ${NOTIFICATIONS_SOURCES}
    ${NEWS_SOURCES}
    ${BACKUP_SOURCES}
    ${MINECRAFT_SOURCES}
    ${SCREENSHOTS_SOURCES}
    ${TASKS_SOURCES}
    ${SETTINGS_SOURCES}
    ${JAVA_SOURCES}
    ${TRANSLATIONS_SOURCES}
    ${TOOLS_SOURCES}
    ${META_SOURCES}
    ${ICONS_SOURCES}
    ${FTB_SOURCES}
    ${FLAME_SOURCES}
    ${MODPACKSCH_SOURCES}
    ${TECHNIC_SOURCES}
    ${ATLAUNCHER_SOURCES}
    ${MODRINTH_SOURCES}
    ${CONTENT_DOWNLOAD_SOURCES}
    ${FRIENDS_SOURCES}
)

SET(KITE_SOURCES
    Application.h
    Application.cpp
    ApplicationMessage.h
    ApplicationMessage.cpp

    VersionProxyModel.h
    VersionProxyModel.cpp
    HoeDown.h

    KonamiCode.h
    KonamiCode.cpp

    resources/backgrounds/backgrounds.qrc
    resources/multimc/multimc.qrc
    resources/pe_dark/pe_dark.qrc
    resources/pe_light/pe_light.qrc
    resources/pe_colored/pe_colored.qrc
    resources/pe_blue/pe_blue.qrc
    resources/OSX/OSX.qrc
    resources/iOS/iOS.qrc
    resources/flat/flat.qrc
    resources/flat_white/flat_white.qrc
    resources/documents/documents.qrc
    resources/breeze_dark/breeze_dark.qrc
    resources/breeze_light/breeze_light.qrc
    resources/shaders/shaders.qrc
    ${CMAKE_BINARY_DIR}/${Kite_AppBinaryName}.qrc

    ui/GuiUtil.h
    ui/GuiUtil.cpp
    ui/ColorCache.h
    ui/ColorCache.cpp
    ui/MainWindow.h
    ui/ShortcutUtils.cpp
    ui/ShortcutUtils.h
    ui/FastFileIconProvider.cpp
    ui/FastFileIconProvider.h
    ui/WidgetUiHost.cpp
    ui/WidgetUiHost.h
    ui/MainWindow.cpp
    ui/MacMenuBar.h
    ui/MacMenuBar.cpp
    ui/InstanceWindow.h
    ui/InstanceWindow.cpp

    # FIXME: maybe find a better home for this.
    SkinUtils.cpp
    SkinUtils.h

    ui/setupwizard/SetupWizard.h
    ui/setupwizard/SetupWizard.cpp
    ui/setupwizard/BaseWizardPage.h
    ui/setupwizard/JavaWizardPage.cpp
    ui/setupwizard/JavaWizardPage.h
    ui/setupwizard/LanguageWizardPage.cpp
    ui/setupwizard/LanguageWizardPage.h

    ui/themes/FusionTheme.cpp
    ui/themes/FusionTheme.h
    ui/themes/BrightTheme.cpp
    ui/themes/BrightTheme.h
    ui/themes/GreenDarkTheme.cpp
    ui/themes/GreenDarkTheme.h
    ui/themes/GreenLightTheme.cpp
    ui/themes/GreenLightTheme.h
    ui/themes/CustomTheme.cpp
    ui/themes/CustomTheme.h
    ui/themes/DarkTheme.cpp
    ui/themes/DarkTheme.h
    ui/themes/ITheme.cpp
    ui/themes/ITheme.h
    ui/themes/SystemTheme.cpp
    ui/themes/SystemTheme.h
    ui/themes/ThemeManager.cpp
    ui/themes/ThemeManager.h

    ui/LaunchController.h
    ui/LaunchController.cpp

    ui/InstancePageProvider.h

    ui/JavaCommon.h
    ui/JavaCommon.cpp

    ui/pages/BasePage.h
    ui/pages/BasePageContainer.h
    ui/pages/BasePageProvider.h

    ui/pages/instance/BackupPage.cpp
    ui/pages/instance/BackupPage.h
    ui/pages/instance/GameOptionsPage.cpp
    ui/pages/instance/GameOptionsPage.h
    ui/pages/instance/VersionPage.cpp
    ui/pages/instance/VersionPage.h
    ui/pages/instance/TexturePackPage.h
    ui/pages/instance/ResourcePackPage.h
    ui/pages/instance/ShaderPackPage.h
    ui/pages/instance/DataPackPage.h
    ui/pages/instance/DataPackPage.cpp
    ui/pages/instance/ModFolderPage.cpp
    ui/pages/instance/ModFolderPage.h
    ui/pages/instance/NotesPage.cpp
    ui/pages/instance/NotesPage.h
    ui/pages/instance/LogPage.cpp
    ui/pages/instance/LogPage.h
    ui/pages/instance/InstanceSettingsPage.cpp
    ui/pages/instance/InstanceSettingsPage.h
    ui/pages/instance/ScreenshotsPage.cpp
    ui/pages/instance/ScreenshotsPage.h
    ui/pages/instance/OtherLogsPage.cpp
    ui/pages/instance/OtherLogsPage.h
    ui/pages/instance/ServersPage.cpp
    ui/pages/instance/ServersPage.h
    ui/pages/instance/LegacyUpgradePage.cpp
    ui/pages/instance/LegacyUpgradePage.h
    ui/pages/instance/ManagedPackPage.cpp
    ui/pages/instance/ManagedPackPage.h
    ui/pages/instance/WorldListPage.cpp
    ui/pages/instance/WorldListPage.h

    ui/pages/global/AccountListPage.cpp
    ui/pages/global/AccountListPage.h
    ui/pages/global/AppearancePage.cpp
    ui/pages/global/AppearancePage.h
    ui/pages/global/CustomCommandsPage.cpp
    ui/pages/global/CustomCommandsPage.h
    ui/pages/global/ExternalToolsPage.cpp
    ui/pages/global/ExternalToolsPage.h
    ui/pages/global/JavaPage.cpp
    ui/pages/global/JavaPage.h
    ui/pages/global/LanguagePage.cpp
    ui/pages/global/LanguagePage.h
    ui/pages/global/MinecraftPage.cpp
    ui/pages/global/MinecraftPage.h
    ui/pages/global/KitePage.cpp
    ui/pages/global/KitePage.h
    ui/pages/global/ProxyPage.cpp
    ui/pages/global/ProxyPage.h
    ui/pages/global/PasteEEPage.cpp
    ui/pages/global/PasteEEPage.h

    ui/pages/modplatform/VanillaPage.cpp
    ui/pages/modplatform/VanillaPage.h

    ui/pages/modplatform/ContentProviderPage.cpp
    ui/pages/modplatform/ContentProviderPage.h

    ui/pages/modplatform/atlauncher/AtlFilterModel.cpp
    ui/pages/modplatform/atlauncher/AtlFilterModel.h
    ui/pages/modplatform/atlauncher/AtlListModel.cpp
    ui/pages/modplatform/atlauncher/AtlListModel.h
    ui/pages/modplatform/atlauncher/AtlOptionalModDialog.cpp
    ui/pages/modplatform/atlauncher/AtlOptionalModDialog.h
    ui/pages/modplatform/atlauncher/AtlPage.cpp
    ui/pages/modplatform/atlauncher/AtlPage.h

    ui/pages/modplatform/ftb/FtbFilterModel.cpp
    ui/pages/modplatform/ftb/FtbFilterModel.h
    ui/pages/modplatform/ftb/FtbListModel.cpp
    ui/pages/modplatform/ftb/FtbListModel.h
    ui/pages/modplatform/ftb/FtbPage.cpp
    ui/pages/modplatform/ftb/FtbPage.h

    ui/pages/modplatform/legacy_ftb/Page.cpp
    ui/pages/modplatform/legacy_ftb/Page.h
    ui/pages/modplatform/legacy_ftb/ListModel.h
    ui/pages/modplatform/legacy_ftb/ListModel.cpp

    ui/pages/modplatform/flame/FlameModel.cpp
    ui/pages/modplatform/flame/FlameModel.h
    ui/pages/modplatform/flame/FlamePage.cpp
    ui/pages/modplatform/flame/FlamePage.h

    ui/pages/modplatform/modrinth/ModrinthModel.cpp
    ui/pages/modplatform/modrinth/ModrinthModel.h
    ui/pages/modplatform/modrinth/ModrinthPage.cpp
    ui/pages/modplatform/modrinth/ModrinthPage.h

    ui/pages/modplatform/technic/TechnicModel.cpp
    ui/pages/modplatform/technic/TechnicModel.h
    ui/pages/modplatform/technic/TechnicPage.cpp
    ui/pages/modplatform/technic/TechnicPage.h

    ui/pages/modplatform/ImportPage.cpp
    ui/pages/modplatform/ImportPage.h

    ui/dialogs/AboutDialog.cpp
    ui/dialogs/AboutDialog.h
    ui/dialogs/BlockedModsDialog.cpp
    ui/dialogs/BlockedModsDialog.h
    ui/dialogs/ProfileSelectDialog.cpp
    ui/dialogs/ProfileSelectDialog.h
    ui/dialogs/ProfileSetupDialog.cpp
    ui/dialogs/ProfileSetupDialog.h
    ui/dialogs/CopyInstanceDialog.cpp
    ui/dialogs/CopyInstanceDialog.h
    ui/dialogs/CreateShortcutDialog.cpp
    ui/dialogs/CreateShortcutDialog.h
    ui/dialogs/CustomMessageBox.cpp
    ui/dialogs/CustomMessageBox.h
    ui/dialogs/EditAccountDialog.cpp
    ui/dialogs/EditAccountDialog.h
    ui/dialogs/ExportInstanceDialog.cpp
    ui/dialogs/ExportInstanceDialog.h
    ui/dialogs/ExportPackDialog.cpp
    ui/dialogs/ExportPackDialog.h
    ui/dialogs/IconPickerDialog.cpp
    ui/dialogs/IconPickerDialog.h
    ui/dialogs/InstallLoaderDialog.cpp
    ui/dialogs/InstallLoaderDialog.h
    ui/dialogs/MSALoginDialog.cpp
    ui/dialogs/MSALoginDialog.h
    ui/dialogs/NewComponentDialog.cpp
    ui/dialogs/NewComponentDialog.h
    ui/dialogs/NewInstanceDialog.cpp
    ui/dialogs/NewInstanceDialog.h
    ui/dialogs/NewsViewerDialog.cpp
    ui/dialogs/NewsViewerDialog.h
    ui/dialogs/NotificationDialog.cpp
    ui/dialogs/NotificationDialog.h
    ui/pagedialog/PageDialog.cpp
    ui/pagedialog/PageDialog.h
    ui/dialogs/PluginsDialog.cpp
    ui/dialogs/PluginsDialog.h
    ui/dialogs/ProgressDialog.cpp
    ui/dialogs/ProgressDialog.h
    ui/dialogs/UpdateAvailableDialog.cpp
    ui/dialogs/UpdateAvailableDialog.h
    ui/dialogs/VersionSelectDialog.cpp
    ui/dialogs/VersionSelectDialog.h
    ui/dialogs/skins/SkinManageDialog.cpp
    ui/dialogs/skins/SkinManageDialog.h
    ui/dialogs/skins/render/CubeMesh.cpp
    ui/dialogs/skins/render/CubeMesh.h
    ui/dialogs/skins/render/PlayerScene.cpp
    ui/dialogs/skins/render/PlayerScene.h
    ui/dialogs/skins/render/SkinPreviewSurface.cpp
    ui/dialogs/skins/render/SkinPreviewSurface.h
    ui/dialogs/DownloadContentDialog.cpp
    ui/dialogs/DownloadContentDialog.h
    ui/dialogs/DownloadSummaryDialog.cpp
    ui/dialogs/DownloadSummaryDialog.h
    ui/dialogs/ModUpdateDialog.cpp
    ui/dialogs/ModUpdateDialog.h
    ui/dialogs/ExportListDialog.cpp
    ui/dialogs/ExportListDialog.h
    ui/dialogs/KiteLogsDialog.cpp
    ui/dialogs/KiteLogsDialog.h
    ui/dialogs/UntrustedModsDialog.cpp
    ui/dialogs/UntrustedModsDialog.h


    ui/widgets/Common.cpp
    ui/widgets/Common.h
    ui/widgets/CustomCommands.cpp
    ui/widgets/CustomCommands.h
    ui/widgets/DropLabel.cpp
    ui/widgets/DropLabel.h
    ui/widgets/FocusLineEdit.cpp
    ui/widgets/FocusLineEdit.h
    ui/widgets/IconLabel.cpp
    ui/widgets/IconLabel.h
    ui/widgets/JavaSettingsWidget.cpp
    ui/widgets/JavaSettingsWidget.h
    ui/widgets/LabeledToolButton.cpp
    ui/widgets/LabeledToolButton.h
    ui/widgets/LanguageSelectionWidget.cpp
    ui/widgets/LanguageSelectionWidget.h
    ui/widgets/LogView.cpp
    ui/widgets/LogView.h
    ui/widgets/MCModInfoFrame.cpp
    ui/widgets/MCModInfoFrame.h
    ui/widgets/ModListView.cpp
    ui/widgets/ModListView.h
    ui/widgets/PageContainer.cpp
    ui/widgets/PageContainer.h
    ui/widgets/PageContainer_p.h
    ui/widgets/VersionListView.cpp
    ui/widgets/VersionListView.h
    ui/widgets/VersionSelectWidget.cpp
    ui/widgets/VersionSelectWidget.h
    ui/widgets/ProgressWidget.h
    ui/widgets/ProgressWidget.cpp
    ui/widgets/ProjectItemDelegate.h
    ui/widgets/ProjectItemDelegate.cpp
    ui/widgets/ProjectDescriptionPage.h
    ui/widgets/ProjectDescriptionPage.cpp
    ui/widgets/ContentFilterWidget.h
    ui/widgets/ContentFilterWidget.cpp
    ui/widgets/CheckComboBox.h
    ui/widgets/CheckComboBox.cpp
    ui/widgets/TaskStepProgressBar.h
    ui/widgets/TaskStepProgressBar.cpp
    ui/widgets/WideBar.h
    ui/widgets/WideBar.cpp

    ui/instanceview/InstanceProxyModel.cpp
    ui/instanceview/InstanceProxyModel.h
    ui/instanceview/AccessibleInstanceView.cpp
    ui/instanceview/AccessibleInstanceView.h
    ui/instanceview/AccessibleInstanceView_p.h
    ui/instanceview/InstanceView.cpp
    ui/instanceview/InstanceView.h
    ui/instanceview/InstanceDelegate.cpp
    ui/instanceview/InstanceDelegate.h
    ui/instanceview/VisualGroup.cpp
    ui/instanceview/VisualGroup.h
)

set(KITE_UI_FILES
    ui/pages/global/AccountListPage.ui
    ui/pages/global/AppearancePage.ui
    ui/pages/global/JavaPage.ui
    ui/pages/global/KitePage.ui
    ui/pages/global/PasteEEPage.ui
    ui/pages/global/ProxyPage.ui
    ui/pages/global/MinecraftPage.ui
    ui/pages/global/ExternalToolsPage.ui
    ui/pages/instance/BackupPage.ui
    ui/pages/instance/ModFolderPage.ui
    ui/pages/instance/NotesPage.ui
    ui/pages/instance/LogPage.ui
    ui/pages/instance/ServersPage.ui
    ui/pages/instance/GameOptionsPage.ui
    ui/pages/instance/OtherLogsPage.ui
    ui/pages/instance/InstanceSettingsPage.ui
    ui/pages/instance/VersionPage.ui
    ui/pages/instance/WorldListPage.ui
    ui/pages/instance/LegacyUpgradePage.ui
    ui/pages/instance/ManagedPackPage.ui
    ui/pages/instance/ScreenshotsPage.ui
    ui/pages/modplatform/atlauncher/AtlOptionalModDialog.ui
    ui/pages/modplatform/atlauncher/AtlPage.ui
    ui/pages/modplatform/VanillaPage.ui
    ui/pages/modplatform/ContentProviderPage.ui
    ui/pages/modplatform/flame/FlamePage.ui
    ui/pages/modplatform/modrinth/ModrinthPage.ui
    ui/pages/modplatform/legacy_ftb/Page.ui
    ui/pages/modplatform/ImportPage.ui
    ui/pages/modplatform/ftb/FtbPage.ui
    ui/pages/modplatform/technic/TechnicPage.ui
    ui/widgets/CustomCommands.ui
    ui/widgets/MCModInfoFrame.ui
    ui/dialogs/CopyInstanceDialog.ui
    ui/dialogs/ProfileSetupDialog.ui
    ui/dialogs/NewInstanceDialog.ui
    ui/dialogs/NotificationDialog.ui
    ui/dialogs/UpdateAvailableDialog.ui
    ui/dialogs/KiteLogsDialog.ui
    ui/dialogs/PluginsDialog.ui
    ui/dialogs/NewComponentDialog.ui
    ui/dialogs/ProfileSelectDialog.ui
    ui/dialogs/skins/SkinManageDialog.ui
    ui/dialogs/CreateShortcutDialog.ui
    ui/dialogs/ExportInstanceDialog.ui
    ui/dialogs/ExportPackDialog.ui
    ui/dialogs/IconPickerDialog.ui
    ui/dialogs/MSALoginDialog.ui
    ui/dialogs/AboutDialog.ui
    ui/dialogs/EditAccountDialog.ui
    ui/dialogs/UntrustedModsDialog.ui
    ui/MainWindow.ui
)

# Wrapped once: two qt_wrap_ui calls for the same form would declare two rules for the same ui_*.h.
# Both targets list the generated sources; the rule stays single.
set(KITE_SHARED_UI_FILES
    ui/dialogs/ProgressDialog.ui
    ui/widgets/TaskStepProgressBar.ui
)

# Not the versionless qt_wrap_ui: it is a no-op unless QT_DEFAULT_MAJOR_VERSION is set.
qt6_wrap_ui(KITE_UI ${KITE_UI_FILES})
qt6_wrap_ui(KITE_SHARED_UI ${KITE_SHARED_UI_FILES})

if(NOT Kite_DISABLE_JAVA_DOWNLOADER)
    list(APPEND KITE_SOURCES
        ui/dialogs/JavaDownloadDialog.cpp
        ui/dialogs/JavaDownloadDialog.h
    )
endif()

if (APPLE)
    set(KITE_SOURCES
        ${KITE_SOURCES}
        ui/themes/ThemeManager.mm
    )
endif()

set(KITE_QRC_FILES
    resources/backgrounds/backgrounds.qrc
    resources/multimc/multimc.qrc
    resources/pe_dark/pe_dark.qrc
    resources/pe_light/pe_light.qrc
    resources/pe_colored/pe_colored.qrc
    resources/pe_blue/pe_blue.qrc
    resources/OSX/OSX.qrc
    resources/iOS/iOS.qrc
    resources/flat/flat.qrc
    resources/flat_white/flat_white.qrc
    resources/documents/documents.qrc
    resources/breeze_dark/breeze_dark.qrc
    resources/breeze_light/breeze_light.qrc
    resources/shaders/shaders.qrc
    ${CMAKE_BINARY_DIR}/${Kite_AppBinaryName}.qrc
)

qt6_add_resources(KITE_RESOURCES ${KITE_QRC_FILES})

if(WIN32)
    set(KITE_RCS ${CMAKE_BINARY_DIR}/${Kite_AppBinaryName}.rc)
endif()

# Kite_core works without a user interface: it must not link QtWidgets or know about the plugin host
# or ui/. Kite_logic is the plugin host and the widget UI on top of it.
add_library(Kite_core STATIC ${LOGIC_SOURCES})

# Embedded as :/kite/bundles.json; in the core so tests find it too.
qt6_add_resources(Kite_core kite_bundles
    PREFIX "/kite"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources/kite"
    FILES resources/kite/bundles.json)

# kite_<lang>.ts -> :/i18n/kite_<lang>.qm, plus :/i18n/stats.txt ("<lang> <translated> <total>",
# counted from the .ts: a .qm cannot tell). No LinguistTools or no .ts: English only.
if(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
    file(GLOB KITE_TS_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/translations/kite_*.ts")
    if(KITE_TS_FILES)
        # Content changes of a .ts re-run cmake so stats.txt follows them.
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${KITE_TS_FILES})
        set(_kite_i18n_stats "")
        foreach(_ts IN LISTS KITE_TS_FILES)
            get_filename_component(_lang "${_ts}" NAME_WE)
            string(REGEX REPLACE "^kite_" "" _lang "${_lang}")
            file(READ "${_ts}" _ts_text)
            string(REGEX MATCHALL "<message[ >]" _ts_messages "${_ts_text}")
            list(LENGTH _ts_messages _ts_total)
            string(REGEX MATCHALL "<translation type=\"(unfinished|vanished|obsolete)\"" _ts_open "${_ts_text}")
            list(LENGTH _ts_open _ts_unfinished)
            math(EXPR _ts_done "${_ts_total} - ${_ts_unfinished}")
            string(APPEND _kite_i18n_stats "${_lang} ${_ts_done} ${_ts_total}\n")
        endforeach()
        unset(_ts_text)
        file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/stats.txt"
            CONTENT "${_kite_i18n_stats}" @ONLY)
        qt6_add_lrelease(Kite_core
            TS_FILES ${KITE_TS_FILES}
            QM_FILES_OUTPUT_VARIABLE KITE_QM_FILES)
        qt6_add_resources(Kite_core kite_i18n
            PREFIX "/i18n"
            BASE "${CMAKE_CURRENT_BINARY_DIR}"
            FILES ${KITE_QM_FILES} "${CMAKE_CURRENT_BINARY_DIR}/stats.txt")
        message(STATUS "Bundled translations: ${KITE_TS_FILES}")
    endif()
endif()
add_library(Kite_logic STATIC ${PLUGIN_SOURCES} ${KITE_SOURCES} ${KITE_UI} ${KITE_SHARED_UI} ${KITE_RESOURCES})
target_link_libraries(Kite_logic Kite_core)

add_unit_test(CustomTheme
    SOURCES ui/themes/CustomTheme_test.cpp
    LIBS Kite_logic
    )

add_unit_test(PluginLoader
    SOURCES plugin/PluginLoader_test.cpp
    LIBS Kite_logic
    )

add_unit_test(PluginUiRenderer
    SOURCES plugin/PluginUiRenderer_test.cpp
    LIBS Kite_logic
    )

add_unit_test(PluginSurfaceModel
    SOURCES plugin/PluginSurfaceModel_test.cpp
    LIBS Kite_logic
    )

add_unit_test(PluginListModel
    SOURCES plugin/PluginListModel_test.cpp
    LIBS Kite_logic
    )

add_unit_test(Contrast
    SOURCES theme/Contrast_test.cpp
    LIBS Kite_logic
    )

# Links the core alone, to prove the palette is core.
add_unit_test(ThemePalette
    SOURCES theme/ThemePalette_test.cpp
    LIBS Kite_core
    )

add_unit_test(InstanceFilterModel
    SOURCES models/InstanceFilterModel_test.cpp
    LIBS Kite_core
    )

add_unit_test(InstanceList
    SOURCES InstanceList_test.cpp
    LIBS Kite_core
    )

add_unit_test(LaunchProgressTracker
    SOURCES launch/LaunchProgressTracker_test.cpp
    LIBS Kite_core
    )

add_unit_test(LaunchTask
    SOURCES launch/LaunchTask_test.cpp
    LIBS Kite_core
    )

add_unit_test(IdSelectionModel
    SOURCES models/IdSelectionModel_test.cpp
    LIBS Kite_core
    )

add_unit_test(SettingsAdapter
    SOURCES models/SettingsAdapter_test.cpp
    LIBS Kite_core
    )

add_unit_test(InstanceLogBridge
    SOURCES models/InstanceDetails_test.cpp
    LIBS Kite_core
    )

add_unit_test(AccountsController
    SOURCES models/AccountsController_test.cpp
    LIBS Kite_core
    )

add_unit_test(NewInstanceController
    SOURCES models/NewInstanceController_test.cpp
    LIBS Kite_core
    )

add_unit_test(ServersListModel
    SOURCES models/ServersListModel_test.cpp
    LIBS Kite_core
    )

add_unit_test(JavaController
    SOURCES models/JavaController_test.cpp
    LIBS Kite_core
    )
add_unit_test(JavaDownloadController
    SOURCES models/JavaDownloadController_test.cpp
    LIBS Kite_core
    )

add_unit_test(FolderSettingsController
    SOURCES models/FolderSettingsController_test.cpp
    LIBS Kite_core
    )
add_unit_test(UpdateSettingsController
    SOURCES models/UpdateSettingsController_test.cpp
    LIBS Kite_core
    )
add_unit_test(PlayTimeSummary
    SOURCES models/PlayTimeSummary_test.cpp
    LIBS Kite_core
    )
add_unit_test(ComponentTools
    SOURCES models/ComponentTools_test.cpp
    LIBS Kite_core
    )
add_unit_test(CatPackList
    SOURCES models/CatPackList_test.cpp
    LIBS Kite_core
    )

add_unit_test(ManagedPackController
    SOURCES models/ManagedPackController_test.cpp
    LIBS Kite_core
    )

add_unit_test(RecentWorldsModel
    SOURCES models/RecentWorldsModel_test.cpp
    LIBS Kite_core
    )

add_unit_test(AboutInfo
    SOURCES models/AboutInfo_test.cpp
    LIBS Kite_core
    )

add_unit_test(NewsModel
    SOURCES models/NewsModel_test.cpp
    LIBS Kite_core
    )

add_unit_test(NewsEntry
    SOURCES news/NewsEntry_test.cpp
    LIBS Kite_core
    )

add_unit_test(SkinsController
    SOURCES models/SkinsController_test.cpp
    LIBS Kite_core
    )

add_unit_test(AuthParsers
    SOURCES minecraft/auth/Parsers_test.cpp
    LIBS Kite_core
    )

add_unit_test(InstancePathProblems
    SOURCES core/InstancePathProblems_test.cpp
    LIBS Kite_core
    )

add_unit_test(TaskWatcher
    SOURCES tasks/TaskWatcher_test.cpp
    LIBS Kite_core
    )

add_unit_test(ModrinthModpackModel
    SOURCES modplatform/modrinth/ModrinthModpackModel_test.cpp
    LIBS Kite_core
    )

add_unit_test(RichBody
    SOURCES models/RichBody_test.cpp
    LIBS Kite_core
    )

add_unit_test(FlameModpackModel
    SOURCES modplatform/flame/FlameModpackModel_test.cpp
    LIBS Kite_core
    )
add_unit_test(FtbModpackModel
    SOURCES modplatform/modpacksch/FtbModpackModel_test.cpp
    LIBS Kite_core
    )
add_unit_test(AtlModpackModel
    SOURCES modplatform/atlauncher/AtlModpackModel_test.cpp
    LIBS Kite_core
    )
add_unit_test(AtlOptionalModsModel
    SOURCES models/AtlOptionalModsModel_test.cpp
    LIBS Kite_core
    )
add_unit_test(TechnicModpackModel
    SOURCES modplatform/technic/TechnicModpackModel_test.cpp
    LIBS Kite_core
    )

add_unit_test(ContentBrowser
    SOURCES models/ContentBrowser_test.cpp
    LIBS Kite_core
    )

add_unit_test(LoaderInstaller
    SOURCES models/LoaderInstaller_test.cpp
    LIBS Kite_core
    )

add_unit_test(OtherLogsModel
    SOURCES models/OtherLogsModel_test.cpp
    LIBS Kite_core
    )

add_unit_test(LogUploader
    SOURCES models/LogUploader_test.cpp
    LIBS Kite_core
    )

add_unit_test(LauncherLogs
    SOURCES models/LauncherLogs_test.cpp
    LIBS Kite_core
    )

add_unit_test(ScreenshotUploader
    SOURCES models/ScreenshotUploader_test.cpp
    LIBS Kite_core
    )

add_unit_test(WorldTools
    SOURCES models/WorldTools_test.cpp
    LIBS Kite_core
    )

add_unit_test(LegacyUpgrade
    SOURCES models/LegacyUpgrade_test.cpp
    LIBS Kite_core
    )

add_unit_test(LogFind
    SOURCES models/LogFind_test.cpp
    LIBS Kite_core
    )

add_unit_test(KeyValueFilterModel
    SOURCES models/KeyValueFilterModel_test.cpp
    LIBS Kite_core
    )

add_unit_test(ContentFilterModel
    SOURCES models/ContentFilterModel_test.cpp
    LIBS Kite_core
    )
add_unit_test(ContentInstallReview
    SOURCES models/ContentInstallReview_test.cpp
    LIBS Kite_core
    )
add_unit_test(BundleCatalog
    SOURCES modplatform/BundleCatalog_test.cpp
    LIBS Kite_core
    )
add_unit_test(PresetDefaults
    SOURCES minecraft/PresetDefaults_test.cpp
    LIBS Kite_core
    )
add_unit_test(GraphicsBackend
    SOURCES minecraft/GraphicsBackend_test.cpp
    LIBS Kite_core
    )
add_unit_test(PerformanceSetupTask
    SOURCES minecraft/PerformanceSetupTask_test.cpp
    LIBS Kite_core
    )
add_unit_test(KiteModRules
    SOURCES minecraft/KiteModRules_test.cpp
    LIBS Kite_core
    )
add_unit_test(ModHealthModel
    SOURCES models/ModHealthModel_test.cpp
    LIBS Kite_core
    )
add_unit_test(ContentUpdateController
    SOURCES models/ContentUpdateController_test.cpp
    LIBS Kite_core
    )
add_unit_test(ContentListExport
    SOURCES modplatform/ContentListExport_test.cpp
    LIBS Kite_core
    )
add_unit_test(InstanceExportController
    SOURCES models/InstanceExportController_test.cpp
    LIBS Kite_core Qt${QT_VERSION_MAJOR}::Gui
    )
add_unit_test(InstanceShortcutController
    SOURCES models/InstanceShortcutController_test.cpp
    LIBS Kite_core Qt${QT_VERSION_MAJOR}::Gui
    )
add_unit_test(LibraryController
    SOURCES models/LibraryController_test.cpp
    LIBS Kite_core
    )

add_unit_test(IconTheme
    SOURCES ui/themes/IconTheme_test.cpp
    LIBS Kite_logic
    )

add_unit_test(IconList
    SOURCES icons/IconList_test.cpp
    LIBS Kite_core Qt${QT_VERSION_MAJOR}::Gui
    )

add_unit_test(AutoIcons
    SOURCES icons/AutoIcons_test.cpp icons/AutoIcons.cpp
    LIBS Qt${QT_VERSION_MAJOR}::Core
    )
target_include_directories(AutoIcons_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_unit_test(GameTextureIcons
    SOURCES icons/GameTextureIcons_test.cpp
    LIBS Kite_core Qt${QT_VERSION_MAJOR}::Gui
    )

target_compile_definitions(IconTheme_test PRIVATE
    KITE_MAINWINDOW_UI_PATH="${CMAKE_CURRENT_SOURCE_DIR}/ui/MainWindow.ui")

set_tests_properties(IconTheme PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORMTHEME="
    )

# Core headers are included by their path under launcher/ ("core/UiHost.h"), so that directory is
# part of the core's interface; CMAKE_INCLUDE_CURRENT_DIR does not reach the QML module declared in
# launcher/qml/.
target_include_directories(Kite_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Kite_core PUBLIC
    classparser
    nbt++
    ZLIB::ZLIB
    tomlplusplus::tomlplusplus
    BuildConfig
    Katabasis
)

if(Kite_DISABLE_JAVA_DOWNLOADER)
    target_compile_definitions(Kite_core PUBLIC Kite_DISABLE_JAVA_DOWNLOADER)
endif()

# Keep the function/line of every log message in all configurations: Qt fills QMessageLogContext
# only while QT_NO_DEBUG is absent. PUBLIC because the message logger macros expand at every call
# site.
target_compile_definitions(Kite_core PUBLIC QT_MESSAGELOGCONTEXT)
target_link_libraries(Kite_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Xml
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::NetworkAuth
    Qt${QT_VERSION_MAJOR}::Concurrent
    Qt${QT_VERSION_MAJOR}::Gui
    LibArchive::LibArchive
    cmark::cmark
    rainbow
    LocalPeer::LocalPeer
)

# QtWidgets is linked here and only here (enforced by Kite_core_link_check).
target_link_libraries(Kite_logic Qt${QT_VERSION_MAJOR}::Widgets)

# Application picks MainWindow or the QML shell at startup, so the shell is compiled in either way.
target_link_libraries(Kite_logic Kite_qml)
if(Kite_QML_UI)
    target_compile_definitions(Kite_logic PRIVATE KITE_QML_UI_DEFAULT)
endif()

target_link_libraries(Kite_logic Qt6::OpenGL)

# Plugin system needs dlopen/dlsym on Unix
if(UNIX AND NOT APPLE)
    target_link_libraries(Kite_logic ${CMAKE_DL_LIBS})
endif()

# GpgME (C++ bindings): MMCO plugin signature verification. Off by default on MSVC, which gpgme does
# not support; when off, PluginSignature.cpp is a stub that reports every module as unsigned.
if(Kite_PLUGIN_SIGNATURES)
    target_link_libraries(Kite_logic ${Kite_GpgmeppTarget})
    target_compile_definitions(Kite_logic PRIVATE KITE_PLUGIN_SIGNATURES=1)
else()
    target_compile_definitions(Kite_logic PRIVATE KITE_PLUGIN_SIGNATURES=0)
endif()

if(APPLE)
    # PUBLIC: ThemeManager.mm in Kite_logic also needs Cocoa.
    target_link_libraries(Kite_core PUBLIC
        "-framework AppKit"
        "-framework Foundation"
        objc
    )
    set(CMAKE_MACOSX_RPATH 1)
    set(CMAKE_INSTALL_RPATH "@loader_path/../Frameworks/")
endif()

if(Kite_SPARKLE_ENABLED)
    # PUBLIC: Application.cpp, in the application target, tests this define too.
    target_compile_definitions(Kite_core PUBLIC KITE_SPARKLE_ENABLED)
    target_include_directories(Kite_core PRIVATE "${MACOSX_SPARKLE_DIR}")
    target_link_libraries(Kite_core PUBLIC ${SPARKLE_FRAMEWORK})

    # Sparkle ships inside the bundle: it is loaded at runtime.
    install(DIRECTORY "${MACOSX_SPARKLE_DIR}/Sparkle.framework"
        DESTINATION "${FRAMEWORK_DEST_DIR}"
        USE_SOURCE_PERMISSIONS)
endif()

# Not linking Qt::Widgets into Kite_core is not enough: a static library resolves nothing by itself,
# and on macOS the framework path finds <QtWidgets/...> anyway. So link the whole core into a
# program with nothing else: a QtWidgets symbol in any core object fails the link on every platform.
# Part of `all` on purpose.
add_executable(Kite_core_link_check core/CoreLinkCheck.cpp)
target_link_libraries(Kite_core_link_check PRIVATE
    "$<LINK_LIBRARY:WHOLE_ARCHIVE,Kite_core>")

add_test(NAME CoreLinksWithoutQtWidgets COMMAND Kite_core_link_check)

# A separate target, not part of the application, so QmlModule_test can link and instantiate the
# same module headlessly.
add_subdirectory(qml)

# Kite_qml is linked explicitly: it carries the compiled .qml and puts the module plugins in the
# link closure that qt_import_qml_plugins scans (scanning the test's sources alone left Main.qml out
# of the resources on Linux).
add_unit_test(QmlModule
    SOURCES qml/QmlModule_test.cpp
    LIBS Kite_qml Kite_qmlplugin Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Gui
    )

# add_unit_test uses a plain add_executable, so the QML plugin import must be asked for by hand, or
# the test fails at runtime with 'plugin "Kite_qmlplugin" not found'. Kite_qmlplugin is linked above
# because nothing imports "Kite".
qt_import_qml_plugins(QmlModule_test)

# Kite_logic, not the core: the built-in icons are compiled into Kite_logic.
add_unit_test(InstanceIconProvider
    SOURCES qml/InstanceIconProvider_test.cpp
    LIBS Kite_logic Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Gui
    )

add_unit_test(AccountFaceProvider
    SOURCES qml/AccountFaceProvider_test.cpp
    LIBS Kite_logic Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Gui
    )

add_unit_test(ScreenshotThumbnailProvider
    SOURCES qml/ScreenshotThumbnailProvider_test.cpp
    LIBS Kite_logic Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Gui
    )

add_unit_test(ModIconProvider
    SOURCES qml/ModIconProvider_test.cpp
    LIBS Kite_logic Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Gui
    )

add_unit_test(QmlShell
    SOURCES qml/QmlShell_test.cpp
    LIBS Kite_logic Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Gui
    )

add_unit_test(QmlUiHost
    SOURCES qml/QmlUiHost_test.cpp
    LIBS Kite_logic Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Gui
    )

add_executable(${Kite_Name} MACOSX_BUNDLE WIN32 main.cpp ${KITE_RCS})
# Keyword signature: qt_import_qml_plugins links with PRIVATE and CMake refuses to mix the two
# forms. Qt::Svg is linked only so deployment packs the SVG image plugin; without it QML icons fail
# to load in the macOS bundle.
target_link_libraries(${Kite_Name} PRIVATE Kite_logic Kite_qmlplugin
    Qt${QT_VERSION_MAJOR}::Svg)

# Imports the static QML plugins; without it the launcher fails at runtime with 'plugin
# "Kite_qmlplugin" not found'.
qt_import_qml_plugins(${Kite_Name})

# Export all symbols so dlopen()'ed .mmco plugins can resolve launcher symbols at runtime
set_target_properties(${Kite_Name} PROPERTIES ENABLE_EXPORTS ON)

if(MSVC)
    target_link_options(${Kite_Name} PRIVATE "/MANIFEST:NO")
endif()

if(DEFINED Kite_AppBinaryName)
    set_target_properties(${Kite_Name} PROPERTIES OUTPUT_NAME "${Kite_AppBinaryName}")
endif()
if(APPLE)
    set_target_properties(${Kite_Name} PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/MacOSXBundleInfo.plist.in")
    # The top-level install() only covers the installed Kite.app; the build-tree bundle needs its
    # icon too.
    set(KITE_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/../${Kite_Branding_ICNS}")
    set_source_files_properties(${KITE_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(${Kite_Name} PRIVATE ${KITE_ICNS})
    # Application::getJarsPath() looks in Contents/Resources/jars; without the Java helpers the
    # build-tree bundle cannot start a game.
    get_target_property(KITE_NEWLAUNCH_JAR NewLaunch JAR_FILE)
    get_target_property(KITE_JAVACHECK_JAR JavaCheck JAR_FILE)
    set_source_files_properties(${KITE_NEWLAUNCH_JAR} ${KITE_JAVACHECK_JAR}
        PROPERTIES MACOSX_PACKAGE_LOCATION Resources/jars GENERATED TRUE)
    target_sources(${Kite_Name} PRIVATE ${KITE_NEWLAUNCH_JAR} ${KITE_JAVACHECK_JAR})
    add_dependencies(${Kite_Name} NewLaunch JavaCheck)
endif()
if(DEFINED Kite_BINARY_RPATH)
    SET_TARGET_PROPERTIES(${Kite_Name} PROPERTIES INSTALL_RPATH "${Kite_BINARY_RPATH}")
endif()

if(DEFINED Kite_APP_BINARY_DEFS)
    target_compile_definitions(${Kite_Name} PRIVATE ${Kite_APP_BINARY_DEFS})
    target_compile_definitions(Kite_logic PRIVATE ${Kite_APP_BINARY_DEFS})
endif()

if(APPLE)
    # CMake names the bundle directory after OUTPUT_NAME ("kite.app"), but everything else installs
    # into "Kite.app"; on a case-sensitive volume the install would split in half. So install the
    # bundle into a staging directory and merge it in. The staging hop is needed: only
    # install(TARGETS) rewrites the build RPATH into INSTALL_RPATH, and only for its own copy.
    set(Kite_BundleStagingDir "_kite_bundle_staging")

    install(TARGETS ${Kite_Name}
        RUNTIME_DEPENDENCY_SET LAUNCHER_DEPENDENCY_SET
        BUNDLE DESTINATION "${Kite_BundleStagingDir}"
        LIBRARY DESTINATION ${LIBRARY_DEST_DIR}
    )

    # Runs before the qt_deploy_runtime_dependencies() script, which expects the binary at
    # ${BINARY_DEST_DIR}/$<TARGET_FILE_NAME>.
    install(CODE "
        set(_kite_prefix \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\")
        set(_kite_staging \"\${_kite_prefix}/${Kite_BundleStagingDir}\")
        set(_kite_staged_bundle \"\${_kite_staging}/$<TARGET_BUNDLE_DIR_NAME:${Kite_Name}>\")
        set(_kite_bundle \"\${_kite_prefix}/${Kite_Name}.app\")
        if(NOT IS_DIRECTORY \"\${_kite_staged_bundle}\")
            message(FATAL_ERROR
                \"Staged app bundle '\${_kite_staged_bundle}' is missing; \"
                \"cannot assemble ${Kite_Name}.app\")
        endif()
        message(STATUS \"Merging \${_kite_staged_bundle} into \${_kite_bundle}\")
        file(COPY \"\${_kite_staged_bundle}/\" DESTINATION \"\${_kite_bundle}\")
        file(REMOVE_RECURSE \"\${_kite_staging}\")
    ")
else()
    install(TARGETS ${Kite_Name}
        RUNTIME_DEPENDENCY_SET LAUNCHER_DEPENDENCY_SET
        BUNDLE DESTINATION ${BUNDLE_DEST_DIR}
        LIBRARY DESTINATION ${LIBRARY_DEST_DIR}
        RUNTIME DESTINATION ${BINARY_DEST_DIR}
    )
endif()


# A separate executable: it replaces the libraries the launcher runs from, and a process cannot
# delete an image it has mapped. macOS is excluded: Sparkle does the job there.
if(NOT APPLE)
    set(MESHUPDATER_UI_FILES
        updater/meshupdater/SelectReleaseDialog.ui
    )

    qt6_wrap_ui(MESHUPDATER_UI ${MESHUPDATER_UI_FILES})

    set(MESHUPDATER_SOURCES
        updater/meshupdater/MeshUpdaterApp.h
        updater/meshupdater/MeshUpdaterApp.cpp
        updater/meshupdater/AssetMatcher.h
        updater/meshupdater/AssetMatcher.cpp
        updater/meshupdater/GitHubRelease.h
        updater/meshupdater/GitHubRelease.cpp
        updater/meshupdater/ReleaseArchive.h
        updater/meshupdater/ReleaseArchive.cpp
        updater/meshupdater/ReleaseDownload.h
        updater/meshupdater/ReleaseDownload.cpp
        updater/meshupdater/UpdaterDialogs.h
        updater/meshupdater/UpdaterDialogs.cpp

        updater/UpdateLockFile.h
        updater/UpdateLockFile.cpp

        # From the launcher; none depend on Application.
        ArchiveOpen.h
        Version.h
        Version.cpp
        MMCStrings.h
        MMCStrings.cpp
        HoeDown.h
        DesktopServices.h
        DesktopServices.cpp
        QObjectPtr.h

        tasks/Task.h
        tasks/Task.cpp

        ui/dialogs/ProgressDialog.h
        ui/dialogs/ProgressDialog.cpp
        ui/widgets/TaskStepProgressBar.h
        ui/widgets/TaskStepProgressBar.cpp
    )

    add_library(kite_updater_logic STATIC ${MESHUPDATER_SOURCES} ${MESHUPDATER_UI} ${KITE_SHARED_UI})
    target_include_directories(kite_updater_logic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(kite_updater_logic PUBLIC QT_MESSAGELOGCONTEXT)
    target_link_libraries(kite_updater_logic
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        LibArchive::LibArchive
        cmark::cmark
        BuildConfig
    )

    # WIN32: no console window; diagnostics go to the log file, --debug borrows the parent console.
    add_executable(kite-updater WIN32 updater/meshupdater/updater_main.cpp)
    if(WIN32)
        target_sources(kite-updater PRIVATE updater/meshupdater/updater.exe.manifest)
    endif()
    target_link_libraries(kite-updater kite_updater_logic)

    if(MSVC)
        # The manifest is embedded explicitly; a linker-generated one as well is an error.
        target_link_options(kite-updater PRIVATE "/MANIFEST:NO")
    endif()

    if(DEFINED Kite_BINARY_RPATH)
        set_target_properties(kite-updater PROPERTIES INSTALL_RPATH "${Kite_BINARY_RPATH}")
    endif()

    # Next to the launcher: KiteExternalUpdater and Application::updaterEnabled() look for it there.
    install(TARGETS kite-updater
        RUNTIME DESTINATION "${BINARY_DEST_DIR}"
    )

    add_unit_test(AssetMatcher
        SOURCES updater/meshupdater/AssetMatcher_test.cpp
        LIBS kite_updater_logic
        )

    add_unit_test(ReleaseArchive
        SOURCES updater/meshupdater/ReleaseArchive_test.cpp
        LIBS kite_updater_logic LibArchive::LibArchive
        )

    add_unit_test(UpdateLockFile
        SOURCES updater/UpdateLockFile_test.cpp
        LIBS kite_updater_logic
        )
endif()

# Bundle utilities complete the portable packages with the libraries the target system lacks. This
# has to stay here.
if(WIN32 OR (UNIX AND APPLE))
    if(WIN32)
        # Find windeployqt directly: qt_generate_deploy_script looks for windeployqt.debug.bat on
        # Debug builds, which may not exist. The host tools may not sit next to qmake, so qmake is
        # asked as well; side-by-side Qt packagings suffix the tool name.
        get_target_property(_qt_qmake_loc Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
        get_filename_component(_qt_bin_dir "${_qt_qmake_loc}" DIRECTORY)

        QUERY_QMAKE(QT_INSTALL_BINS _qt_install_bins)
        QUERY_QMAKE(QT_HOST_BINS _qt_host_bins)

        find_program(WINDEPLOYQT_EXECUTABLE
            NAMES
                windeployqt
                windeployqt-qt${QT_VERSION_MAJOR}
                windeployqt${QT_VERSION_MAJOR}
            HINTS
                "${_qt_bin_dir}"
                "${_qt_install_bins}"
                "${_qt_host_bins}"
                "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin"
        )

        if(NOT WINDEPLOYQT_EXECUTABLE)
            message(FATAL_ERROR
                "windeployqt not found.\n"
                "  Names searched      : windeployqt, "
                "windeployqt-qt${QT_VERSION_MAJOR}, windeployqt${QT_VERSION_MAJOR}\n"
                "  qmake executable    : ${_qt_qmake_loc}\n"
                "  qmake directory     : ${_qt_bin_dir}\n"
                "  QT_INSTALL_BINS     : ${_qt_install_bins}\n"
                "  QT_HOST_BINS        : ${_qt_host_bins}\n"
                "  Qt CMake dir + /bin : ${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin\n"
                "windeployqt ships with Qt's qtbase tools; make sure they are "
                "installed for this Qt and that the directories above look right.")
        endif()

        message(STATUS "windeployqt: ${WINDEPLOYQT_EXECUTABLE}")

        # Not every windeployqt knows --skip-plugin-types and --no-system-dxc-compiler (the latter
        # came partway through Qt 6), and unknown options fail install, so ask the tool which
        # switches its help output advertises.
        execute_process(
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --help
            OUTPUT_VARIABLE _windeployqt_help_out
            ERROR_VARIABLE _windeployqt_help_err
            RESULT_VARIABLE _windeployqt_help_result
        )
        # Some builds print the usage text on stderr, so consider both streams.
        set(_windeployqt_help "${_windeployqt_help_out}${_windeployqt_help_err}")
        if(NOT _windeployqt_help_result EQUAL 0)
            # Not fatal: dropping the optional switches only makes the deployment larger.
            message(WARNING
                "Could not query '${WINDEPLOYQT_EXECUTABLE} --help' "
                "(exit code: ${_windeployqt_help_result}). Deploying with the "
                "baseline option set; the package may contain plugins that "
                "would otherwise have been skipped.")
            set(_windeployqt_help "")
        endif()

        # install-qt-action and MSYS2 only provide release Qt, so always pass --release. --qmldir
        # lets qmlimportscanner see what our QML modules import; those Qt modules (QtQuick, ...) are
        # not compiled in and must be deployed.
        set(WINDEPLOYQT_OPTIONS
            --release
            --no-opengl-sw
            --qmldir "${CMAKE_CURRENT_SOURCE_DIR}/qml"
            --no-system-d3d-compiler
            --no-translations
            --no-compiler-runtime
        )
        if(_windeployqt_help MATCHES "no-system-dxc-compiler")
            list(APPEND WINDEPLOYQT_OPTIONS --no-system-dxc-compiler)
        else()
            message(STATUS "windeployqt: --no-system-dxc-compiler unsupported, skipping it")
        endif()
        if(_windeployqt_help MATCHES "skip-plugin-types")
            list(APPEND WINDEPLOYQT_OPTIONS --skip-plugin-types generic,networkinformation)
        else()
            message(STATUS "windeployqt: --skip-plugin-types unsupported, skipping it")
        endif()

        if(_windeployqt_help MATCHES "no-angle")
            set(_qt_angle_dll "")
            foreach(_qt_angle_dir IN ITEMS
                "${_qt_bin_dir}" "${_qt_install_bins}" "${_qt_host_bins}")
                if(_qt_angle_dir AND EXISTS "${_qt_angle_dir}/libGLESv2.dll")
                    set(_qt_angle_dll "${_qt_angle_dir}/libGLESv2.dll")
                    break()
                endif()
            endforeach()
            if(_qt_angle_dll)
                message(STATUS "windeployqt: ANGLE present (${_qt_angle_dll}), deploying it")
            else()
                message(STATUS
                    "windeployqt: no libGLESv2.dll alongside Qt, passing --no-angle")
                list(APPEND WINDEPLOYQT_OPTIONS --no-angle)
            endif()
        endif()

        message(STATUS "windeployqt options: ${WINDEPLOYQT_OPTIONS}")

        # One quoted argument per line: a list expanded into install(CODE) would become one
        # semicolon-joined argument.
        set(WINDEPLOYQT_OPTIONS_CODE "")
        foreach(_windeployqt_opt IN LISTS WINDEPLOYQT_OPTIONS)
            string(APPEND WINDEPLOYQT_OPTIONS_CODE "\n                    \"${_windeployqt_opt}\"")
        endforeach()
    endif()

    if(UNIX AND APPLE)
        # Qt QML modules are loaded by URI at run time, which qt_deploy_runtime_dependencies() does
        # not follow. Deploy exactly the ones launcher/qml imports (via qmlimportscanner), before
        # that call so its ADDITIONAL_LIBRARIES resolves each plugin's own dependencies. The Kite.*
        # modules are compiled in.
        find_program(QMLIMPORTSCANNER_EXECUTABLE
            NAMES qmlimportscanner
            HINTS
                "${QT_LIBEXECS_DIR}"
                "${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin"
                "${Qt${QT_VERSION_MAJOR}_DIR}/../../../libexec"
        )
        if(NOT QMLIMPORTSCANNER_EXECUTABLE)
            message(FATAL_ERROR
                "qmlimportscanner not found (searched QT_LIBEXECS_DIR="
                "${QT_LIBEXECS_DIR} and Qt${QT_VERSION_MAJOR}_DIR="
                "${Qt${QT_VERSION_MAJOR}_DIR}/../..). It ships with Qt's "
                "qtdeclarative tools and is required to know which Qt QML "
                "modules to bundle into ${Kite_Name}.app.")
        endif()

        execute_process(
            COMMAND "${QMLIMPORTSCANNER_EXECUTABLE}"
                -rootPath "${CMAKE_CURRENT_SOURCE_DIR}/qml"
                -importPath "${QT_QML_DIR}"
            OUTPUT_VARIABLE _qml_scan_json
            RESULT_VARIABLE _qml_scan_result
        )
        if(NOT _qml_scan_result EQUAL 0)
            message(FATAL_ERROR
                "qmlimportscanner failed while scanning launcher/qml "
                "(exit code: ${_qml_scan_result})")
        endif()

        # Reads "relativePath" by regex. Modules without one (our own Kite.*) are compiled in and
        # left out.
        string(REGEX MATCHALL "\"relativePath\": *\"[^\"]*\""
            _qml_relpath_matches "${_qml_scan_json}")
        set(KITE_QT_QML_MODULES "")
        foreach(_qml_match IN LISTS _qml_relpath_matches)
            string(REGEX REPLACE "\"relativePath\": *\"([^\"]*)\"" "\\1"
                _qml_relpath "${_qml_match}")
            list(APPEND KITE_QT_QML_MODULES "${_qml_relpath}")
        endforeach()
        list(REMOVE_DUPLICATES KITE_QT_QML_MODULES)

        if(NOT KITE_QT_QML_MODULES)
            message(FATAL_ERROR
                "qmlimportscanner reported no deployable Qt QML modules for "
                "launcher/qml. qt.conf's QmlImports would point at an empty "
                "directory and the QML shell would fail to load QtQuick at "
                "startup, so this is a hard error rather than a warning.")
        endif()
        message(STATUS "Qt QML modules to bundle into ${Kite_Name}.app: ${KITE_QT_QML_MODULES}")

        # Paths of the plugin binaries as installed, not under $QT_QML_DIR: ADDITIONAL_LIBRARIES
        # would otherwise run install_name_tool/codesign against the Qt installation itself.
        set(KITE_QT_QML_PLUGIN_FILES "")
        foreach(_qml_module IN LISTS KITE_QT_QML_MODULES)
            file(GLOB _qml_module_plugin_libs "${QT_QML_DIR}/${_qml_module}/*.dylib")
            foreach(_qml_module_plugin_lib IN LISTS _qml_module_plugin_libs)
                get_filename_component(_qml_plugin_name "${_qml_module_plugin_lib}" NAME)
                list(APPEND KITE_QT_QML_PLUGIN_FILES "${QML_DEST_DIR}/${_qml_module}/${_qml_plugin_name}")
            endforeach()
        endforeach()

        set(KITE_QT_QML_PLUGIN_FILES_CODE "")
        foreach(_qml_plugin_file IN LISTS KITE_QT_QML_PLUGIN_FILES)
            string(APPEND KITE_QT_QML_PLUGIN_FILES_CODE "\n                    \"${_qml_plugin_file}\"")
        endforeach()

        foreach(_qml_module IN LISTS KITE_QT_QML_MODULES)
            # Copy only the files each module owns directly, not recursively: $QT_QML_DIR nests
            # unrelated modules under shared parents (QtQuick/ also holds WebEngine's Pdf/, Qt3D,
            # ...), and qmlimportscanner lists wanted subdirectories as their own entries.
            # FOLLOW_SYMLINK_CHAIN matters on Homebrew, where every file is a symlink into another
            # keg. Install rules run in registration order, so this precedes the
            # qt_deploy_runtime_dependencies() script below.
            install(CODE "
                file(GLOB _qml_module_files LIST_DIRECTORIES false
                    \"${QT_QML_DIR}/${_qml_module}/*\")
                if(_qml_module_files)
                    file(INSTALL \${_qml_module_files}
                        DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${QML_DEST_DIR}/${_qml_module}\"
                        FOLLOW_SYMLINK_CHAIN
                    )
                endif()
            ")
        endforeach()

        # ADDITIONAL_LIBRARIES makes qt_deploy_runtime_dependencies() also resolve and copy the QML
        # plugins' own dependencies (e.g. QtQuickControls2Impl.framework), as whole frameworks.
        qt_generate_deploy_script(
            TARGET ${Kite_Name}
            OUTPUT_SCRIPT QT_DEPLOY_SCRIPT
            CONTENT "
            qt_deploy_runtime_dependencies(
                EXECUTABLE ${BINARY_DEST_DIR}/$<TARGET_FILE_NAME:${Kite_Name}>
                BIN_DIR ${BINARY_DEST_DIR}
                LIBEXEC_DIR ${BINARY_DEST_DIR}
                LIB_DIR ${LIBRARY_DEST_DIR}
                PLUGINS_DIR ${PLUGIN_DEST_DIR}
                ADDITIONAL_LIBRARIES${KITE_QT_QML_PLUGIN_FILES_CODE}
                NO_OVERWRITE
                NO_TRANSLATIONS
                NO_COMPILER_RUNTIME
            )"
        )
        install(
            SCRIPT ${QT_DEPLOY_SCRIPT}
        )

        foreach(_qml_module IN LISTS KITE_QT_QML_MODULES)
            # A copied plugin's @loader_path/@rpath references to sibling Qt frameworks only
            # resolved from its original place in the Qt install. Add an rpath to the real
            # Frameworks directory; a duplicate -add_rpath just fails and is ignored.
            file(RELATIVE_PATH _qml_module_to_frameworks
                "${CMAKE_INSTALL_PREFIX}/${QML_DEST_DIR}/${_qml_module}"
                "${CMAKE_INSTALL_PREFIX}/${FRAMEWORK_DEST_DIR}")
            install(CODE "
                file(GLOB _qml_plugin_libs
                    \"\${CMAKE_INSTALL_PREFIX}/${QML_DEST_DIR}/${_qml_module}/*.dylib\")
                foreach(_qml_plugin_lib IN LISTS _qml_plugin_libs)
                    execute_process(
                        COMMAND install_name_tool
                            -add_rpath \"@loader_path/${_qml_module_to_frameworks}\"
                            \"\${_qml_plugin_lib}\"
                        OUTPUT_QUIET ERROR_QUIET
                    )
                endforeach()
            ")
        endforeach()
    endif()

    # Collect bin/ directories from CMAKE_PREFIX_PATH (variable and environment) so
    # RUNTIME_DEPENDENCY_SET can find third-party DLLs. nbt++/rainbow are installed by the top-level
    # CMakeLists.txt; everything else under libraries/ links statically.
    set(_prefix_path_bins "")
    foreach(_prefix IN LISTS CMAKE_PREFIX_PATH)
        list(APPEND _prefix_path_bins "${_prefix}/bin")
    endforeach()
    if(DEFINED ENV{CMAKE_PREFIX_PATH})
        file(TO_CMAKE_PATH "$ENV{CMAKE_PREFIX_PATH}" _env_prefixes)
        foreach(_prefix IN LISTS _env_prefixes)
            list(APPEND _prefix_path_bins "${_prefix}/bin")
        endforeach()
    endif()
    list(REMOVE_DUPLICATES _prefix_path_bins)
    install(
        RUNTIME_DEPENDENCY_SET LAUNCHER_DEPENDENCY_SET
        DIRECTORIES
            ${CMAKE_SYSTEM_LIBRARY_PATH}
            ${QT_LIBS_DIR}
            ${QT_LIBEXECS_DIR}
            ${_prefix_path_bins}
        PRE_EXCLUDE_REGEXES
            "^(api-ms-win|ext-ms)-.*\\.dll$"
            "^azure.*\\.dll$"
            "^vcruntime.*\\.dll$"
        POST_EXCLUDE_REGEXES
            "system32"
        LIBRARY DESTINATION ${LIBRARY_DEST_DIR}
        RUNTIME DESTINATION ${BINARY_DEST_DIR}
        FRAMEWORK DESTINATION ${FRAMEWORK_DEST_DIR}
    )

    if(WIN32)
        install(CODE "
            execute_process(
                COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"${WINDEPLOYQT_OPTIONS_CODE}
                    --dir \"\${CMAKE_INSTALL_PREFIX}/${BINARY_DEST_DIR}\"
                    --libdir \"\${CMAKE_INSTALL_PREFIX}/${LIBRARY_DEST_DIR}\"
                    --plugindir \"\${CMAKE_INSTALL_PREFIX}/${PLUGIN_DEST_DIR}\"
                    \"\${CMAKE_INSTALL_PREFIX}/${BINARY_DEST_DIR}/$<TARGET_FILE_NAME:${Kite_Name}>\"
                RESULT_VARIABLE _deploy_result
            )
            if(NOT _deploy_result EQUAL 0)
                message(FATAL_ERROR \"windeployqt failed with exit code: \${_deploy_result}\")
            endif()
        ")

        # Qt loads OpenSSL with LoadLibrary rather than linking it, so
        # neither windeployqt nor RUNTIME_DEPENDENCY_SET sees it; without it Qt falls back to
        # Schannel. The libraries are named, not searched for: the right OpenSSL depends on the Qt
        # build (MSYS2's Qt wants the 3.x ABI), and CI passes the exact files.
        if(Kite_BUNDLE_OPENSSL_LIBS)
            foreach(_openssl_lib IN LISTS Kite_BUNDLE_OPENSSL_LIBS)
                if(NOT EXISTS "${_openssl_lib}")
                    message(FATAL_ERROR
                        "Kite_BUNDLE_OPENSSL_LIBS lists '${_openssl_lib}', "
                        "which does not exist. Shipping the package without it "
                        "would leave the launcher unable to make any HTTPS "
                        "request, so this is a hard error rather than a warning.")
                endif()
                message(STATUS "Bundling OpenSSL runtime: ${_openssl_lib}")
            endforeach()
            install(FILES ${Kite_BUNDLE_OPENSSL_LIBS} DESTINATION ${BINARY_DEST_DIR})
        endif()
    endif()

    # FIXME: remove this crap once we stop using msys2
    if(MINGW)
        install(CODE "
            set(QT_PLUGINS_DIR \"${QT_PLUGINS_DIR}\")
            set(QT_LIBS_DIR \"${QT_LIBS_DIR}\")
            set(QT_LIBEXECS_DIR \"${QT_LIBEXECS_DIR}\")
            set(CMAKE_SYSTEM_LIBRARY_PATH \"${CMAKE_SYSTEM_LIBRARY_PATH}\")
            set(CMAKE_INSTALL_PREFIX \"${CMAKE_INSTALL_PREFIX}\")
            ")

        install(CODE [[
            file(GLOB QT_IMAGEFORMAT_DLLS "${QT_PLUGINS_DIR}/imageformats/*.dll")
            set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL objdump)
            file(GET_RUNTIME_DEPENDENCIES
                RESOLVED_DEPENDENCIES_VAR imageformatdeps
                LIBRARIES ${QT_IMAGEFORMAT_DLLS}
                DIRECTORIES
                    ${CMAKE_SYSTEM_LIBRARY_PATH}
                    ${QT_PLUGINS_DIR}
                    ${QT_LIBS_DIR}
                    ${QT_LIBEXECS_DIR}
                PRE_EXCLUDE_REGEXES
                    "^(api-ms-win|ext-ms)-.*\\.dll$"
                    "^azure.*\\.dll$"
                    "^vcruntime.*\\.dll$"
                POST_EXCLUDE_REGEXES
                    "system32"
            )
            foreach(_lib ${imageformatdeps})
                file(INSTALL
                    DESTINATION ${CMAKE_INSTALL_PREFIX}
                    TYPE SHARED_LIBRARY
                    FOLLOW_SYMLINK_CHAIN
                    FILES ${_lib}
                )
            endforeach()
        ]])
    endif()

    # qt.conf tells Qt where plugins and QML modules are, relative to the executable. QmlImports is
    # the Qt 6 key and must be set explicitly, like Plugins, since nothing else on Windows/macOS
    # writes a qt.conf. The QT_CONF_*_DEST variables (top-level CMakeLists.txt) are those paths in
    # qt.conf form, not the install() DESTINATION form.
    install(
        CODE "file(WRITE \"\${CMAKE_INSTALL_PREFIX}/${RESOURCES_DEST_DIR}/qt.conf\" \"[Paths]\nPlugins = ${QT_CONF_PLUGINS_DEST}\nQmlImports = ${QT_CONF_QML_DEST}\n\")"
    )
    install(
        FILES "qtlogging.ini"
        DESTINATION ${CMAKE_INSTALL_PREFIX}/${RESOURCES_DEST_DIR}
    )
endif()
