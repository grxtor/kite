// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Asks for one line of text: a new name, a group. Enter confirms, Escape
 * cancels; `suggestions` become one-click chips under the field (existing
 * groups, say), so the common answers need no typing.
 */
AppDialog {
    id: root

    property string label
    property string value
    property string placeholder
    property string confirmText: qsTr("Save")
    property bool allowEmpty: false
    property var suggestions: []
    property string error
    signal submitted(string text)

    size: "sm"
    closeOnPrimary: false
    primaryText: root.confirmText
    primaryEnabled: root.allowEmpty || field.text.trim().length > 0
    onPrimaryClicked: root.submit()

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

        Copy {
            visible: root.label.length > 0
            width: parent.width
            text: root.label
            role: "label"
            tone: "secondary"
            wrapMode: Text.Wrap
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
                        radius: Theme.radius.sm
                        color: chip.hovered ? Theme.palette.surfaceOverlay : Theme.palette.surfaceRaised
                        border.width: 1
                        border.color: field.text === chip.modelData ? Theme.palette.accent : Theme.palette.border
                    }
                    contentItem: Copy {
                        id: chipText
                        text: chip.modelData.length > 0 ? chip.modelData : qsTr("No group")
                        role: "label"
                        tone: "secondary"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        Copy {
            visible: root.error.length > 0
            width: parent.width
            text: root.error
            role: "caption"
            tone: "danger"
            wrapMode: Text.Wrap
        }
    }
}
