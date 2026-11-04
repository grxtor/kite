// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "Why <instance> crashed": the window that opens once, by itself, right after a game crashes
 * while Kite runs (Main.qml, QmlShell::crashAnalysed() with fresh set), never for an old crash.
 * Each cause in its own box with its fix and ⋯; the first is the one Kite is surest of. Anything
 * that belongs to the instance's page (the log, a tab, the upload) closes the window and goes there.
 * Afterwards the Log tab's strip (CrashDoctorPanel) keeps saying it.
 */
AppDialog {
    id: root

    property var shell: null
    property string instanceId: ""
    // The window belongs to Main; these lead to the instance's page.
    signal tabRequested(string id, string tab)
    signal logLineRequested(string id, string line)
    signal crashReportRequested(string id, string name)
    signal uploadRequested(string id)
    signal playRequested(string id)
    signal folderRequested(string path)

    readonly property var details: root.shell && root.instanceId.length > 0
                                   ? root.shell.instanceDetails(root.instanceId) : null
    readonly property var doctor: root.details ? root.details.crashDoctor : null
    readonly property var causes: root.doctor ? root.doctor.causes : []
    readonly property bool busy: !!root.doctor && root.doctor.busy
    readonly property int maxHeight: 640

    function show(id) {
        root.instanceId = id
        root.open()
    }

    title: root.details ? qsTr("Why %1 crashed").arg(root.details.name) : qsTr("Why the game crashed")
    icon: "alert-triangle"
    iconColor: Theme.palette.warning
    size: "lg"
    // The one accent button here is the first fix; closing is a plain one.
    showPrimary: false
    cancelText: qsTr("Close")
    height: Math.min(root.maxHeight, implicitHeight, parent ? parent.height - Theme.space.xxl * 2 : root.maxHeight)

    // The window went away once its crash was set aside (Hide this) or a new launch began.
    Connections {
        target: root.doctor
        function onChanged() {
            if (root.opened && !root.doctor.hasReport && !root.doctor.busy)
                root.close()
        }
    }

    // Main's toast, for what a fix says.
    property var toast: null

    CrashDoctorFlows {
        id: flows
        doctor: root.doctor
        details: root.details
        shell: root.shell
        toast: root.toast
        onNavigateTab: (tab) => {
            root.close()
            root.tabRequested(root.instanceId, tab)
        }
        onOpenFolder: (path) => root.folderRequested(path)
        onPlayRequested: {
            root.close()
            root.playRequested(root.instanceId)
        }
        onCrashReportRequested: (name) => {
            root.close()
            root.crashReportRequested(root.instanceId, name)
        }
        onUploadRequested: {
            root.close()
            root.uploadRequested(root.instanceId)
        }
    }
    contentItem: ListView {
        id: list
        implicitHeight: Math.max(contentHeight, Theme.control.heightLg)
        clip: true
        spacing: Theme.space.sm
        boundsBehavior: Flickable.StopAtBounds
        // One box per cause; one for "couldn't tell" when there is none.
        model: root.busy ? 0 : Math.max(1, root.causes.length)
        ScrollBar.vertical: ScrollBar {}

        header: RowLayout {
            width: list.width
            visible: root.busy
            height: visible ? implicitHeight : 0
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

        delegate: BoxSurface {
            id: item
            required property int index
            readonly property bool known: root.causes.length > 0
            width: list.width
            implicitHeight: view.implicitHeight + Theme.space.lg * 2
            sunken: item.index > 0
            radius: Theme.radius.md

            CrashCauseView {
                id: view
                anchors.fill: parent
                anchors.margins: Theme.space.lg
                cause: item.known ? root.causes[item.index] : Format.unknownCrashCause(root.doctor)
                main: item.index === 0
                showIcon: false
                working: item.known && flows.workingCause === item.index
                workingText: flows.workingText
                hasCrashReport: !!root.doctor && root.doctor.crashReport.length > 0
                canUndo: !!root.doctor && root.doctor.undoAction.length > 0
                onUndoRequested: flows.undo()
                onFixRequested: (fixIndex) => flows.run(item.index, fixIndex)
                onShowLine: (line) => {
                    root.close()
                    root.logLineRequested(root.instanceId, line)
                }
                onUploadRequested: flows.upload()
                onCrashReportRequested: flows.openCrashReport()
                onDismissRequested: root.doctor.dismiss()
            }
        }
    }
}
