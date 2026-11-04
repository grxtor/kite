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
    models/WorldCopyTask.h
    models/ContentDrop.h
    models/ContentDrop.cpp
    models/ContentLinkInstall.h
    models/ContentLinkInstall.cpp
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

    Json.h
    Json.cpp

    FileSystem.h
    FileSystem.cpp

    Exception.h

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

)

add_unit_test(Json
    SOURCES Json_test.cpp
    LIBS Kite_logic
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
    minecraft/auth/MinecraftServices.h
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
    minecraft/launch/GameProcessStep.cpp
    minecraft/launch/GameProcessStep.h
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
    minecraft/mod/DroppedContent.h
    minecraft/mod/DroppedContent.cpp
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

add_unit_test(DroppedContent
    SOURCES minecraft/mod/DroppedContent_test.cpp
    LIBS Kite_core
    )

add_unit_test(VersionRange
    SOURCES minecraft/mod/VersionRange_test.cpp
    LIBS Kite_core
    )

add_unit_test(Version
    SOURCES Version_test.cpp
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
    java/GpuPreference.h
    java/GpuPreference.cpp
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

add_unit_test(GpuPreference
    SOURCES java/GpuPreference_test.cpp
    LIBS Kite_core
    )

add_unit_test(JavaRuntime
    SOURCES java/download/JavaRuntime_test.cpp
    LIBS Kite_core
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
    meta/MetaCatalog.cpp
    meta/MetaCatalog.h
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
    modplatform/ContentApis.h
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
    modplatform/PlatformCover.h
    modplatform/PlatformCover.cpp
    modplatform/PlatformCoverFetcher.h
    modplatform/PlatformCoverFetcher.cpp
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
    friends/FriendsNotices.h
    friends/FriendsNotices.cpp
    friends/FriendsInvites.h
    friends/FriendsInvites.cpp
    friends/ShareCodes.h
    friends/ShareCodes.cpp
    friends/ShareCodesReplies.cpp
    friends/ShareManifest.h
    friends/ShareManifest.cpp
    friends/ShareContent.h
    friends/ShareContent.cpp
    friends/ShareCodeInstanceTask.h
    friends/ShareCodeInstanceTask.cpp
    friends/ShareCodesController.h
    friends/ShareCodesController.cpp
    friends/ShareCodesCreate.cpp
    friends/ShareCodesNames.cpp
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
    friends/FriendsInviteActions.cpp
    friends/FriendsBridge.cpp
    friends/FriendsNotify.cpp
    friends/FriendsChatApi.h
    friends/FriendsChatApi.cpp
    friends/FriendsChatModel.h
    friends/FriendsChatModel.cpp
    friends/FriendsChat.h
    friends/FriendsChat.cpp
    friends/FriendsChatHistory.cpp
    friends/FriendsChatSend.cpp
    friends/FriendsEvents.h
    friends/FriendsEvents.cpp
    friends/FriendsSocket.h
    friends/FriendsSocket.cpp
    friends/FriendsRealtime.cpp
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

add_unit_test(FriendsNotices
    SOURCES friends/FriendsNotices_test.cpp
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

add_unit_test(FriendsChat
    SOURCES friends/FriendsChat_test.cpp
    LIBS Kite_core
    )

add_unit_test(FriendsChatApi
    SOURCES friends/FriendsChatApi_test.cpp
    LIBS Kite_core
    )

add_unit_test(FriendsEvents
    SOURCES friends/FriendsEvents_test.cpp
    LIBS Kite_core
    )

# Talks to a QWebSocketServer of its own.
add_unit_test(FriendsSocket
    SOURCES friends/FriendsSocket_test.cpp
    LIBS Kite_core Qt${QT_VERSION_MAJOR}::WebSockets
    )

# Two players chatting through a real friends server; skipped unless
# KITE_FRIENDS_E2E_URL names one (see the test's header).
add_unit_test(FriendsChatE2E
    SOURCES friends/FriendsChatE2E_test.cpp
    LIBS Kite_core Qt${QT_VERSION_MAJOR}::WebSockets
    )

add_unit_test(FriendsInvites
    SOURCES friends/FriendsInvites_test.cpp
    LIBS Kite_core
    )

add_unit_test(ShareCodes
    SOURCES friends/ShareCodes_test.cpp
    LIBS Kite_core
    )

set(SERVERS_SOURCES
    servers/ServerStatus.h
    servers/ServerStatus.cpp
    servers/ServerPing.h
    servers/ServerPing.cpp
    servers/ServerPinger.h
    servers/ServerPinger.cpp
    servers/FeaturedServers.h
    servers/FeaturedServers.cpp
)

add_unit_test(ServerStatus
    SOURCES servers/ServerStatus_test.cpp
    LIBS Kite_core
    )

add_unit_test(ServerPing
    SOURCES servers/ServerPing_test.cpp
    LIBS Kite_core
    )

add_unit_test(FeaturedServers
    SOURCES servers/FeaturedServers_test.cpp
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

set(CRASHDOCTOR_SOURCES
    crashdoctor/CrashActions.h
    crashdoctor/CrashActions.cpp
    crashdoctor/CrashCollector.h
    crashdoctor/CrashCollector.cpp
    crashdoctor/CrashDoctorModel.h
    crashdoctor/CrashDoctorModel.cpp
    crashdoctor/CrashDoctorService.h
    crashdoctor/CrashDoctorService.cpp
    crashdoctor/CrashEngine.h
    crashdoctor/CrashEngine.cpp
    crashdoctor/CrashFiles.h
    crashdoctor/CrashFiles.cpp
    crashdoctor/CrashFixer.h
    crashdoctor/CrashFixer.cpp
    crashdoctor/CrashRules.h
    crashdoctor/CrashRules.cpp
)

set(LOGIC_SOURCES
    ${CRASHDOCTOR_SOURCES}
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
    ${FLAME_SOURCES}
    ${MODPACKSCH_SOURCES}
    ${TECHNIC_SOURCES}
    ${ATLAUNCHER_SOURCES}
    ${MODRINTH_SOURCES}
    ${CONTENT_DOWNLOAD_SOURCES}
    ${FRIENDS_SOURCES}
    ${SERVERS_SOURCES}
)

set(KITE_SOURCES
    Application.h
    Application.cpp
    ApplicationMessage.h
    ApplicationMessage.cpp

    HoeDown.h

    resources/backgrounds/backgrounds.qrc
    resources/multimc/multimc.qrc
    resources/documents/documents.qrc
    ${CMAKE_BINARY_DIR}/${Kite_AppBinaryName}.qrc

    ui/LaunchController.h
    ui/LaunchController.cpp
)

set(KITE_QRC_FILES
    resources/backgrounds/backgrounds.qrc
    resources/multimc/multimc.qrc
    resources/documents/documents.qrc
    ${CMAKE_BINARY_DIR}/${Kite_AppBinaryName}.qrc
)

qt6_add_resources(KITE_RESOURCES ${KITE_QRC_FILES})

if(WIN32)
    set(KITE_RCS ${CMAKE_BINARY_DIR}/${Kite_AppBinaryName}.rc)
endif()

# Kite_core works without a user interface: it must not link QtWidgets or know about the plugin host
# or ui/. Kite_logic is the plugin host and the application on top of it.
add_library(Kite_core STATIC ${LOGIC_SOURCES})

# Embedded as :/kite/bundles.json and :/kite/servers.json; in the core so tests find them too.
qt6_add_resources(Kite_core kite_bundles
    PREFIX "/kite"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources/kite"
    FILES resources/kite/bundles.json resources/kite/servers.json)
# The crash doctor's catalog, embedded as :/kite/crash-rules.json (replaced by
# meta.kiteclient.com's copy when there is a valid newer one).
qt6_add_resources(Kite_core kite_crash_rules
    PREFIX "/kite"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources/kite"
    FILES resources/kite/crash-rules.json)

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
add_library(Kite_logic STATIC ${PLUGIN_SOURCES} ${KITE_SOURCES} ${KITE_RESOURCES})
target_link_libraries(Kite_logic Kite_core)

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
add_unit_test(LoggedProcess
    SOURCES LoggedProcess_test.cpp
    LIBS Kite_core
    )
add_unit_test(CrashEngine
    SOURCES crashdoctor/CrashEngine_test.cpp
    LIBS Kite_core
    )
add_unit_test(CrashFixer
    SOURCES crashdoctor/CrashFixer_test.cpp
    LIBS Kite_core
    )
add_unit_test(CoreSupersededPlugins
    SOURCES plugin/CoreSupersededPlugins_test.cpp
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
add_unit_test(PlatformCover
    SOURCES modplatform/PlatformCover_test.cpp
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

add_unit_test(IconList
    SOURCES icons/IconList_test.cpp
    LIBS Kite_core Qt${QT_VERSION_MAJOR}::Gui
    )

add_unit_test(AutoIcons
    SOURCES icons/AutoIcons_test.cpp icons/AutoIcons.cpp
    LIBS Qt${QT_VERSION_MAJOR}::Core
    )
target_include_directories(AutoIcons_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Needs the compiled icon themes (multimc.qrc), hence Kite_logic.
add_unit_test(AutoIconsBuiltIn
    SOURCES icons/AutoIconsBuiltIn_test.cpp
    LIBS Kite_logic Qt${QT_VERSION_MAJOR}::Gui
    )

add_unit_test(GameTextureIcons
    SOURCES icons/GameTextureIcons_test.cpp
    LIBS Kite_core Qt${QT_VERSION_MAJOR}::Gui
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
    LocalPeer::LocalPeer
)
# The friends server's realtime socket (friends/FriendsSocket.cpp); nothing else uses it.
target_link_libraries(Kite_core PRIVATE Qt${QT_VERSION_MAJOR}::WebSockets)

# QtWidgets is linked here and only here (enforced by Kite_core_link_check).
target_link_libraries(Kite_logic Qt${QT_VERSION_MAJOR}::Widgets)

target_link_libraries(Kite_logic Kite_qml)

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
    # PUBLIC: MacSparkleUpdater.mm in Kite_core needs Cocoa.
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

# The friends panel's conversation over a real FriendsChat; KITE_CHAT_SHOTS=<dir> also saves each state as a PNG.
add_unit_test(FriendChatQml
    SOURCES qml/FriendChatQml_test.cpp qml/FriendChatQmlHarness.h
    LIBS Kite_qml Kite_qmlplugin Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Gui
    )
qt_import_qml_plugins(FriendChatQml_test)

add_unit_test(FriendChatPanelQml
    SOURCES qml/FriendChatPanelQml_test.cpp qml/FriendChatQmlHarness.h
    LIBS Kite_qml Kite_qmlplugin Qt${QT_VERSION_MAJOR}::Quick Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Gui
    )
qt_import_qml_plugins(FriendChatPanelQml_test)

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
        ui/dialogs/ProgressDialog.ui
    )

    # Not the versionless qt_wrap_ui: it is a no-op unless QT_DEFAULT_MAJOR_VERSION is set.
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
    )

    add_library(kite_updater_logic STATIC ${MESHUPDATER_SOURCES} ${MESHUPDATER_UI})
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

    add_unit_test(GitHubRelease
        SOURCES updater/meshupdater/GitHubRelease_test.cpp
        LIBS kite_updater_logic
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
    # RUNTIME_DEPENDENCY_SET can find third-party DLLs. nbt++ is installed by the top-level
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
