// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick

/*
 * The "Custom commands" section of SettingsPage.qml: shell commands run
 * around every launch, launcher-wide. Each instance can override them on
 * its own Settings tab.
 */
SettingsScroll {
    title: qsTr("Custom commands")
    description: qsTr("Shell commands that run for every instance. Each instance can replace them in its own settings.")
    SettingsGroup {
        width: parent.width
        description: qsTr("You can use $INST_NAME, $INST_ID, $INST_DIR, $INST_MC_DIR, $INST_JAVA and $INST_JAVA_ARGS in them.")
        SettingText {
            key: "PreLaunchCommand"
            label: qsTr("Before launch")
            monospace: true
        }
        SettingText {
            key: "WrapperCommand"
            label: qsTr("Wrapper")
            description: qsTr("Runs the game through another program, such as prime-run or gamemoderun.")
            monospace: true
        }
        SettingText {
            key: "PostExitCommand"
            label: qsTr("After exit")
            monospace: true
        }
    }
}
