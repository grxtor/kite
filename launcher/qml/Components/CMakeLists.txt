# SPDX-FileCopyrightText: 2026 Project Tick
# SPDX-FileContributor: Project Tick
# SPDX-License-Identifier: Apache-2.0

######## Launcher-specific QML building blocks (MeshMC.Components) ########

# Mirrors launcher/qml/CMakeLists.txt: RESOURCE_PREFIX is spelled out by hand
# rather than left to qt_policy(QTP0001) (Qt 6.5+), since the floor here is
# 6.4, and this file lives beside the .qml it lists so the URI (MeshMC.Components)
# and the on-disk path agree.
#
# Not added to the parent CMakeLists here -- the integrator wires this
# library into the MeshMC_qml target once MeshMC.Theme exists to link against.
qt_add_library(MeshMC_qml_components STATIC)

# Same reasoning as Theme/CMakeLists.txt's Theme.qml: has to run before
# qt_add_qml_module, or Icons.qml registers as an ordinary re-instantiable
# type and every "import MeshMC.Components" gets its own private instance
# instead of the one Icons.url() is meant to be called on.
set_source_files_properties(Icons.qml Format.qml SettingsStore.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(MeshMC_qml_components
    URI MeshMC.Components
    VERSION 1.0
    RESOURCE_PREFIX "/qt/qml"
    QML_FILES
        CoverArt.qml
        InstanceCard.qml
        InstanceListRow.qml
        InstanceSection.qml
        LibraryPage.qml
        ContinueCard.qml
        PlayButton.qml
        LaunchProgressBar.qml
        IconButton.qml
        Tag.qml
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
        InstanceOverviewTab.qml
        InstancePage.qml
        ContentTab.qml
        PackList.qml
        PalettePicker.qml
        ContentBrowserView.qml
        PluginNode.qml
        PluginSurfaces.qml
        UiRequestDialog.qml
        BusyOverlay.qml
        OnboardingView.qml
        WorldsTab.qml
        ScreenshotsTab.qml
        LogTab.qml
        TabStrip.qml
        StatTile.qml
        ConfirmDialog.qml
        DialogHeader.qml
        AccountsPage.qml
        AccountRow.qml
        SkinCapeEditor.qml
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
        MemorySetting.qml
        SegmentedControl.qml
        EmptyState.qml
        SidebarNav.qml
        NavItem.qml
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
        HomePage.qml
        HomeJumpCard.qml
        HomeWorldTile.qml
    RESOURCES
        icons/alert-triangle.svg
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
        icons/folder.svg
        icons/globe.svg
        icons/grid.svg
        icons/home.svg
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
        icons/settings.svg
        icons/sort.svg
        icons/stop.svg
        icons/sun.svg
        icons/terminal.svg
        icons/trash.svg
        icons/user.svg
        icons/users.svg
        icons/x.svg
    IMPORTS
        MeshMC.Theme
)

# MeshIcon.qml reaches into QtQuick.Controls.impl (IconImage) directly, same
# as the Style module's use of the same import (see Style/CMakeLists.txt) --
# Quick + QuickControls2 already resolve it at runtime, so no separate
# Qt::QuickControls2Impl link is needed.
target_link_libraries(MeshMC_qml_components PUBLIC
    Qt${QT_VERSION_MAJOR}::Quick
    Qt${QT_VERSION_MAJOR}::QuickControls2
)
