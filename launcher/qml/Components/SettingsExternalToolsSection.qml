// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * Profilers, MCEdit, and the editor used for JSON/text files -- the QML
 * equivalent of the classic ExternalToolsPage. Each path is picked from a
 * native dialog (see SettingPathField.qml) rather than typed by hand, and
 * JProfiler/JVisualVM/MCEdit each get a Check button next to their path,
 * same as the classic page.
 */
SettingsScroll {
    title: qsTr("External tools")
    description: qsTr("Profilers, MCEdit, and the editor used for JSON and text files.")

    SettingsGroup {
        width: parent.width
        title: qsTr("JProfiler")
        description: qsTr("https://www.ej-technologies.com/products/jprofiler/overview.html")
        SettingPathField {
            key: "JProfilerPath"
            label: qsTr("Install folder")
            folder: true
            toolId: "jprofiler"
        }
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("JVisualVM")
        description: qsTr("https://visualvm.github.io/")
        SettingPathField {
            key: "JVisualVMPath"
            label: qsTr("Executable")
            toolId: "jvisualvm"
        }
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("MCEdit")
        description: qsTr("https://www.mcedit.net/")
        SettingPathField {
            key: "MCEditPath"
            label: qsTr("Install folder")
            folder: true
            toolId: "mcedit"
        }
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("Editors")
        description: qsTr("Leave empty to use the system default.")
        SettingPathField {
            key: "JsonEditor"
            label: qsTr("Text editor")
            placeholder: qsTr("Automatic")
        }
    }
}
