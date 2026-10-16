// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The "About" section of SettingsPage.qml: which MeshMC this is, where to
 * get help, the credits and the licences -- the widget About dialog plus
 * the Help menu's links, all from QmlShell.about (AboutInfo, read from the
 * build itself). The licence text is long, so it only loads on request.
 */
SettingsScroll {
    id: root

    // QmlShell.about (AboutInfo), or null.
    property var about: null
    // Read by SettingsPage.qml's section list.
    readonly property bool shown: !!root.about
    property bool licencesOpen: false

    // One build fact: a label and its value, selectable so it can be
    // copied into a bug report.
    component FactRow: SettingRow {
        id: fact
        property string value
        property bool monospace: false
        visible: fact.value.length > 0
        TextEdit {
            text: fact.value
            readOnly: true
            selectByMouse: true
            color: Theme.palette.textSecondary
            selectionColor: Theme.palette.accentSubtle
            font.family: fact.monospace ? Theme.font.mono : Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
        }
    }

    title: qsTr("About")
    description: qsTr("The version you are running, where to get help, and the people and licences behind MeshMC.")

    SettingsGroup {
        width: parent.width
        title: root.about ? root.about.name : ""
        FactRow { label: qsTr("Version"); value: root.about ? root.about.version : "" }
        FactRow { label: qsTr("Platform"); value: root.about ? root.about.platform : "" }
        FactRow { label: qsTr("Built"); value: root.about ? root.about.buildDate : "" }
        FactRow { label: qsTr("Commit"); value: root.about ? root.about.commit : ""; monospace: true }
        FactRow { label: qsTr("Version channel"); value: root.about ? root.about.channel : "" }
        FactRow { label: qsTr("Update channel"); value: root.about ? root.about.updateChannel : "" }
        SettingRow {
            visible: !!root.about && root.about.sourceUrl.length > 0
            label: qsTr("Source code")
            description: root.about ? root.about.sourceUrl : ""
            IconButton {
                iconName: "external-link"
                tip: qsTr("Open in browser")
                flat: false
                onClicked: Qt.openUrlExternally(root.about.sourceUrl)
            }
        }
    }

    SettingsGroup {
        width: parent.width
        visible: helpRepeater.count > 0
        title: qsTr("Help")
        Repeater {
            id: helpRepeater
            model: root.about ? root.about.helpLinks : []
            delegate: SettingRow {
                required property var modelData
                label: modelData.label
                description: modelData.url
                IconButton {
                    iconName: "external-link"
                    tip: qsTr("Open in browser")
                    flat: false
                    onClicked: Qt.openUrlExternally(modelData.url)
                }
            }
        }
    }

    SettingsGroup {
        width: parent.width
        visible: credits.text.length > 0
        title: qsTr("Credits")
        Text {
            id: credits
            width: parent.width
            padding: Theme.space.lg
            // linkColor does not reach RichText; a style rule does.
            text: root.about && root.about.creditsHtml.length > 0
                  ? "<style>a { color: " + Theme.palette.accent + "; text-decoration: none; }</style>"
                    + root.about.creditsHtml : ""
            textFormat: Text.RichText
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            linkColor: Theme.palette.accent
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            onLinkActivated: (link) => Qt.openUrlExternally(link)
        }
    }

    SettingsGroup {
        width: parent.width
        visible: !!root.about && root.about.licenseText.length > 0
        title: qsTr("Licences")
        description: root.about ? root.about.copyright : ""
        SettingRow {
            label: qsTr("MeshMC and the libraries it uses")
            description: qsTr("The full text of every licence.")
            Button {
                text: root.licencesOpen ? qsTr("Hide") : qsTr("Show")
                onClicked: root.licencesOpen = !root.licencesOpen
            }
        }
        Loader {
            width: parent.width
            active: root.licencesOpen
            visible: active
            sourceComponent: Text {
                padding: Theme.space.lg
                text: root.about ? root.about.licenseText : ""
                textFormat: Text.MarkdownText
                wrapMode: Text.Wrap
                color: Theme.palette.textSecondary
                linkColor: Theme.palette.accent
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                onLinkActivated: (link) => Qt.openUrlExternally(link)
            }
        }
    }
}
