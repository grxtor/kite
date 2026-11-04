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
 * Everything here comes from this computer; the log upload is the only thing that leaves it, and
 * asks first.
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
                    iconName: "alert-triangle"
                    size: Theme.icon.sm
                    color: Theme.palette.warning
                }
                Copy {
                    Layout.fillWidth: true
                    text: root.busy ? qsTr("Reading why the game crashed…")
                          : root.topCause ? qsTr("The game crashed: %1").arg(root.topCause.title)
                          : qsTr("The game crashed; Kite couldn't find the cause")
                    textFormat: Text.PlainText
                    font.weight: Font.Medium
                    elide: Text.ElideRight
                }
                Button {
                    visible: !root.busy && (root.hasFix || !root.topCause)
                    enabled: !root.working
                    text: root.working && root.flows.workingText.length > 0 ? root.flows.workingText
                          : root.hasFix ? root.topCause.fixes[0].label
                          : qsTr("Share the log")
                    onClicked: root.hasFix ? root.flows.run(0, 0) : root.flows.upload()
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
                        // "Upload the log" is the button itself when nothing was found.
                        canUpload: root.hasFix
                        hasCrashReport: !!root.doctor && root.doctor.crashReport.length > 0
                        canUndo: !!root.doctor && root.doctor.undoAction.length > 0
                        onFixRequested: (fixIndex) => root.flows.run(0, fixIndex)
                        onUploadRequested: root.flows.upload()
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
