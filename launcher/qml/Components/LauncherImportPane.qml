// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Instances other launchers have on this computer, to bring into Kite: the
 * list grouped by launcher, then what the pick costs (disk space, the shared
 * folder of a link) and, while an import runs, its progress with a Stop. The
 * import button belongs to whoever holds the pane (the new-instance dialog,
 * the first-run flow); it calls start().
 */
ColumnLayout {
    id: root

    // QmlShell.launcherImport (LauncherImportController).
    property var controller: null
    // What lies behind the list, for its scroll fades.
    property color fadeTo: Theme.glass.popup

    readonly property bool running: !!root.controller && root.controller.running
    readonly property int selected: root.controller ? root.controller.selectedCount : 0
    readonly property bool canImport: !!root.controller && !root.running && root.selected > 0
                                      && root.controller.space !== "short"
    // One line on the pick, for a footer: how many and what the copies take.
    readonly property string summary: {
        if (!root.controller || root.running)
            return ""
        if (root.selected === 0)
            return root.controller.availableCount > 0 ? qsTr("Tick the instances to bring over.") : ""
        var linked = root.controller.linkedCount || 0
        var parts = [root.selected === 1 ? qsTr("1 instance selected")
                                         : qsTr("%1 instances selected").arg(root.selected)]
        if (root.selected > linked)
            parts.push(qsTr("%1 to copy").arg(root.controller.selectedSizeText))
        if (linked > 0)
            parts.push(linked === 1 ? qsTr("1 linked") : qsTr("%1 linked").arg(linked))
        return parts.join(", ")
    }
    // Shown once a run ends, until the next one starts.
    property bool showResult: false

    function start() {
        if (root.canImport && root.controller.start())
            root.showResult = true
    }

    // Looks once when first shown; the refresh button looks again.
    function scanIfNeeded() {
        if (root.controller && !root.controller.scanned && !root.controller.scanning)
            root.controller.scan()
    }

    function resultText() {
        if (!root.controller)
            return ""
        var imported = root.controller.importedCount
        var failed = root.controller.failedCount
        var parts = []
        if (imported > 0)
            parts.push(imported === 1 ? qsTr("1 instance imported.") : qsTr("%1 instances imported.").arg(imported))
        if (failed > 0)
            parts.push(failed === 1 ? qsTr("1 instance could not be imported.")
                                    : qsTr("%1 instances could not be imported.").arg(failed))
        return parts.join(" ")
    }

    spacing: Theme.space.md
    // Only once on screen: a pane built behind another tab does not look yet.
    Component.onCompleted: if (root.visible) root.scanIfNeeded()
    onVisibleChanged: if (root.visible) root.scanIfNeeded()
    onControllerChanged: if (root.visible) root.scanIfNeeded()

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space.sm

        CheckBox {
            id: allBox
            enabled: !!root.controller && root.controller.availableCount > 0 && !root.running
            checked: !!root.controller && root.controller.allSelected
            text: qsTr("Select all")
            onToggled: {
                root.controller.setAllChecked(checked)
                checked = Qt.binding(() => !!root.controller && root.controller.allSelected)
            }
        }
        Item { Layout.fillWidth: true }
        Copy {
            visible: !!root.controller && root.controller.scanned
            text: !root.controller ? ""
                : root.controller.foundCount === 1 ? qsTr("1 instance found")
                : qsTr("%1 instances found").arg(root.controller.foundCount)
            role: "caption"
            tone: "tertiary"
        }
        IconButton {
            iconName: "refresh"
            tip: qsTr("Look again")
            size: Theme.control.heightSm
            enabled: !!root.controller && !root.controller.scanning && !root.running
            onClicked: root.controller.scan()
        }
    }

    BoxSurface {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: Theme.control.heightLg * 3
        color: "transparent"

        ListView {
            id: list
            anchors.fill: parent
            anchors.margins: Theme.space.xs
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: root.controller ? root.controller.model : null
            spacing: Theme.space.xxs
            ScrollBar.vertical: ScrollBar {}

            // The launcher once per group: its name, and its logo where Kite carries it.
            section.property: "launcher"
            section.delegate: Item {
                id: heading
                required property string section
                width: list.width
                height: Theme.control.heightSm + Theme.space.xs
                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space.sm
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: Theme.space.xxs
                    spacing: Theme.space.sm
                    LauncherMark {
                        id: mark
                        anchors.verticalCenter: parent.verticalCenter
                        visible: mark.source.toString().length > 0
                        launcher: heading.section
                        size: Theme.icon.sm
                    }
                    Copy {
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.controller ? root.controller.launcherName(heading.section) : heading.section
                        role: "label"
                        tone: "secondary"
                        font.weight: Font.DemiBold
                    }
                }
            }

            delegate: LauncherImportRow {
                required property var model
                width: list.width
                row: model
                controller: root.controller
                locked: root.running
            }
        }

        ScrollFade {
            anchors.left: list.left
            anchors.right: list.right
            anchors.top: list.top
            height: Theme.space.lg
            atTop: true
            flickable: list
            fadeTo: root.fadeTo
        }
        ScrollFade {
            anchors.left: list.left
            anchors.right: list.right
            anchors.bottom: list.bottom
            flickable: list
            fadeTo: root.fadeTo
        }

        BusyIndicator {
            anchors.centerIn: parent
            running: !!root.controller && root.controller.scanning && list.count === 0
            visible: running
        }

        EmptyState {
            anchors.centerIn: parent
            width: parent.width - Theme.space.xxl * 2
            visible: !!root.controller && root.controller.scanned && !root.controller.scanning && list.count === 0
            iconName: "folder-input"
            title: qsTr("No other launchers found")
            body: qsTr("Kite looks where Prism Launcher, the Modrinth App, CurseForge and the Minecraft Launcher keep their instances.")
        }
    }

    Copy {
        Layout.fillWidth: true
        visible: !!root.controller && root.controller.unsupported.length > 0
        text: root.controller ? qsTr("Also found, not supported yet: %1").arg(root.controller.unsupported.join(", ")) : ""
        role: "caption"
        tone: "tertiary"
        wrapMode: Text.Wrap
    }
    Copy {
        Layout.fillWidth: true
        visible: !!root.controller && root.controller.unreadable.length > 0
        text: root.controller ? root.controller.unreadable.join("\n") : ""
        role: "caption"
        tone: "warning"
        wrapMode: Text.Wrap
    }

    NoticeBar {
        Layout.fillWidth: true
        visible: root.running
        tone: "info"
        text: root.controller ? root.controller.status : ""
        progress: root.controller ? root.controller.progress : -1
        actionText: qsTr("Stop")
        onActionTriggered: root.controller.cancel()
    }
    NoticeBar {
        Layout.fillWidth: true
        visible: !root.running && !!root.controller && root.selected > 0 && root.controller.space === "short"
        tone: "danger"
        text: root.controller ? qsTr("Not enough free space: the copies need %1 and the disk has %2.")
                                .arg(root.controller.selectedSizeText).arg(root.controller.freeSpaceText) : ""
    }
    NoticeBar {
        Layout.fillWidth: true
        visible: !root.running && !!root.controller && root.selected > 0 && root.controller.space === "low"
        tone: "warning"
        text: root.controller ? qsTr("Little space would be left: the copies take %1 of the %2 free.")
                                .arg(root.controller.selectedSizeText).arg(root.controller.freeSpaceText) : ""
    }
    NoticeBar {
        Layout.fillWidth: true
        visible: !root.running && !!root.controller && root.controller.anyLinked
        tone: "warning"
        text: qsTr("A linked instance keeps playing from the other launcher's folder: both launchers then write to the same worlds, mods and settings.")
    }
    NoticeBar {
        Layout.fillWidth: true
        visible: root.showResult && !root.running && !!root.controller
                 && root.controller.importedCount + root.controller.failedCount > 0
        tone: root.controller && root.controller.failedCount > 0 ? "warning" : "accent"
        text: {
            if (!root.controller)
                return ""
            var lines = [root.resultText()].concat(root.controller.errors)
            return lines.join("\n")
        }
    }
}
