// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * "Set up Kite": a checklist of what a new player still has to do. In a side panel it is a titled box: a done step is
 * struck through and quiet, a step still to do is a button. Across a page (`wide`) with one or two steps left it is
 * one slim row instead: each step's ring and text with its action at the end; with more left, the box with its steps
 * in columns. The host hides it once every step is done.
 */
Rectangle {
    id: root

    property bool hasInstances: false
    property bool signedIn: false
    // No fill or hairline, for a host that is already a panel.
    property bool flat: false

    // Spans a page's width rather than a side panel's.
    property bool wide: false

    signal signInRequested()

    // `action` names the step's button in the slim row and what act() does.
    readonly property var steps: [
        { key: "signin", text: qsTr("Sign in with Microsoft"), action: qsTr("Sign in"), done: root.signedIn }
    ]
    readonly property var pending: root.steps.filter((step) => !step.done)
    readonly property int slimMaxSteps: 2
    readonly property int stepColumnMinWidth: 280
    readonly property bool slim: root.wide && root.pending.length <= root.slimMaxSteps
    readonly property int stepColumns: root.wide ? Math.max(1, Math.min(root.steps.length,
                                                     Math.floor(column.width / root.stepColumnMinWidth))) : 1
    readonly property bool allDone: root.pending.length === 0

    function act(key) {
        if (key === "signin")
            root.signInRequested()
    }
    // An empty library has the page's own "Create instance"; the checklist waits for a first instance.
    readonly property bool shown: root.hasInstances && !root.allDone

    implicitHeight: root.slim ? slimRow.implicitHeight + Theme.space.sm * 2
                              : column.implicitHeight + Theme.space.md * 2
    radius: Theme.radius.lg
    color: root.flat ? "transparent" : Theme.palette.surface
    border.width: root.flat ? 0 : 1
    border.color: Theme.palette.border

    component Ring: Rectangle {
        id: ringRoot
        property bool done: false
        width: Theme.icon.md
        height: width
        radius: width / 2
        color: ringRoot.done ? Theme.palette.textTertiary : "transparent"
        border.width: ringRoot.done ? 0 : 2
        border.color: Theme.palette.textSecondary

        MeshIcon {
            anchors.centerIn: parent
            visible: ringRoot.done
            iconName: "check"
            size: Theme.icon.xxs
            color: Theme.palette.surface
        }
    }

    component Step: AbstractButton {
        id: step
        property bool done: false

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
            Ring {
                id: ring
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                done: step.done
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
        visible: !root.slim

        Copy {
            width: parent.width
            height: Theme.control.heightSm
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Set up Kite")
            role: "title"
            elide: Text.ElideRight
        }
        Grid {
            width: parent.width
            columns: root.stepColumns
            columnSpacing: Theme.space.sm
            rowSpacing: Theme.space.xs

            Repeater {
                model: root.steps
                delegate: Step {
                    required property var modelData
                    width: Math.floor((column.width - Theme.space.sm * (root.stepColumns - 1)) / root.stepColumns)
                    text: modelData.text
                    done: modelData.done
                    onClicked: root.act(modelData.key)
                }
            }
        }
    }

    // The slim row: the steps still to do share the width, each with its action at its end.
    Row {
        id: slimRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.space.lg
        anchors.rightMargin: Theme.space.sm
        spacing: Theme.space.xl
        visible: root.slim

        Repeater {
            model: root.slim ? root.pending : []
            delegate: Item {
                id: slimStep
                required property var modelData
                width: Math.floor((slimRow.width - slimRow.spacing * (root.pending.length - 1)) / root.pending.length)
                implicitHeight: Theme.control.height

                Ring {
                    id: slimRing
                    anchors.verticalCenter: parent.verticalCenter
                }
                Copy {
                    anchors.left: slimRing.right
                    anchors.leftMargin: Theme.space.md
                    anchors.right: slimAction.left
                    anchors.rightMargin: Theme.space.md
                    anchors.verticalCenter: parent.verticalCenter
                    text: slimStep.modelData.text
                    elide: Text.ElideRight
                    font.weight: Font.Medium
                }
                Button {
                    id: slimAction
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    buttonSize: "sm"
                    text: slimStep.modelData.action
                    onClicked: root.act(slimStep.modelData.key)
                }
            }
        }
    }
}
