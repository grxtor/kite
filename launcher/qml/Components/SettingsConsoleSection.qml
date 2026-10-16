// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * The "Console" section of SettingsPage.qml: what the game's log window
 * does while you play, and how much of the log is kept.
 */
SettingsScroll {
    title: qsTr("Console")
    description: qsTr("What the game's log window does while you play, and how much of it is kept.")
    SettingsGroup {
        width: parent.width
        title: qsTr("Game console")
        SettingSwitch {
            key: "ShowConsole"
            label: qsTr("Show the console while playing")
        }
        SettingSwitch {
            key: "AutoCloseConsole"
            label: qsTr("Close the console when the game quits")
        }
        SettingSwitch {
            key: "ShowConsoleOnError"
            label: qsTr("Show the console when the game crashes")
        }
    }
    SettingsGroup {
        width: parent.width
        title: qsTr("Font")
        ConsoleFontRow {
            label: qsTr("Font")
            description: qsTr("Used for the game's log. A monospaced font lines columns up best.")
        }
        SettingNumber {
            key: "ConsoleFontSize"
            label: qsTr("Font size")
            from: 6
            to: 48
            suffix: qsTr("pt")
        }
        ConsolePreview {}
    }
    SettingsGroup {
        width: parent.width
        title: qsTr("Log history")
        SettingNumber {
            key: "ConsoleMaxLines"
            label: qsTr("Lines to keep")
            from: 10000
            to: 1000000
            stepSize: 10000
        }
        SettingSwitch {
            key: "ConsoleOverflowStop"
            label: qsTr("Stop logging when the limit is reached")
            description: qsTr("Otherwise the oldest lines are dropped to make room.")
        }
    }
}
