// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * The Java part of one instance's settings (InstanceSettingsTab.qml):
 * memory, which Java runs the game (with Choose, Browse and Test, see
 * JavaPathRow.qml), its JVM arguments and the vendor automatic downloads
 * come from. Each group follows the launcher-wide value (Settings, "Java &
 * memory") until switched to "custom for this instance". The tab hands in
 * its own SettingsSource and disables this whole column while the game
 * runs. Downloading and removing Java is launcher-wide, so a link points
 * to Settings for that.
 */
Column {
    id: root

    // The tab's SettingsSource over this instance's settings.
    property var source
    property int systemMemoryMiB: 8192

    spacing: Theme.space.xl

    OverrideGroup {
        id: memory
        width: parent.width
        title: qsTr("Memory")
        source: root.source
        gateKey: "OverrideMemory"
        keys: ["MinMemAlloc", "MaxMemAlloc", "PermGen"]
        MemorySetting {
            source: root.source
            enabled: memory.overriding
            label: qsTr("Maximum memory")
            systemMiB: root.systemMemoryMiB
        }
        MemoryStartSetting {
            source: root.source
            enabled: memory.overriding
            label: qsTr("Starting memory")
        }
        SettingNumber {
            source: root.source
            key: "PermGen"
            visible: javaLocation.legacy
            enabled: memory.overriding
            label: qsTr("PermGen size")
            description: qsTr("Extra memory for Java older than 8. Newer Java ignores it.")
            from: 64
            to: 4096
            stepSize: 64
            suffix: qsTr("MiB")
        }
    }

    // The Java choice and, under it, the way to the launcher-wide Java
    // downloads.
    Column {
        width: parent.width
        spacing: Theme.space.sm

        OverrideGroup {
            id: javaPath
            width: parent.width
            title: qsTr("Java")
            source: root.source
            gateKey: "OverrideJavaLocation"
            gateLabel: qsTr("Use a specific Java for this instance")
            keys: ["JavaPath"]
            JavaPathRow {
                id: javaLocation
                source: root.source
                enabled: javaPath.overriding
                label: qsTr("Java executable")
            }
        }

        LinkButton {
            x: parent.width - width
            visible: !!JavaStore.controller
            text: qsTr("Download or remove Java in Settings")
            onClicked: JavaStore.navigateRequested("settings/java")
        }
    }

    OverrideGroup {
        id: javaArgs
        width: parent.width
        title: qsTr("JVM arguments")
        source: root.source
        gateKey: "OverrideJavaArgs"
        gateLabel: qsTr("Use custom JVM arguments for this instance")
        keys: ["JvmArgs"]
        JvmArgsRow {
            source: root.source
            enabled: javaArgs.overriding
            label: qsTr("Arguments")
        }
    }

    OverrideGroup {
        id: javaVendor
        width: parent.width
        visible: !!JavaStore.controller && JavaStore.controller.downloaderAvailable
        title: qsTr("Automatic Java")
        source: root.source
        gateKey: "OverrideJavaAutoDownloadVendor"
        gateLabel: qsTr("Download Java from a different vendor for this instance")
        keys: ["JavaAutoDownloadVendor"]
        JavaVendorRow {
            source: root.source
            enabled: javaVendor.overriding
            label: qsTr("Download from")
        }
    }
}
