// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * "Set up Kite" on Home's side panel: a checklist of what a new player still has to do. A done step is struck
 * through and quiet; a step still to do is a button. The page hides the card once every step is done.
 */
Rectangle {
    id: root

    property bool hasInstances: false
    property bool signedIn: false
    // No fill or hairline, for a host that is already a panel.
    property bool flat: false

    signal signInRequested()

    readonly property bool allDone: root.signedIn
    // An empty library has the page's own "Create instance"; the checklist waits for a first instance.
    readonly property bool shown: root.hasInstances && !root.allDone

    implicitHeight: column.implicitHeight + Theme.space.md * 2
    radius: Theme.radius.lg
    color: root.flat ? "transparent" : Theme.palette.surface
    border.width: root.flat ? 0 : 1
    border.color: Theme.palette.border

    component Step: AbstractButton {
        id: step
        property bool done: false
        readonly property int indicatorSize: Theme.icon.md

        enabled: !step.done
        hoverEnabled: true
        implicitHeight: Theme.control.height
        leftPadding: Theme.space.md
        rightPadding: Theme.space.md
        Accessible.name: step.text
        Accessible.description: step.done ? qsTr("Done") : ""

        background: StateFill {
            hovered: step.hovered
            down: step.down
            radius: Theme.radius.md
            border.width: step.visualFocus ? Theme.focus.width : 0
            border.color: Theme.palette.focusRing
        }

        contentItem: Item {
            Rectangle {
                id: ring
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: step.indicatorSize
                height: width
                radius: width / 2
                color: step.done ? Theme.palette.textTertiary : "transparent"
                border.width: step.done ? 0 : 2
                border.color: Theme.palette.textSecondary

                MeshIcon {
                    anchors.centerIn: parent
                    visible: step.done
                    iconName: "check"
                    size: Theme.icon.xxs
                    color: Theme.palette.surface
                }
            }

            Copy {
                anchors.left: ring.right
                anchors.leftMargin: Theme.space.sm
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: step.text
                tone: step.done ? "tertiary" : "primary"
                elide: Text.ElideRight
                font.strikeout: step.done
                font.weight: Font.Medium
            }
        }
    }

    Column {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.space.md
        spacing: Theme.space.sm

        Copy {
            width: parent.width
            height: Theme.control.heightSm
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Set up Kite")
            role: "title"
            elide: Text.ElideRight
        }

        Step {
            width: parent.width
            text: qsTr("Sign in with Microsoft")
            done: root.signedIn
            onClicked: root.signInRequested()
        }
    }
}
