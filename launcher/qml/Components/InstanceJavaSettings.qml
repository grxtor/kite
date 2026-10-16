// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * The Java part of one instance's settings (InstanceSettingsTab.qml):
 * memory, which Java runs the game, and its JVM arguments. Each group
 * follows the launcher-wide value (Settings, "Java & memory") until
 * switched to "custom for this instance". The tab hands in its own
 * SettingsSource and disables this whole column while the game runs.
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
        SettingNumber {
            source: root.source
            key: "MinMemAlloc"
            enabled: memory.overriding
            label: qsTr("Starting memory")
            from: 128
            to: root.source.number("MaxMemAlloc")
            stepSize: 128
            suffix: qsTr("MiB")
        }
    }

    OverrideGroup {
        id: javaPath
        width: parent.width
        title: qsTr("Java")
        source: root.source
        gateKey: "OverrideJavaLocation"
        gateLabel: qsTr("Use a specific Java for this instance")
        keys: ["JavaPath"]
        SettingText {
            source: root.source
            key: "JavaPath"
            enabled: javaPath.overriding
            label: qsTr("Java executable")
            monospace: true
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
        SettingText {
            source: root.source
            key: "JvmArgs"
            enabled: javaArgs.overriding
            label: qsTr("Arguments")
            monospace: true
        }
    }
}
