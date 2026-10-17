// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The "Java & memory" section of SettingsPage.qml: how much memory
 * Minecraft gets and which Java runtime runs it, launcher-wide. Each
 * instance can override these in its own InstanceJavaSettings.qml.
 *
 * The Java rows (path with Choose, Browse and Test, JVM arguments, the
 * download vendor, the Java MeshMC downloaded) work through JavaStore; see
 * JavaPathRow.qml and friends. PermGen only appears for a Java old enough
 * to use it.
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
            description: qsTr("Used when an instance does not choose its own. Leave empty to let MeshMC pick.")
        }
        JvmArgsRow {
            label: qsTr("JVM arguments")
            description: qsTr("Extra flags for every launch. MeshMC already sets memory and the classpath.")
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

        Text {
            text: qsTr("Downloaded Java")
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.title.pixelSize
            font.weight: Font.Bold
        }
        Text {
            width: parent.width
            text: qsTr("Java that MeshMC downloaded and keeps in its own folder.")
            wrapMode: Text.Wrap
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
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

        Text {
            width: parent.width
            visible: root.removeError.length > 0
            text: root.removeError
            wrapMode: Text.Wrap
            color: Theme.palette.danger
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
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
                text: qsTr("Refresh")
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
