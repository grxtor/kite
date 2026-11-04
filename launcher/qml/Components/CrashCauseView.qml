// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One cause the crash doctor found (CrashDoctorModel.causes[i]): what happened in a title and two
 * short sentences, the mods it names when it is a group, the log line that shows it, and its fix as
 * one button, the other fixes in ⋯ (CrashCauseMenu) with the log upload and the crash report, and a
 * link to the log line it was read from. Draws no surface of its own. Under the Log tab's strip,
 * which already says the title and holds the buttons, it shows only the details (showTitle and
 * showActions off); in the crash window the window's own icon stands for all (showIcon off).
 */
ColumnLayout {
    id: root

    // {ruleId, title, explanation, line, count, rows, fixes: [{action, label, params, runner, confirm}]},
    // and for "couldn't tell" a muted detail ("Exit code 1").
    property var cause: null
    // The first cause gets the accent button; the others a plain one.
    property bool main: true
    property bool showTitle: true
    property bool showActions: true
    property bool showIcon: true
    // A fix of this cause is running; its button says so.
    property bool working: false
    property string workingText: ""
    property bool hasCrashReport: false
    property bool canUpload: true
    // The log upload in ⋯ even with no fix: something else holds the main button (CrashSendBlock).
    property bool uploadInMenu: false
    // The last fix can still be undone (CrashDoctorModel.undoAction).
    property bool canUndo: false
    signal fixRequested(int fixIndex)
    signal showLine(string line)
    signal uploadRequested()
    signal crashReportRequested()
    signal dismissRequested()
    signal undoRequested()

    readonly property var fixes: root.cause ? root.cause.fixes : []
    readonly property var rows: root.cause && root.cause.count > 1 ? root.cause.rows : []
    readonly property int maxRows: 6

    spacing: Theme.space.sm

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space.md

        MeshIcon {
            Layout.alignment: Qt.AlignTop
            Layout.topMargin: Theme.space.xxs
            visible: root.showTitle && root.showIcon
            iconName: "alert-triangle"
            size: Theme.icon.md
            color: Theme.palette.warning
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Theme.space.xs

            Copy {
                Layout.fillWidth: true
                visible: root.showTitle
                text: root.cause ? root.cause.title : ""
                role: "bodyStrong"
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }
            Copy {
                Layout.fillWidth: true
                text: root.cause ? root.cause.explanation : ""
                tone: "secondary"
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
            }

            Copy {
                Layout.fillWidth: true
                visible: !!root.cause && (root.cause.detail || "").length > 0
                text: root.cause ? root.cause.detail || "" : ""
                role: "caption"
                tone: "tertiary"
                textFormat: Text.PlainText
            }

            Repeater {
                model: root.rows.slice(0, root.maxRows)
                delegate: Copy {
                    required property string modelData
                    Layout.fillWidth: true
                    text: "•  " + modelData
                    role: "label"
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                }
            }
            Copy {
                visible: root.rows.length > root.maxRows
                text: qsTr("and %1 more").arg(root.rows.length - root.maxRows)
                role: "caption"
                tone: "tertiary"
            }
            // Where it was read, so the player can see it is not a guess.
            LinkButton {
                visible: !!root.cause && root.cause.line.length > 0
                text: qsTr("Show in log")
                onClicked: root.showLine(root.cause.line)
            }
        }

        // The one fix, then everything else in ⋯.
        RowLayout {
            Layout.alignment: Qt.AlignTop
            visible: root.showActions
            spacing: Theme.space.xs

            Button {
                visible: root.fixes.length > 0
                highlighted: root.main
                enabled: !root.working
                text: root.working && root.workingText.length > 0 ? root.workingText
                      : root.fixes.length > 0 ? root.fixes[0].label : ""
                onClicked: root.fixRequested(0)
            }
            Button {
                visible: root.fixes.length === 0 && root.canUpload && !root.uploadInMenu
                highlighted: root.main
                text: qsTr("Share the log")
                onClicked: root.uploadRequested()
            }
            IconButton {
                id: moreButton
                iconName: "more"
                tip: qsTr("More")
                onClicked: menu.popup(moreButton, moreButton.width - menu.width, moreButton.height)

                CrashCauseMenu {
                    id: menu
                    cause: root.cause
                    working: root.working
                    canUpload: root.canUpload
                    uploadInMenu: root.uploadInMenu
                    hasCrashReport: root.hasCrashReport
                    canUndo: root.canUndo
                    onFixRequested: (fixIndex) => root.fixRequested(fixIndex)
                    onUploadRequested: root.uploadRequested()
                    onCrashReportRequested: root.crashReportRequested()
                    onUndoRequested: root.undoRequested()
                    onDismissRequested: root.dismissRequested()
                }
            }
        }
    }
}
