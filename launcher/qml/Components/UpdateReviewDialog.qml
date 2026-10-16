// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * "Check for updates": the check's progress, then what it found -- each
 * file from the version installed now to the newest one that runs on this
 * instance, with a link to what changed -- to tick and update. The QML
 * replacement for ModUpdateDialog and the progress dialog before it.
 * Closing it while it checks gives up on the check.
 *
 * As tall as its rows, up to a limit; past that the list scrolls.
 */
Dialog {
    id: root

    // ContentUpdateController, and the check it is showing (TaskWatcher).
    property var controller: null
    property var watcher: null
    // Lower-case plural for the copy: "mods", "resource packs".
    property string noun: qsTr("mods")
    // Started with the download's TaskWatcher once Update is pressed.
    signal updateStarted(var watcher)

    readonly property var updates: root.controller ? root.controller.updates : null
    readonly property bool checking: !!root.controller && root.controller.checking
    readonly property int count: root.updates ? root.updates.count : 0
    readonly property int preferredWidth: 600
    readonly property int maxHeight: 580

    function show(watcher) {
        root.watcher = watcher
        root.open()
    }

    // The "All" box: ticked, unticked, or some of them.
    function allState() {
        if (!root.updates || root.updates.checkedCount === 0)
            return Qt.Unchecked
        return root.updates.checkedCount === root.updates.count ? Qt.Checked : Qt.PartiallyChecked
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(root.preferredWidth, parent ? parent.width - Theme.space.xxl * 2 : root.preferredWidth)
    height: Math.min(root.maxHeight, implicitHeight, parent ? parent.height - Theme.space.xxl * 2 : root.maxHeight)
    modal: true
    closePolicy: Popup.CloseOnEscape
    title: qsTr("Updates")

    header: DialogHeader {
        title: root.title
        icon: "refresh"
    }

    onClosed: {
        if (root.checking && root.controller)
            root.controller.cancelCheck()
        root.watcher = null
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        // While checking: what it is doing, and how far along.
        ColumnLayout {
            Layout.fillWidth: true
            visible: root.checking
            spacing: Theme.space.sm
            Text {
                Layout.fillWidth: true
                text: root.watcher && root.watcher.status.length > 0 ? root.watcher.status
                                                                       : qsTr("Checking for updates…")
                elide: Text.ElideRight
                color: Theme.palette.textSecondary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
            }
            LaunchProgressBar {
                Layout.fillWidth: true
                progress: root.watcher ? root.watcher.progress : -1
            }
        }

        Text {
            Layout.fillWidth: true
            visible: !root.checking
            text: root.count === 0 ? qsTr("Everything is up to date.")
                  : qsTr("Newer versions that run on this instance. Untick any you want to keep as they are.")
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            lineHeight: Theme.type.label.lineHeight
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: contentHeight
            visible: !root.checking && root.count > 0
            clip: true
            spacing: Theme.space.xs
            boundsBehavior: Flickable.StopAtBounds
            model: root.updates
            ScrollBar.vertical: ScrollBar {}

            delegate: ReviewRow {
                required property int index
                required property string name
                required property string provider
                required property string currentVersion
                required property string newVersion
                required property string changelog
                required property var model

                width: list.width
                checked: model.checked
                title: name
                subtitle: qsTr("%1 → %2").arg(currentVersion).arg(newVersion)
                tags: provider.length > 0 ? [provider] : []
                linkText: qsTr("What's new")
                linkUrl: changelog
                onToggled: (on) => root.updates.setChecked(index, on)
            }
        }
    }

    footer: Item {
        implicitHeight: footerRow.implicitHeight + Theme.space.lg

        RowLayout {
            id: footerRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: Theme.space.lg
            anchors.rightMargin: Theme.space.lg
            spacing: Theme.space.sm

            CheckBox {
                // Box edge on the content edge.
                leftPadding: 0
                visible: !root.checking && root.count > 1
                text: qsTr("All")
                checkState: root.allState()
                onClicked: {
                    root.updates.setAllChecked(root.updates.checkedCount !== root.updates.count)
                    checkState = Qt.binding(() => root.allState())
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                flat: true
                text: root.checking || root.count > 0 ? qsTr("Cancel") : qsTr("Close")
                onClicked: root.close()
            }
            Button {
                visible: !root.checking && root.count > 0
                highlighted: true
                enabled: !!root.updates && root.updates.checkedCount > 0
                text: root.updates ? qsTr("Update %1").arg(root.updates.checkedCount) : qsTr("Update")
                icon.source: Icons.url("download")
                onClicked: {
                    const watcher = root.controller.applyUpdates()
                    root.close()
                    if (watcher)
                        root.updateStarted(watcher)
                }
            }
        }
    }
}
