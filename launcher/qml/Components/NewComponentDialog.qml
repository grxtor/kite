// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Add an empty component to an instance (the old NewComponentDialog): a
 * name and an id. The component starts with no libraries or arguments;
 * the user fills it in by editing its JSON afterwards (⋯ > Edit JSON).
 * The id is what the launcher knows it by, so it must be new to this
 * instance; leaving it empty uses one made from the name.
 */
Dialog {
    id: root

    // ComponentTools (InstanceDetails.componentTools).
    property var tools: null

    readonly property string suggestedUid: root.tools ? root.tools.suggestUid(nameField.text) : ""
    readonly property string problem: root.tools
                                      ? root.tools.validateNewComponent(nameField.text, uidField.text, root.tools.componentUids())
                                      : ""
    // Shown only once the user has typed something, not on an untouched form.
    readonly property bool touched: nameField.text.length > 0 || uidField.text.length > 0

    readonly property int dialogWidth: 480

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(root.dialogWidth, parent ? parent.width - Theme.space.xxl * 2 : root.dialogWidth)
    modal: true
    title: qsTr("Add empty component")

    header: DialogHeader {
        title: root.title
        icon: "layers"
    }

    onOpened: {
        nameField.text = ""
        uidField.text = ""
        nameField.forceActiveFocus()
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Text {
            Layout.fillWidth: true
            text: qsTr("An empty component has no files of its own. Add its libraries and arguments by editing its JSON afterwards.")
            wrapMode: Text.Wrap
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            lineHeight: Theme.type.label.lineHeight
        }

        // Each label over its field, as in the other dialogs' forms.
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

        Text {
            Layout.fillWidth: true
            visible: root.touched && root.problem.length > 0
            text: root.problem
            wrapMode: Text.Wrap
            color: Theme.palette.danger
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.lg
        topPadding: 0

        Button {
            text: qsTr("Add")
            highlighted: true
            enabled: root.problem.length === 0
            onClicked: root.add()
        }
        Button {
            text: qsTr("Cancel")
            flat: true
            onClicked: root.close()
        }
    }

    function add() {
        var error = root.tools.addEmpty(nameField.text, uidField.text)
        if (error.length === 0)
            root.close()
    }
}
