// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The "Java & memory" section of SettingsPage.qml: how much memory Minecraft gets
 * and which Java runtime runs it, launcher-wide. Instances can override these in
 * InstanceJavaSettings.qml. PermGen only appears for a Java old enough to use it.
 */
SettingsScroll {
    id: root

    // Installed memory in MiB: the ceiling for the memory slider.
    property int systemMemoryMiB: 8192

    readonly property var java: JavaStore.controller
    readonly property bool canDownload: !!java && java.downloaderAvailable
    property string removeError

    title: qsTr("Java & memory")
    description: qsTr("How much memory Minecraft gets, and which Java runtime runs it.")

    Connections {
        target: JavaStore.download
        ignoreUnknownSignals: true
        function onInstalled() {
            if (root.java)
                root.java.refreshManaged()
        }
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("Memory")
        MemorySetting {
            label: qsTr("Maximum memory")
            systemMiB: root.systemMemoryMiB
        }
        MemoryStartSetting {
            label: qsTr("Starting memory")
            description: qsTr("Memory reserved when the game starts. Never above the maximum.")
        }
        SettingNumber {
            visible: javaPath.legacy
            key: "PermGen"
            label: qsTr("PermGen size")
            description: qsTr("Extra memory for Java older than 8. Newer Java ignores it.")
            from: 64
            to: 4096
            stepSize: 64
            suffix: qsTr("MiB")
        }
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("Java runtime")
        JavaPathRow {
            id: javaPath
            label: qsTr("Java executable")
            description: qsTr("Used when an instance does not choose its own. Leave empty to let Kite pick.")
        }
        JvmArgsRow {
            label: qsTr("JVM arguments")
            description: qsTr("Extra flags for every launch. Kite already sets memory and the classpath.")
        }
        // Windows keeps this per program; Kite writes it for the Java of each launch.
        SettingSwitch {
            visible: Qt.platform.os === "windows"
            key: "UseHighPerformanceGpu"
            label: qsTr("Use the high-performance graphics card")
            description: qsTr("For computers with two graphics cards: Java runs on the faster one. A choice you made for Java in the Windows graphics settings stays.")
        }
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("Automatic Java")
        SettingSwitch {
            key: "JavaAutoDownload"
            label: qsTr("Download Java automatically")
            description: qsTr("Fetches the Java version each Minecraft release needs, so you never have to install one yourself.")
        }
        JavaVendorRow {
            enabled: SettingsStore.bool("JavaAutoDownload")
            label: qsTr("Download from")
            description: qsTr("Which vendor the automatic downloads come from.")
        }
    }

    Column {
        width: parent.width
        spacing: Theme.space.sm
        visible: root.canDownload

        GroupHeading {
            width: parent.width
            title: qsTr("Downloaded Java")
            description: qsTr("Java that Kite downloaded and keeps in its own folder.")
        }

        ManagedJavaList {
            width: parent.width
            currentPath: SettingsStore.string("JavaPath")
            onUseRequested: (path) => {
                root.removeError = ""
                SettingsStore.setValue("JavaPath", path)
            }
            onFailed: (message) => root.removeError = message
        }

        Copy {
            width: parent.width
            visible: root.removeError.length > 0
            text: root.removeError
            wrapMode: Text.Wrap
            tone: "danger"
            role: "label"
        }

        Row {
            spacing: Theme.space.sm
            Button {
                text: qsTr("Download Java…")
                icon.source: Icons.url("download")
                onClicked: downloadDialog.open()
            }
            Button {
                flat: true
                text: qsTr("Detect again")
                icon.source: Icons.url("refresh")
                onClicked: root.java.refreshManaged()
            }
        }
    }

    JavaDownloadDialog {
        id: downloadDialog
        onUseRequested: (path) => SettingsStore.setValue("JavaPath", path)
    }
}
