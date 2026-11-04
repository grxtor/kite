// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The ⋯ of a crash doctor cause: its other fixes, then the log upload, the crash report, undoing
 * the last fix and hiding the cause. The first fix is the button next to it, so it is not here.
 */
Menu {
    id: root

    // CrashDoctorModel.causes[i], or null for "couldn't tell".
    property var cause: null
    property bool working: false
    property bool canUpload: true
    // The upload is here even without fixes (the caller's button does something else).
    property bool uploadInMenu: false
    // Offer "Send to Kite" here (the Log tab's strip, when a fix did not help).
    property bool canSend: false
    property bool hasCrashReport: false
    property bool canUndo: false
    signal fixRequested(int fixIndex)
    signal uploadRequested()
    signal sendRequested()
    signal crashReportRequested()
    signal undoRequested()
    signal dismissRequested()

    readonly property var fixes: root.cause ? root.cause.fixes : []

    Repeater {
        model: root.fixes.length > 1 ? root.fixes.slice(1) : []
        delegate: MenuItem {
            required property var modelData
            required property int index
            enabled: !root.working
            text: modelData.label
            onTriggered: root.fixRequested(index + 1)
        }
    }
    MenuSeparator { visible: root.fixes.length > 1 }
    MenuItem {
        visible: root.canSend
        height: visible ? implicitHeight : 0
        text: qsTr("Send to Kite")
        icon.source: Icons.url("arrow-up")
        onTriggered: root.sendRequested()
    }
    MenuItem {
        visible: (root.fixes.length > 0 || root.uploadInMenu) && root.canUpload
        height: visible ? implicitHeight : 0
        text: qsTr("Share the log")
        icon.source: Icons.url("arrow-up")
        onTriggered: root.uploadRequested()
    }
    MenuItem {
        visible: root.hasCrashReport
        height: visible ? implicitHeight : 0
        text: qsTr("Open the crash report")
        icon.source: Icons.url("file")
        onTriggered: root.crashReportRequested()
    }
    MenuItem {
        visible: root.canUndo
        height: visible ? implicitHeight : 0
        text: qsTr("Undo the last fix")
        icon.source: Icons.url("refresh")
        onTriggered: root.undoRequested()
    }
    MenuItem {
        text: qsTr("Hide this")
        icon.source: Icons.url("x")
        onTriggered: root.dismissRequested()
    }
}
