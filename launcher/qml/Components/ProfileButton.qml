// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The account, top-right on every page: the default account's Minecraft
 * face, its name and a chevron, opening ProfileMenu below it. With no
 * account there is nothing to open -- a plain "Sign in" button goes straight
 * to the Accounts page instead (see openAccountsRequested()), the same page
 * ProfileMenu's own account actions land on.
 */
AbstractButton {
    id: control

    property string name: ""
    // "Microsoft", "Offline" or "" when there is no account.
    property string kind: ""
    property string avatarSource: ""
    // AccountsController: accounts, setDefault, remove -- see ProfileMenu.
    property var controller: null

    readonly property bool signedIn: control.name.length > 0
    readonly property int maxNameWidth: 130

    signal openAccountsRequested()

    // For a dev-route snapshot ("profilemenu") that needs the dropdown open
    // without a real click.
    function openMenu() { if (control.signedIn) menu.open() }

    readonly property int hPadding: Theme.space.sm + 2
    implicitWidth: contentRow.implicitWidth + hPadding * 2
    implicitHeight: Theme.control.height
    hoverEnabled: true

    Accessible.name: signedIn ? qsTr("Account menu: %1").arg(control.name) : qsTr("Sign in")

    onClicked: control.signedIn ? menu.toggle() : control.openAccountsRequested()

    background: Rectangle {
        radius: Theme.radius.pill
        color: control.down ? Theme.palette.pressedOverlay
             : control.hovered || menu.visible ? Theme.palette.hoverOverlay : "transparent"
        border.width: 1
        border.color: control.hovered || menu.visible ? Theme.palette.borderStrong : Theme.palette.border

        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
        Behavior on border.color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    // A plain Item, not the Row itself, is the contentItem: QQC2 stretches
    // contentItem to fill the button's whole content box, and a Row whose
    // own width is forced wider than its packed children just leaves the
    // surplus as dead space after the last one (the chevron) -- exactly the
    // gap this used to show. Centering the Row inside this wrapper instead
    // keeps the Row itself sized to its content, so the pill hugs it.
    contentItem: Item {
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight

        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: Theme.space.sm

            Item {
                id: face
                anchors.verticalCenter: parent.verticalCenter
                visible: control.signedIn
                width: Theme.control.height - 10
                height: width

                Rectangle {
                    anchors.fill: parent
                    // A small corner radius, not a full circle: the face
                    // image is a plain square (Qt Quick clip only clips to
                    // the bounding box, not to a rounded shape, so a fully
                    // round container just let the image's square corners
                    // poke out past it). A small radius keeps that overflow
                    // imperceptible instead of fighting a shape the image
                    // was never cut to.
                    radius: 6
                    color: Theme.palette.accentSubtle
                    border.width: 1
                    border.color: Theme.palette.border

                    Text {
                        anchors.centerIn: parent
                        visible: control.signedIn
                        text: control.name.charAt(0).toUpperCase()
                        color: Theme.palette.accent
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                        font.weight: Font.Bold
                    }

                    // Drawn over the initial, not the other way around: an
                    // account without a skin (or one whose texture has not
                    // loaded yet) comes back transparent, and the initial
                    // shows through -- same idiom as the old sidebar chip
                    // this replaces. Sampled well above display size and
                    // downscaled by the GPU (smooth: false keeps that
                    // downscale crisp).
                    Image {
                        id: faceImage
                        anchors.fill: parent
                        anchors.margins: 1
                        source: control.signedIn ? control.avatarSource : ""
                        visible: control.signedIn && control.avatarSource.length > 0
                        smooth: false
                        sourceSize: Qt.size(64, 64)
                    }
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.signedIn
                width: Math.min(implicitWidth, control.maxNameWidth)
                text: control.name
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                font.weight: Font.DemiBold
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: !control.signedIn
                text: qsTr("Sign in")
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                font.weight: Font.DemiBold
            }

            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.signedIn
                iconName: "chevron-down"
                size: Theme.icon.sm
                color: Theme.palette.textSecondary
                rotation: menu.visible ? 180 : 0

                Behavior on rotation { NumberAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
            }
        }
    }

    ProfileMenu {
        id: menu
        x: control.width - width
        y: control.height + Theme.space.xs
        name: control.name
        kind: control.kind
        avatarSource: control.avatarSource
        controller: control.controller
        function toggle() { visible ? close() : open() }
        onOpenAccountsRequested: control.openAccountsRequested()
    }
}
