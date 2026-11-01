// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Add an empty component to an instance: a name and an id. The component starts
 * with no libraries or arguments; the user fills it in by editing its JSON. The
 * id must be new to this instance; empty uses one made from the name.
 */
AppDialog {
    id: root

    property var tools: null

    readonly property string suggestedUid: root.tools ? root.tools.suggestUid(nameField.text) : ""
    readonly property string problem: root.tools
                                      ? root.tools.validateNewComponent(nameField.text, uidField.text, root.tools.componentUids())
                                      : ""
    // Shown only once the user has typed something, not on an untouched form.
    readonly property bool touched: nameField.text.length > 0 || uidField.text.length > 0

    size: "sm"
    closeOnPrimary: false
    title: qsTr("Add empty component")
    primaryText: qsTr("Add")
    primaryEnabled: root.problem.length === 0
    onPrimaryClicked: root.add()

    onOpened: {
        nameField.text = ""
        uidField.text = ""
        nameField.forceActiveFocus()
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Copy {
            Layout.fillWidth: true
            text: qsTr("An empty component has no files of its own. Add its libraries and arguments by editing its JSON afterwards.")
            role: "label"
            tone: "tertiary"
            wrapMode: Text.Wrap
            lineHeight: Theme.type.label.lineHeight
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space.xs
            FieldLabel { text: qsTr("Name") }
            TextField {
                id: nameField
                Layout.fillWidth: true
                selectByMouse: true
                placeholderText: qsTr("My component")
                Accessible.name: qsTr("Name")
                onAccepted: if (root.problem.length === 0) root.add()
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space.xs
            FieldLabel { text: qsTr("ID") }
            TextField {
                id: uidField
                Layout.fillWidth: true
                selectByMouse: true
                font.family: Theme.font.mono
                placeholderText: root.suggestedUid.length > 0 ? root.suggestedUid : "org.example.mycomponent"
                Accessible.name: qsTr("ID")
                onAccepted: if (root.problem.length === 0) root.add()
            }
        }

        Copy {
            Layout.fillWidth: true
            visible: root.touched && root.problem.length > 0
            text: root.problem
            role: "label"
            tone: "danger"
            wrapMode: Text.Wrap
        }
    }

    function add() {
        var error = root.tools.addEmpty(nameField.text, uidField.text)
        if (error.length === 0)
            root.close()
    }
}
