// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * The "Minecraft" section of SettingsPage.qml: the game window, play time
 * tracking, and native library overrides, launcher-wide.
 */
SettingsScroll {
    title: qsTr("Minecraft")
    description: qsTr("The game window, play time tracking, and native library overrides.")
    SettingsGroup {
        width: parent.width
        title: qsTr("Game window")
        SettingSwitch {
            id: maximized
            key: "LaunchMaximized"
            label: qsTr("Start maximized")
        }
        SettingNumber {
            key: "MinecraftWinWidth"
            enabled: !maximized.checked
            label: qsTr("Window width")
            from: 1
            to: 65536
            suffix: qsTr("px")
        }
        SettingNumber {
            key: "MinecraftWinHeight"
            enabled: !maximized.checked
            label: qsTr("Window height")
            from: 1
            to: 65536
            suffix: qsTr("px")
        }
    }
    SettingsGroup {
        width: parent.width
        title: qsTr("Play time")
        SettingSwitch {
            key: "RecordGameTime"
            label: qsTr("Record play time")
        }
        SettingSwitch {
            key: "ShowGameTime"
            label: qsTr("Show play time per instance")
        }
        SettingSwitch {
            key: "ShowGlobalGameTime"
            label: qsTr("Show total play time")
        }
    }
    SettingsGroup {
        width: parent.width
        title: qsTr("Native libraries")
        description: qsTr("Only for systems where the bundled libraries don't work, such as some Linux setups.")
        SettingSwitch {
            key: "UseNativeGLFW"
            label: qsTr("Use the system's GLFW")
        }
        SettingSwitch {
            key: "UseNativeOpenAL"
            label: qsTr("Use the system's OpenAL")
        }
    }
}
