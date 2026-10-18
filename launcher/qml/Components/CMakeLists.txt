# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-FileContributor: grxtor
# SPDX-License-Identifier: Apache-2.0

######## Launcher-specific QML building blocks (Kite.Components) ########

# Mirrors launcher/qml/CMakeLists.txt: RESOURCE_PREFIX is spelled out by hand
# rather than left to qt_policy(QTP0001) (Qt 6.5+), since the floor here is
# 6.4, and this file lives beside the .qml it lists so the URI (Kite.Components)
# and the on-disk path agree.
#
# Not added to the parent CMakeLists here -- the integrator wires this
# library into the Kite_qml target once Kite.Theme exists to link against.
qt_add_library(Kite_qml_components STATIC)

# Same reasoning as Theme/CMakeLists.txt's Theme.qml: has to run before
# qt_add_qml_module, or Icons.qml registers as an ordinary re-instantiable
# type and every "import Kite.Components" gets its own private instance
# instead of the one Icons.url() is meant to be called on.
set_source_files_properties(Icons.qml Format.qml SettingsStore.qml JavaStore.qml LauncherStore.qml PixelArt.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# The fallback cover scenes: 32 rendered scenes at three crops
# (card/band/strip, see art/scenes/build.py), plus the world thumbnails
# (art/worlds), generic modpack covers (art/packs) and empty-state
# illustrations (art/empty/*.jpg), all built by art/scenes/build.py. Globbed
# rather than listed line by line -- ~100 checked-in JPGs -- with
# CONFIGURE_DEPENDS so rebuilding the set is picked up.
file(GLOB KITE_ART_SCENES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} CONFIGURE_DEPENDS
    art/scenes/card/*.jpg art/scenes/band/*.jpg art/scenes/strip/*.jpg
    art/worlds/*.jpg art/packs/*.jpg art/empty/*.jpg)

qt_add_qml_module(Kite_qml_components
    URI Kite.Components
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        CoverArt.qml
        InstanceCard.qml
        InstanceListRow.qml
        InstanceSection.qml
        LibraryPage.qml
        LinkButton.qml
        ContinueCard.qml
        CornerMatte.qml
        PlayButton.qml
        LaunchOptionsMenu.qml
        LaunchProgressBar.qml
        IconButton.qml
        Tag.qml
        ScrollFade.qml
        SearchBox.qml
        RecentItem.qml
        Format.qml
        SettingsPage.qml
        DiscoverPage.qml
        ModpackCard.qml
        ModpackDetail.qml
        Skeleton.qml
        SettingsScroll.qml
        SettingsGroup.qml
        SettingsStore.qml
        SettingsSource.qml
        OverrideGroup.qml
        InstanceSettingsTab.qml
        InstanceJavaSettings.qml
        JavaStore.qml
        JavaPathRow.qml
        JavaPickerDialog.qml
        JavaDownloadDialog.qml
        JavaVendorRow.qml
        JvmArgsRow.qml
        ManagedJavaList.qml
        LauncherStore.qml
        FolderRow.qml
        ExtraInstanceFolders.qml
        FontFamilyRow.qml
        CatPicture.qml
        CatPackPicker.qml
        ConsoleFontRow.qml
        ConsolePreview.qml
        TotalPlayTime.qml
        NewComponentDialog.qml
        InstanceOverviewTab.qml
        InstancePage.qml
        ContentTab.qml
        VersionTab.qml
        LoaderInstallDialog.qml
        MinecraftVersionDialog.qml
        GameOptionsTab.qml
        GameOptionRow.qml
        PackList.qml
        PalettePicker.qml
        ContentBrowserView.qml
        PackRow.qml
        ContentSelectionBar.qml
        ExportContentListDialog.qml
        InstallReviewDialog.qml
        ContentFilterPopup.qml
        NoticeBar.qml
        ReviewRow.qml
        VersionRow.qml
        UpdateReviewDialog.qml
        ChangeVersionDialog.qml
        ContentUpdateFlows.qml
        FieldLabel.qml
        ExportInstanceDialog.qml
        CreateShortcutDialog.qml
        InstanceExportFlows.qml
        DuplicateInstanceDialog.qml
        PluginNode.qml
        PluginSurfaces.qml
        UiRequestDialog.qml
        AppMenuBar.qml
        BusyOverlay.qml
        OnboardingView.qml
        WorldsTab.qml
        ScreenshotsTab.qml
        LogTab.qml
        LogSourcePicker.qml
        LogLines.qml
        LogFindBar.qml
        OtherLogsView.qml
        ClipboardText.qml
        TabScroll.qml
        TabStrip.qml
        TabToolbar.qml
        StatTile.qml
        ConfirmDialog.qml
        DialogHeader.qml
        AccountsPage.qml
        AccountRow.qml
        SkinCapeEditor.qml
        SkinTile.qml
        SkinPreview.qml
        CapeTile.qml
        NewInstanceDialog.qml
        PromptDialog.qml
        Toast.qml
        IconPickerDialog.qml
        SettingRow.qml
        SettingSwitch.qml
        SettingChoice.qml
        SettingNumber.qml
        SettingText.qml
        SettingPathField.qml
        SettingsProxySection.qml
        SettingsExternalToolsSection.qml
        SettingsLogUploadSection.qml
        SettingsGeneralSection.qml
        SettingsJavaSection.qml
        SettingsMinecraftSection.qml
        SettingsConsoleSection.qml
        SettingsAppearanceSection.qml
        SettingsCommandsSection.qml
        SettingsPluginsSection.qml
        SettingsPluginsListSection.qml
        SettingsLauncherLogsSection.qml
        SettingsUpdatesSection.qml
        SettingsAboutSection.qml
        MemorySetting.qml
        MemoryField.qml
        MemoryStartSetting.qml
        SegmentedControl.qml
        EmptyState.qml
        SidebarNav.qml
        NavItem.qml
        NavSelectionIndicator.qml
        AccountChip.qml
        ProfileButton.qml
        ProfileMenu.qml
        PlayDock.qml
        TopBar.qml
        SectionHeader.qml
        StatusBadge.qml
        Gallery.qml
        MeshIcon.qml
        Icons.qml
        PixelArt.qml
        HomePage.qml
        HomeJumpCard.qml
        HomeWorldTile.qml
        HoverReason.qml
        HomeLibraryTile.qml
        AmbientPattern.qml
        PageBackdrop.qml
        BrandMark.qml
        SkinSilhouette.qml
        ServersTab.qml
        BackupsTab.qml
        DataPacksTab.qml
        ManagedPackTab.qml
        RemoteRichText.qml
        PlayerHead.qml
        ViewModeToggle.qml
        ChipGroup.qml
        InstanceNotesTab.qml
        LegacyUpgradeNotice.qml
        ContentResultRow.qml
        ContentProjectPanel.qml
    RESOURCES
        icons/alert-triangle.svg
        icons/archive.svg
        icons/arrow-down.svg
        icons/arrow-right.svg
        icons/arrow-up.svg
        icons/bell.svg
        icons/check.svg
        icons/chevron-down.svg
        icons/chevron-left.svg
        icons/chevron-right.svg
        icons/clock.svg
        icons/compass.svg
        icons/copy.svg
        icons/cube.svg
        icons/download.svg
        icons/edit.svg
        icons/external-link.svg
        icons/file.svg
        icons/filter.svg
        icons/folder.svg
        icons/folder-input.svg
        icons/globe.svg
        icons/grid.svg
        icons/home.svg
        icons/hourglass.svg
        icons/image.svg
        icons/info.svg
        icons/layers.svg
        icons/library.svg
        icons/list.svg
        icons/log-out.svg
        icons/more.svg
        icons/moon.svg
        icons/package.svg
        icons/play.svg
        icons/plus.svg
        icons/refresh.svg
        icons/search.svg
        icons/server.svg
        icons/settings.svg
        icons/sliders.svg
        icons/sort.svg
        icons/stop.svg
        icons/sun.svg
        icons/terminal.svg
        icons/trash.svg
        icons/user.svg
        icons/users.svg
        icons/x.svg
        # Original pixel art (see art/generate.py) -- checked-in PNGs, never
        # generated at build or run time.
        art/blocks/cobblestone.png
        art/blocks/deepslate.png
        art/blocks/dirt.png
        art/blocks/grass_side.png
        art/blocks/grass_top.png
        art/blocks/gravel.png
        art/blocks/oak_planks.png
        art/blocks/sand.png
        art/blocks/stone.png
        art/ambient/blocks_mask.png
        ${KITE_ART_SCENES}
        art/hero/home_hero.png
        art/hero/home_day.jpg
        art/hero/home_golden.jpg
        art/hero/home_night.jpg
        art/empty/no_instances.png
        art/empty/no_worlds.png
        art/empty/not_found.png
    IMPORTS
        Kite.Theme
)

# MeshIcon.qml reaches into QtQuick.Controls.impl (IconImage) directly, same
# as the Style module's use of the same import (see Style/CMakeLists.txt) --
# Quick + QuickControls2 already resolve it at runtime, so no separate
# Qt::QuickControls2Impl link is needed.
target_link_libraries(Kite_qml_components PUBLIC
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
)
