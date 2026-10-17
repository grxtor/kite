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
 * nothing until the next launch. A TabScroll: the tab strip names the tab,
 * so there is no heading, and the groups run the page's full width.
 */
TabScroll {
    id: root

    // SettingsAdapter over the instance's settings.
    property var adapter: null
    property int systemMemoryMiB: 8192
    property bool running: false
    // PluginSurfaceModel for this instance's settings anchor, or null.
    property var pluginSurfaces: null

    description: qsTr("Each group follows the launcher's settings until you switch it to custom for this instance.")

    SettingsSource {
        id: store
        adapter: root.adapter
    }

    NoticeBar {
        visible: root.running
        width: parent.width
        text: qsTr("The game is running. Settings can be changed once it has closed.")
    }

    Column {
        width: parent.width
        spacing: Theme.space.xl
        enabled: !root.running

        // Memory, Java and JVM arguments: their own file, so Java
        // management can grow there without touching the rest of this tab.
        InstanceJavaSettings {
            width: parent.width
            source: store
            systemMemoryMiB: root.systemMemoryMiB
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

        OverrideGroup {
            id: gameTime
            width: parent.width
            title: qsTr("Play time")
            source: store
            gateKey: "OverrideGameTime"
            keys: ["RecordGameTime", "ShowGameTime"]
            SettingSwitch {
                source: store
                key: "RecordGameTime"
                enabled: gameTime.overriding
                label: qsTr("Record play time")
            }
            SettingSwitch {
                source: store
                key: "ShowGameTime"
                enabled: gameTime.overriding
                label: qsTr("Show play time")
            }
        }

        OverrideGroup {
            id: workarounds
            width: parent.width
            title: qsTr("Native libraries")
            source: store
            gateKey: "OverrideNativeWorkarounds"
            gateLabel: qsTr("Use custom native library settings for this instance")
            keys: ["UseNativeGLFW", "UseNativeOpenAL"]
            SettingSwitch {
                source: store
                key: "UseNativeGLFW"
                enabled: workarounds.overriding
                label: qsTr("Use the system's GLFW")
                description: qsTr("Only for systems where the bundled libraries don't work, such as some Linux setups.")
            }
            SettingSwitch {
                source: store
                key: "UseNativeOpenAL"
                enabled: workarounds.overriding
                label: qsTr("Use the system's OpenAL")
            }
        }

        SettingsGroup {
            width: parent.width
            title: qsTr("Data packs")
            description: qsTr("A folder of data packs added to every world. Needs a loader mod such as Paxi or OpenLoader.")
            SettingSwitch {
                id: globalPacks
                source: store
                key: "GlobalDataPacksEnabled"
                label: qsTr("Use a shared data pack folder")
            }
            SettingText {
                source: store
                key: "GlobalDataPacksPath"
                enabled: globalPacks.checked
                label: qsTr("Folder, inside the game folder")
                placeholder: "datapacks"
            }
        }

        PluginSurfaces {
            width: parent.width
            model: root.pluginSurfaces
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
