// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * "Set up Kite" on Home's side panel: the first two things a new player
 * does, as a checklist. A step already done is
 * struck through and quiet; a step still to do is a button that does it,
 * lighting up under the pointer. The page hides the card once both are
 * done.
 */
Rectangle {
    id: root

    // The library has an instance.
    property bool hasInstances: false
    // A Microsoft account is signed in.
    property bool signedIn: false
    // No fill or hairline of its own, for a host that is already a panel
    // (Sade's and Gece's side panels): a card in it would be a box in a box.
    property bool flat: false

    signal createRequested()
    signal signInRequested()

    readonly property bool allDone: root.hasInstances && root.signedIn
    // An empty library has the page's own "Create instance" in front of it;
    // the checklist comes once there is something to check off.
    readonly property bool shown: root.hasInstances && !root.allDone

    implicitHeight: column.implicitHeight + Theme.space.md * 2
    radius: Theme.radius.lg
    color: root.flat ? "transparent" : Theme.palette.surface
    border.width: root.flat ? 0 : 1
    border.color: Theme.palette.border

    /*
     * One step: a hollow ring and the words, or -- done -- a filled ring
     * with a check and the words struck through. Only a step still to do
     * can be pressed.
     */
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

        background: Rectangle {
            radius: Theme.radius.md
            color: step.down ? Theme.palette.pressedOverlay
                 : step.hovered ? Theme.palette.hoverOverlay : "transparent"
            border.width: step.visualFocus ? Theme.focus.width : 0
            border.color: Theme.palette.focusRing
            Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
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
                    size: Theme.icon.sm - 4
                    color: Theme.palette.surface
                }
            }

            Text {
                anchors.left: ring.right
                anchors.leftMargin: Theme.space.sm
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: step.text
                elide: Text.ElideRight
                font.strikeout: step.done
                color: step.done ? Theme.palette.textTertiary : Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
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

        Text {
            width: parent.width
            height: Theme.control.heightSm
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Set up Kite")
            elide: Text.ElideRight
            color: Theme.palette.textPrimary
            font.family: Theme.font.heading
            font.pixelSize: Theme.type.title.pixelSize
            font.weight: Theme.type.title.weight
        }

        Step {
            width: parent.width
            text: qsTr("Create your first instance")
            done: root.hasInstances
            onClicked: root.createRequested()
        }
        Step {
            width: parent.width
            text: qsTr("Sign in with Microsoft")
            done: root.signedIn
            onClicked: root.signInRequested()
        }
    }
}
