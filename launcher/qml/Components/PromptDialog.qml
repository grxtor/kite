// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * Asks for one line of text: a new name, a group. Enter confirms, Escape
 * cancels; `suggestions` become one-click chips under the field (existing
 * groups, say), so the common answers need no typing.
 */
Dialog {
    id: root

    property string label
    property string value
    property string placeholder
    property string confirmText: qsTr("Save")
    property bool allowEmpty: false
    property var suggestions: []
    property string error
    signal submitted(string text)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(440, parent ? parent.width - Theme.space.xxl * 2 : 440)
    modal: true

    onOpened: {
        field.text = root.value
        root.error = ""
        field.forceActiveFocus()
        field.selectAll()
    }

    function submit() {
        var text = field.text.trim()
        if (!root.allowEmpty && text.length === 0)
            return
        root.submitted(text)
    }

    contentItem: Column {
        spacing: Theme.space.sm

        Text {
            visible: root.label.length > 0
            width: parent.width
            text: root.label
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
        }

        TextField {
            id: field
            width: parent.width
            placeholderText: root.placeholder
            selectByMouse: true
            onAccepted: root.submit()
        }

        Flow {
            width: parent.width
            spacing: Theme.space.xs
            visible: root.suggestions.length > 0
            Repeater {
                model: root.suggestions
                delegate: AbstractButton {
                    id: chip
                    required property string modelData
                    hoverEnabled: true
                    implicitHeight: Theme.control.heightSm
                    implicitWidth: chipText.implicitWidth + Theme.space.md * 2
                    onClicked: field.text = modelData
                    background: Rectangle {
                        radius: height / 2
                        color: chip.hovered ? Theme.palette.surfaceOverlay : Theme.palette.surfaceRaised
                        border.width: 1
                        border.color: field.text === chip.modelData ? Theme.palette.accent : Theme.palette.border
                    }
                    contentItem: Text {
                        id: chipText
                        text: chip.modelData.length > 0 ? chip.modelData : qsTr("No group")
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                }
            }
        }

        Text {
            visible: root.error.length > 0
            width: parent.width
            text: root.error
            wrapMode: Text.Wrap
            color: Theme.palette.danger
            font.family: Theme.font.family
            font.pixelSize: Theme.type.caption.pixelSize
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.lg
        topPadding: 0
        Button {
            text: root.confirmText
            highlighted: true
            enabled: root.allowEmpty || field.text.trim().length > 0
            onClicked: root.submit()
        }
        Button {
            text: qsTr("Cancel")
            flat: true
            onClicked: root.close()
        }
    }
}
