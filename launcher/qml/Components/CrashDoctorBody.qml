// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * What the crash doctor says about one crash, without a surface: "reading..." while it reads, the
 * first cause with its fix, the other possible causes folded under one row, or, when it found
 * nothing, that it could not tell and the log upload. Under the Log tab's strip, which says the first
 * cause's title and holds its buttons, the first cause shows only its details (firstDetailsOnly).
 * Last, the offer to send the crash to Kite when nothing knew it or its fix did not help.
 */
ColumnLayout {
    id: root

    // CrashDoctorModel, or null.
    property var doctor: null
    // CrashDoctorFlows that runs the fixes.
    property var flows: null
    property bool othersShown: false
    property bool firstDetailsOnly: false
    signal showLine(string line)

    readonly property var causes: root.doctor ? root.doctor.causes : []
    readonly property bool busy: !!root.doctor && root.doctor.busy
    readonly property bool hasCrashReport: !!root.doctor && root.doctor.crashReport.length > 0
    readonly property bool canUndo: !!root.doctor && root.doctor.undoAction.length > 0
    readonly property int otherCount: Math.max(0, root.causes.length - 1)
    readonly property string otherText: root.otherCount === 1 ? qsTr("1 more possible cause")
                                        : qsTr("%1 more possible causes").arg(root.otherCount)
    readonly property var unknownCause: Format.unknownCrashCause(root.doctor)

    spacing: Theme.space.md

    RowLayout {
        Layout.fillWidth: true
        visible: root.busy
        spacing: Theme.space.md
        BusyIndicator {
            Layout.preferredWidth: Theme.icon.md
            Layout.preferredHeight: Theme.icon.md
            running: root.busy
        }
        Copy {
            Layout.fillWidth: true
            text: qsTr("Reading why the game crashed…")
            tone: "secondary"
        }
    }

    Repeater {
        // The first cause, or the honest "couldn't tell".
        model: root.busy ? 0 : 1
        delegate: CrashCauseView {
            Layout.fillWidth: true
            readonly property bool known: root.causes.length > 0
            cause: known ? root.causes[0] : root.unknownCause
            main: true
            showTitle: !root.firstDetailsOnly
            showActions: !root.firstDetailsOnly
            working: known && !!root.flows && root.flows.workingCause === 0
            workingText: root.flows ? root.flows.workingText : ""
            hasCrashReport: root.hasCrashReport
            canUndo: root.canUndo
            onFixRequested: (fixIndex) => root.flows.run(0, fixIndex)
            onShowLine: (line) => root.showLine(line)
            onUploadRequested: root.flows.upload()
            onCrashReportRequested: root.flows.openCrashReport()
            onDismissRequested: root.doctor.dismiss()
            onUndoRequested: root.flows.undo()
        }
    }

    Rectangle {
        Layout.fillWidth: true
        visible: !root.busy && root.otherCount > 0
        implicitHeight: 1
        color: Theme.palette.border
    }

    AbstractButton {
        id: toggle
        visible: !root.busy && root.otherCount > 0
        hoverEnabled: true
        implicitHeight: Theme.control.heightSm
        implicitWidth: toggleRow.implicitWidth
        onClicked: root.othersShown = !root.othersShown
        contentItem: Item {}
        Accessible.role: Accessible.Button
        Accessible.name: root.otherText

        Row {
            id: toggleRow
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.xs
            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                iconName: root.othersShown ? "chevron-down" : "chevron-right"
                size: Theme.icon.xs
                color: toggle.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
            }
            Copy {
                anchors.verticalCenter: parent.verticalCenter
                text: root.otherText
                role: "label"
                color: toggle.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
            }
        }
    }

    Repeater {
        model: !root.busy && root.othersShown ? root.otherCount : 0
        delegate: CrashCauseView {
            required property int index
            Layout.fillWidth: true
            cause: root.causes[index + 1]
            main: false
            working: !!root.flows && root.flows.workingCause === index + 1
            workingText: root.flows ? root.flows.workingText : ""
            hasCrashReport: root.hasCrashReport
            canUndo: root.canUndo
            onFixRequested: (fixIndex) => root.flows.run(index + 1, fixIndex)
            onShowLine: (line) => root.showLine(line)
            onUploadRequested: root.flows.upload()
            onCrashReportRequested: root.flows.openCrashReport()
            onDismissRequested: root.doctor.dismiss()
            onUndoRequested: root.flows.undo()
        }
    }

    Rectangle {
        Layout.fillWidth: true
        visible: sendBlock.visible
        implicitHeight: 1
        color: Theme.palette.border
    }
    CrashSendBlock {
        id: sendBlock
        Layout.fillWidth: true
        visible: !root.busy && sendBlock.shown
        doctor: root.doctor
    }
}
