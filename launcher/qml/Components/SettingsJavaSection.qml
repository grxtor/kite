// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * The "Java & memory" section of SettingsPage.qml: how much memory
 * Minecraft gets and which Java runtime runs it, launcher-wide. Each
 * instance can override these in its own InstanceJavaSettings.qml.
 */
SettingsScroll {
    id: root

    // Installed memory in MiB: the ceiling for the memory slider.
    property int systemMemoryMiB: 8192

    title: qsTr("Java & memory")
    description: qsTr("How much memory Minecraft gets, and which Java runtime runs it.")
    SettingsGroup {
        width: parent.width
        title: qsTr("Memory")
        MemorySetting {
            label: qsTr("Maximum memory")
            hint: qsTr("How much memory Minecraft may use. Big modpacks want 6–8 GiB; vanilla is happy with 2–4.")
            systemMiB: root.systemMemoryMiB
        }
        SettingNumber {
            key: "MinMemAlloc"
            label: qsTr("Starting memory")
            description: qsTr("Memory reserved when the game starts. Never above the maximum.")
            from: 128
            to: SettingsStore.number("MaxMemAlloc")
            stepSize: 128
            suffix: qsTr("MiB")
        }
    }
    SettingsGroup {
        width: parent.width
        title: qsTr("Java runtime")
        SettingSwitch {
            key: "JavaAutoDownload"
            label: qsTr("Download Java automatically")
            description: qsTr("Fetches the Java version each Minecraft release needs, so you never have to install one yourself.")
        }
        SettingText {
            key: "JavaPath"
            label: qsTr("Java executable")
            description: qsTr("Used when an instance does not choose its own. Leave empty to let MeshMC pick.")
            placeholder: qsTr("Automatic")
            monospace: true
        }
        SettingText {
            key: "JvmArgs"
            label: qsTr("JVM arguments")
            description: qsTr("Extra flags for every launch. MeshMC already sets memory and the classpath.")
            placeholder: "-XX:+UseG1GC"
            monospace: true
        }
    }
}
