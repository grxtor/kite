// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * A modpack as a compact tile: ModpackCard's logo, title, author, downloads and Install without the pitch and the
 * tags, for a row of suggestions (Home's "Popular modpacks"). The whole tile opens the pack; Install is the same
 * install the Discover rows have. `skeleton: true` swaps every field for a Skeleton block.
 */
AbstractButton {
    id: root

    property string title: ""
    property string author: ""
    property string logoUrl: ""
    property var downloads: 0
    property bool skeleton: false
    // As ModpackCard's: "", "pending", "installing", "installed" or "failed" (`installError` says why).
    property string installState: ""
    property string installError: ""
    property bool installEnabled: true
    signal installRequested()

    readonly property int logoSize: Theme.control.heightLg
    readonly property int logoSourceSize: logoSize * 2

    implicitHeight: content.implicitHeight + topPadding + bottomPadding
    padding: Theme.space.md
    hoverEnabled: !root.skeleton
    enabled: !root.skeleton
    focusPolicy: Qt.TabFocus

    Accessible.role: Accessible.ListItem
    Accessible.name: root.title

    background: Rectangle {
        id: card
        radius: Theme.radius.lg
        color: root.down ? Theme.palette.surfaceRaised
             : root.hovered ? Theme.palette.surfaceHover : Theme.palette.surface
        border.width: root.visualFocus ? Theme.focus.width : 1
        border.color: root.visualFocus ? Theme.palette.focusRing
                    : root.hovered ? Theme.neutral.borderHover : Theme.palette.border
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    contentItem: ColumnLayout {
        id: content
        spacing: Theme.space.md

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.md

            Item {
                Layout.preferredWidth: root.logoSize
                Layout.preferredHeight: root.logoSize

                Skeleton {
                    anchors.fill: parent
                    radius: Theme.radius.md
                    visible: root.skeleton
                }
                Image {
                    id: logo
                    anchors.fill: parent
                    visible: !root.skeleton && status === Image.Ready
                    source: root.skeleton ? "" : root.logoUrl
                    sourceSize: Qt.size(root.logoSourceSize, root.logoSourceSize)
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    mipmap: true
                    asynchronous: true
                }
                MeshIcon {
                    anchors.centerIn: parent
                    visible: !root.skeleton && (root.logoUrl.length === 0 || logo.status === Image.Error)
                    iconName: "package"
                    size: Theme.icon.lg
                    color: Theme.palette.textTertiary
                }
                CornerMatte {
                    visible: logo.visible
                    cornerRadius: Theme.radius.md
                    matte: card.color
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: Theme.space.xxs

                Copy {
                    Layout.fillWidth: true
                    visible: !root.skeleton
                    text: root.title
                    elide: Text.ElideRight
                    role: "bodyStrong"
                }
                Copy {
                    Layout.fillWidth: true
                    visible: !root.skeleton
                    text: root.author.length > 0 ? qsTr("by %1").arg(root.author) : ""
                    elide: Text.ElideRight
                    role: "label"
                    tone: "secondary"
                }
                Skeleton {
                    visible: root.skeleton
                    Layout.preferredWidth: 96
                    Layout.preferredHeight: Theme.type.body.pixelSize
                    radius: Theme.radius.sm
                }
                Skeleton {
                    visible: root.skeleton
                    Layout.preferredWidth: 64
                    Layout.preferredHeight: Theme.type.label.pixelSize
                    radius: Theme.radius.sm
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.control.heightSm
            spacing: Theme.space.xs

            MeshIcon {
                visible: downloadsText.visible
                iconName: "download"
                size: Theme.icon.sm
                color: Theme.palette.textTertiary
            }
            Copy {
                id: downloadsText
                // Not every provider counts downloads.
                visible: !root.skeleton && (root.downloads || 0) > 0
                text: Format.compactNumber(root.downloads)
                role: "label"
                tone: "secondary"
            }
            Item { Layout.fillWidth: true }
            Button {
                readonly property bool busy: root.installState === "pending" || root.installState === "installing"
                visible: !root.skeleton
                enabled: root.installEnabled && !busy && root.installState !== "installed"
                buttonSize: "sm"
                text: busy ? qsTr("Installing…")
                    : root.installState === "installed" ? qsTr("Installed")
                    : root.installState === "failed" ? qsTr("Try again") : qsTr("Install")
                icon.source: Icons.url(root.installState === "installed" ? "check" : "download")
                Accessible.name: qsTr("Install %1").arg(root.title)
                ToolTip.visible: hovered && root.installState === "failed" && root.installError.length > 0
                ToolTip.delay: 400
                ToolTip.text: root.installError
                onClicked: root.installRequested()
            }
        }
    }
}
