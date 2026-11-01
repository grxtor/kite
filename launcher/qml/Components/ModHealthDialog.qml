// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * What is wrong with the mods in this instance: the health check (ModHealthModel)
 * as a list, each row offering the one fix the tab can do (enable a dependency,
 * disable a clashing file, download a missing or outdated dependency). Presses
 * leave to ContentTab (actionRequested, installAll), which owns the mods list
 * and the install flow; both are locked while the game runs.
 */
AppDialog {
    id: root

    // InstanceContentController's `health`, a ModHealthModel.
    property var health: null
    property bool unlocked: true
    property string lockedReason: ""
    // @p action is "enable", "disable" or "install"; @p key the file to switch (for "install", the
    // file that needs the mod); @p dependencyId the mod to install; @p replacesKey the file it
    // replaces, or "".
    signal actionRequested(string action, string key, string dependencyId, string replacesKey)
    signal installAll()

    readonly property int maxHeight: 560
    readonly property int actionLabelMaxWidth: 220
    readonly property bool blocking: !!root.health && root.health.blockingCount > 0
    readonly property int installCount: {
        if (!root.health)
            return 0
        root.health.revision
        return root.health.installRows().length
    }

    height: Math.min(root.maxHeight, implicitHeight, parent ? parent.height - Theme.space.xxl * 2 : root.maxHeight)
    title: qsTr("Mod problems")
    icon: "alert-triangle"
    iconColor: root.blocking ? Theme.palette.danger : Theme.palette.warning

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        PanelNote {
            Layout.fillWidth: true
            text: !root.health || root.health.count === 0 ? qsTr("Nothing is wrong with these mods.")
                  : root.blocking ? qsTr("The game may not start until these are sorted out.")
                  : qsTr("The game should still start. These are worth a look.")
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: contentHeight
            clip: true
            spacing: Theme.space.xs
            boundsBehavior: Flickable.StopAtBounds
            model: root.health
            ScrollBar.vertical: ScrollBar {}

            delegate: BoxSurface {
                id: item
                required property string key
                required property string title
                required property string detail
                required property bool blocking
                required property string action
                required property string actionKey
                required property string actionLabel
                required property string dependencyId
                required property string replacesKey

                readonly property bool locked: !root.unlocked

                width: list.width
                implicitHeight: Math.max(body.implicitHeight, actionHolder.visible ? actionHolder.implicitHeight : 0)
                                + Theme.space.md * 2
                sunken: true
                radius: Theme.radius.sm

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space.md
                    anchors.rightMargin: Theme.space.md
                    spacing: Theme.space.md

                    MeshIcon {
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: Theme.space.md
                        iconName: "alert-triangle"
                        size: Theme.icon.sm
                        color: item.blocking ? Theme.palette.danger : Theme.palette.warning
                    }

                    ColumnLayout {
                        id: body
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: Theme.space.xxs

                        Copy {
                            Layout.fillWidth: true
                            text: item.title
                            wrapMode: Text.Wrap
                            font.weight: Font.Medium
                        }
                        Copy {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: item.detail
                            role: "caption"
                            tone: "tertiary"
                            wrapMode: Text.Wrap
                        }
                    }

                    HoverReason {
                        id: actionHolder
                        visible: item.action !== ""
                        Layout.alignment: Qt.AlignVCenter
                        reason: item.locked ? root.lockedReason : ""

                        Button {
                            enabled: !item.locked
                            text: labelMetrics.elidedText
                            ToolTip.visible: hovered && labelMetrics.elidedText !== item.actionLabel
                            ToolTip.delay: 400
                            ToolTip.text: item.actionLabel
                            onClicked: root.actionRequested(item.action, item.action === "install" ? item.key : item.actionKey,
                                                            item.dependencyId, item.replacesKey)

                            TextMetrics {
                                id: labelMetrics
                                font.family: Theme.font.family
                                font.pixelSize: Theme.type.body.pixelSize
                                font.weight: Font.Medium
                                text: item.actionLabel
                                elide: Text.ElideMiddle
                                elideWidth: root.actionLabelMaxWidth
                            }
                        }
                    }
                }
            }
        }
    }

    footer: Item {
        implicitHeight: footerRow.implicitHeight + Theme.space.xl

        RowLayout {
            id: footerRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: Theme.space.xl
            anchors.rightMargin: Theme.space.xl
            spacing: Theme.space.sm

            Item { Layout.fillWidth: true }
            HoverReason {
                visible: root.installCount > 1
                reason: root.unlocked ? "" : root.lockedReason

                Button {
                    enabled: root.unlocked
                    text: qsTr("Install all")
                    icon.source: Icons.url("download")
                    onClicked: root.installAll()
                }
            }
            Button {
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
