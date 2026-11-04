// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Above the Log tab's live log after a crash: one line, "The game crashed: <first cause>", with the
 * main fix, Details and ⋯. Details opens the rest under it (CrashDoctorBody): the explanation, the
 * mods it names, the log line, the other possible causes. It stays until the next launch or until
 * ⋯ > Hide this. The window that opens right after a crash (CrashDoctorDialog) says the same.
 * Everything here comes from this computer; the log upload and sending the crash to Kite are the
 * only things that leave it, and only when asked for. When Kite could not tell, the button sends the
 * crash to Kite (Details shows what goes). When rules that came later know an old crash, the line
 * says "Fix found" with its fix (CrashFollowUp).
 */
Item {
    id: root

    // CrashDoctorModel, or null.
    property var doctor: null
    // CrashDoctorFlows that runs the fixes.
    property var flows: null
    property bool expanded: false
    property alias othersShown: body.othersShown
    signal showLine(string line)

    readonly property bool busy: !!root.doctor && root.doctor.busy
    readonly property bool shown: !!root.doctor && (root.doctor.hasReport || root.busy)
    readonly property var topCause: root.doctor && root.doctor.causes.length > 0 ? root.doctor.causes[0] : null
    readonly property bool hasFix: !!root.topCause && root.topCause.fixes.length > 0
    readonly property bool working: !!root.flows && root.flows.workingCause === 0
    readonly property bool canSend: !!root.doctor && root.doctor.canSend
    readonly property bool solved: !!root.doctor && root.doctor.solvedLater
    // Nothing found: the button sends the crash to Kite, or shares the log without a server.
    readonly property bool sendIsButton: !root.topCause && root.canSend

    visible: root.shown
    implicitHeight: root.shown ? strip.implicitHeight : 0

    // A neutral surface like the rest of the page; only the icon carries the warning colour.
    Rectangle {
        id: strip
        anchors.left: parent.left
        anchors.right: parent.right
        implicitHeight: column.implicitHeight + Theme.space.sm * 2
        radius: Theme.radius.lg
        color: Theme.palette.surface
        border.width: 1
        border.color: Theme.palette.border

        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.topMargin: Theme.space.sm
            anchors.bottomMargin: Theme.space.sm
            anchors.leftMargin: Theme.space.md
            anchors.rightMargin: Theme.space.sm
            spacing: Theme.space.md

            RowLayout {
                Layout.fillWidth: true
                Layout.minimumHeight: Theme.control.height
                spacing: Theme.space.md

                BusyIndicator {
                    visible: root.busy
                    Layout.preferredWidth: Theme.icon.sm
                    Layout.preferredHeight: Theme.icon.sm
                    running: root.busy
                }
                MeshIcon {
                    visible: !root.busy
                    iconName: root.solved ? "check" : "alert-triangle"
                    size: Theme.icon.sm
                    color: root.solved ? Theme.palette.accent : Theme.palette.warning
                }
                Copy {
                    Layout.fillWidth: true
                    text: root.busy ? qsTr("Reading why the game crashed…")
                          : root.solved ? qsTr("Fix found: %1").arg(root.topCause.title)
                          : root.topCause ? qsTr("The game crashed: %1").arg(root.topCause.title)
                          : qsTr("The game crashed; Kite couldn't find the cause")
                    textFormat: Text.PlainText
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }
                // Details open holds Send itself, with what goes; one Send on screen.
                Button {
                    visible: !root.busy && (root.hasFix || !root.topCause) && !(root.sendIsButton && root.expanded)
                    enabled: !root.working
                    text: root.working && root.flows.workingText.length > 0 ? root.flows.workingText
                          : root.hasFix ? root.topCause.fixes[0].label
                          : root.sendIsButton ? qsTr("Send to Kite")
                          : qsTr("Share the log")
                    onClicked: root.hasFix ? root.flows.run(0, 0)
                               : root.sendIsButton ? root.doctor.send(false)
                               : root.flows.upload()
                }
                // The same word open or closed, so nothing moves; the chevron says which.
                Button {
                    visible: !root.busy
                    flat: true
                    text: qsTr("Details")
                    icon.source: Icons.url(root.expanded ? "chevron-down" : "chevron-right")
                    onClicked: root.expanded = !root.expanded
                }
                IconButton {
                    id: moreButton
                    visible: !root.busy
                    flat: false
                    iconName: "more"
                    tip: qsTr("More")
                    onClicked: menu.popup(moreButton, moreButton.width - menu.width, moreButton.height)

                    CrashCauseMenu {
                        id: menu
                        cause: root.topCause
                        working: root.working
                        // "Share the log" is the button itself when nothing was found and there is
                        // nowhere to send the crash.
                        canUpload: root.hasFix || root.sendIsButton
                        uploadInMenu: root.sendIsButton
                        canSend: root.hasFix && root.canSend
                        hasCrashReport: !!root.doctor && root.doctor.crashReport.length > 0
                        canUndo: !!root.doctor && root.doctor.undoAction.length > 0
                        onFixRequested: (fixIndex) => root.flows.run(0, fixIndex)
                        onUploadRequested: root.flows.upload()
                        onSendRequested: root.doctor.send(false)
                        onCrashReportRequested: root.flows.openCrashReport()
                        onUndoRequested: root.flows.undo()
                        onDismissRequested: root.doctor.dismiss()
                    }
                }
            }

            CrashDoctorBody {
                id: body
                Layout.fillWidth: true
                Layout.leftMargin: Theme.icon.sm + Theme.space.md
                Layout.rightMargin: Theme.space.sm
                Layout.bottomMargin: Theme.space.sm
                visible: root.expanded && !root.busy
                firstDetailsOnly: true
                doctor: root.doctor
                flows: root.flows
                onShowLine: (line) => root.showLine(line)
            }
        }
    }
}
