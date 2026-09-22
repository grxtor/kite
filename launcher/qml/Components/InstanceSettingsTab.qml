// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * One instance's own settings: each group follows the launcher-wide value
 * until switched to "custom for this instance". Locked while the instance
 * runs -- the game already read these, a change now would silently do
 * nothing until the next launch.
 */
SettingsScroll {
    id: root

    // SettingsAdapter over the instance's settings.
    property var adapter: null
    property int systemMemoryMiB: 8192
    property bool running: false

    title: qsTr("Instance settings")

    SettingsSource {
        id: store
        adapter: root.adapter
    }

    Rectangle {
        visible: root.running
        width: parent.width
        height: lockedText.implicitHeight + Theme.space.md * 2
        radius: Theme.radius.lg
        color: Theme.palette.warningSubtle

        Text {
            id: lockedText
            anchors.fill: parent
            anchors.margins: Theme.space.md
            text: qsTr("The game is running. Settings can be changed once it has closed.")
            wrapMode: Text.Wrap
            verticalAlignment: Text.AlignVCenter
            color: Theme.palette.warning
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            font.weight: Font.Medium
        }
    }

    Column {
        width: parent.width
        spacing: Theme.space.xl
        enabled: !root.running

        OverrideGroup {
            id: memory
            width: parent.width
            title: qsTr("Memory")
            source: store
            gateKey: "OverrideMemory"
            keys: ["MinMemAlloc", "MaxMemAlloc", "PermGen"]
            MemorySetting {
                source: store
                enabled: memory.overriding
                label: qsTr("Maximum memory")
                systemMiB: root.systemMemoryMiB
            }
            SettingNumber {
                source: store
                key: "MinMemAlloc"
                enabled: memory.overriding
                label: qsTr("Starting memory")
                from: 128
                to: store.number("MaxMemAlloc")
                stepSize: 128
                suffix: qsTr("MiB")
            }
        }

        OverrideGroup {
            id: javaPath
            width: parent.width
            title: qsTr("Java")
            source: store
            gateKey: "OverrideJavaLocation"
            gateLabel: qsTr("Use a specific Java for this instance")
            keys: ["JavaPath"]
            SettingText {
                source: store
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
            source: store
            gateKey: "OverrideJavaArgs"
            gateLabel: qsTr("Use custom JVM arguments for this instance")
            keys: ["JvmArgs"]
            SettingText {
                source: store
                key: "JvmArgs"
                enabled: javaArgs.overriding
                label: qsTr("Arguments")
                monospace: true
            }
        }

        OverrideGroup {
            id: windowGroup
            width: parent.width
            title: qsTr("Game window")
            source: store
            gateKey: "OverrideWindow"
            keys: ["LaunchMaximized", "MinecraftWinWidth", "MinecraftWinHeight"]
            SettingSwitch {
                id: maximized
                source: store
                key: "LaunchMaximized"
                enabled: windowGroup.overriding
                label: qsTr("Start maximized")
            }
            SettingNumber {
                source: store
                key: "MinecraftWinWidth"
                enabled: windowGroup.overriding && !maximized.checked
                label: qsTr("Window width")
                from: 1
                to: 65536
                suffix: qsTr("px")
            }
            SettingNumber {
                source: store
                key: "MinecraftWinHeight"
                enabled: windowGroup.overriding && !maximized.checked
                label: qsTr("Window height")
                from: 1
                to: 65536
                suffix: qsTr("px")
            }
        }

        OverrideGroup {
            id: consoleGroup
            width: parent.width
            title: qsTr("Console")
            source: store
            gateKey: "OverrideConsole"
            keys: ["ShowConsole", "AutoCloseConsole", "ShowConsoleOnError"]
            SettingSwitch {
                source: store
                key: "ShowConsole"
                enabled: consoleGroup.overriding
                label: qsTr("Show the console while playing")
            }
            SettingSwitch {
                source: store
                key: "AutoCloseConsole"
                enabled: consoleGroup.overriding
                label: qsTr("Close the console when the game quits")
            }
            SettingSwitch {
                source: store
                key: "ShowConsoleOnError"
                enabled: consoleGroup.overriding
                label: qsTr("Show the console when the game crashes")
            }
        }

        OverrideGroup {
            id: commands
            width: parent.width
            title: qsTr("Custom commands")
            source: store
            gateKey: "OverrideCommands"
            keys: ["PreLaunchCommand", "WrapperCommand", "PostExitCommand"]
            SettingText {
                source: store
                key: "PreLaunchCommand"
                enabled: commands.overriding
                label: qsTr("Before launch")
                monospace: true
            }
            SettingText {
                source: store
                key: "WrapperCommand"
                enabled: commands.overriding
                label: qsTr("Wrapper")
                monospace: true
            }
            SettingText {
                source: store
                key: "PostExitCommand"
                enabled: commands.overriding
                label: qsTr("After exit")
                monospace: true
            }
        }

        SettingsGroup {
            width: parent.width
            title: qsTr("On launch")
            SettingSwitch {
                id: joinServer
                source: store
                key: "JoinServerOnLaunch"
                label: qsTr("Join a server when the game starts")
            }
            SettingText {
                source: store
                key: "JoinServerOnLaunchAddress"
                enabled: joinServer.checked
                label: qsTr("Server address")
                placeholder: "play.example.net"
            }
        }
    }
}
