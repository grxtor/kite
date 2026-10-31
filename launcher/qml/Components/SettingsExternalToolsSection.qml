// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Profilers, MCEdit and the editor used for JSON/text files. Each path is picked
 * from a native dialog (SettingPathField.qml); JProfiler, JVisualVM and MCEdit
 * get a Check button.
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

    // MCEdit is unmaintained: shown only to those who already set a path.
    SettingsGroup {
        width: parent.width
        visible: SettingsStore.string("MCEditPath").length > 0
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
