// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * What is wrong with the mods in this instance: the mods tab's health
 * check (ModHealthModel) as a list a person can read and act on. Each row
 * says what the problem is, what follows from it, and offers the one thing
 * the tab can do about it -- turn a switched-off dependency on, turn a
 * clashing, doubled or foreign file off, or download a dependency that is
 * missing or outdated and install it. A problem the tab cannot fix (the
 * loader's own version) has no button, only the explanation. With two or
 * more installs on offer, "Install all" asks for them in one review.
 *
 * Problems the loader would refuse to start over wear the danger colour;
 * warnings the warning one. The rows follow the check as it is redone, so
 * a row goes when its problem does.
 *
 * Presses leave to ContentTab (actionRequested, installAll), which owns the
 * mods list and the install flow. Both switching files and installing write
 * into the mods folder, so both are locked while the game runs, like the
 * list itself.
 *
 * As tall as its rows, up to a limit; past that the list scrolls.
 */
Dialog {
    id: root

    // InstanceContentController's `health`, a ModHealthModel.
    property var health: null
    // Whether the mods folder may be changed now, and why not.
    property bool unlocked: true
    property string lockedReason: ""
    // @p action is "enable", "disable" or "install"; @p key the file to
    // switch, or for "install" the file that needs the mod; @p dependencyId
    // the mod to install and @p replacesKey the file it replaces, or "".
    signal actionRequested(string action, string key, string dependencyId, string replacesKey)
    // Every install on offer, in one review.
    signal installAll()

    readonly property int preferredWidth: 560
    readonly property int maxHeight: 560
    // The widest an action's label gets before it is shortened in the
    // middle (a file name with its version can be long).
    readonly property int actionLabelMaxWidth: 220
    readonly property bool blocking: !!root.health && root.health.blockingCount > 0
    // How many rows offer an install; re-asked when the check is redone.
    readonly property int installCount: {
        if (!root.health)
            return 0
        root.health.revision
        return root.health.installRows().length
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(root.preferredWidth, parent ? parent.width - Theme.space.xxl * 2 : root.preferredWidth)
    height: Math.min(root.maxHeight, implicitHeight, parent ? parent.height - Theme.space.xxl * 2 : root.maxHeight)
    modal: true
    // The title row sets the inset; the gap under it is the dialog's own spacing.
    topPadding: 0
    // The actions row sets the bottom inset; this and the spacing over it
    // leave one inset (xl) between the body and the actions.
    bottomPadding: Theme.space.sm
    closePolicy: Popup.CloseOnEscape
    title: qsTr("Mod problems")

    header: DialogHeader {
        title: root.title
        icon: "alert-triangle"
        iconColor: root.blocking ? Theme.palette.danger : Theme.palette.warning
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Text {
            Layout.fillWidth: true
            text: !root.health || root.health.count === 0 ? qsTr("Nothing is wrong with these mods.")
                  : root.blocking ? qsTr("The game may not start until these are sorted out.")
                  : qsTr("The game should still start. These are worth a look.")
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
            clip: true
            spacing: Theme.space.xs
            boundsBehavior: Flickable.StopAtBounds
            model: root.health
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
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

                // Switching a file and installing one both change the mods
                // folder, which waits for the game to close.
                readonly property bool locked: !root.unlocked

                width: list.width
                implicitHeight: Math.max(body.implicitHeight, actionHolder.visible ? actionHolder.implicitHeight : 0)
                                + Theme.space.md * 2
                radius: Theme.radius.sm
                color: Theme.palette.surfaceSunken
                border.width: 1
                border.color: Theme.palette.border

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

                        Text {
                            Layout.fillWidth: true
                            text: item.title
                            wrapMode: Text.Wrap
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                            font.weight: Font.Medium
                        }
                        Text {
                            Layout.fillWidth: true
                            visible: text.length > 0
                            text: item.detail
                            wrapMode: Text.Wrap
                            color: Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
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
