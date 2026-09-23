// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * The signed-in account at the foot of the sidebar. With no account it
 * turns into a sign-in prompt instead of showing a fake "Guest": playing
 * online needs an account, so that is the one thing worth saying here.
 */
AbstractButton {
    id: control

    property string name
    // "Microsoft", "Offline" or "" when there is no account.
    property string kind
    property string avatarSource

    readonly property bool signedIn: name.length > 0

    implicitHeight: Theme.control.heightLg + Theme.space.md
    implicitWidth: 200
    hoverEnabled: true

    Accessible.name: signedIn ? qsTr("Account: %1").arg(name) : qsTr("Sign in")

    background: Rectangle {
        radius: Theme.radius.lg
        color: control.hovered ? Theme.palette.surfaceRaised : Theme.palette.surface
        border.width: 1
        border.color: Theme.palette.border
        Behavior on color { ColorAnimation { duration: Theme.motion.fast; easing.type: Theme.motion.easing } }
    }

    contentItem: Item {
        Rectangle {
            id: avatar
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.sm + 2
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.control.height - 4
            height: width
            radius: width / 2
            color: control.signedIn ? Theme.palette.accentSubtle : Theme.palette.surfaceOverlay
            border.width: 1
            border.color: Theme.palette.border

            Text {
                anchors.centerIn: parent
                visible: control.signedIn
                text: control.name.charAt(0).toUpperCase()
                color: Theme.palette.accent
                font.family: Theme.font.family
                font.pixelSize: Theme.type.title.pixelSize
                font.weight: Font.Bold
            }

            Image {
                id: avatarImage
                anchors.fill: parent
                anchors.margins: 1
                // Drawn over the initial: an account without a skin comes
                // back transparent and the initial shows through. Sampled
                // well above display size and downscaled by the GPU
                // (smooth: false keeps that downscale a crisp nearest-
                // neighbour one) so the face stays sharp regardless of the
                // chip's own size or the display's pixel ratio.
                source: control.avatarSource
                visible: control.signedIn && control.avatarSource.length > 0
                smooth: false
                sourceSize: Qt.size(64, 64)
            }

            MeshIcon {
                anchors.centerIn: parent
                visible: !control.signedIn
                iconName: "user"
                size: Theme.icon.md
                color: Theme.palette.textSecondary
            }
        }

        Column {
            anchors.left: avatar.right
            anchors.leftMargin: Theme.space.sm + 2
            anchors.right: chevron.left
            anchors.rightMargin: Theme.space.xs
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
                width: parent.width
                text: control.signedIn ? control.name : qsTr("Sign in")
                elide: Text.ElideRight
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.label.pixelSize
                font.weight: Font.DemiBold
            }

            Text {
                width: parent.width
                text: !control.signedIn ? qsTr("Not signed in")
                    : control.kind === "Microsoft" ? qsTr("Microsoft account")
                    : control.kind === "Offline" ? qsTr("Offline account")
                    : control.kind
                elide: Text.ElideRight
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }

        MeshIcon {
            id: chevron
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.sm
            anchors.verticalCenter: parent.verticalCenter
            iconName: "chevron-right"
            size: Theme.icon.sm
            color: control.hovered ? Theme.palette.textSecondary : Theme.palette.textTertiary
        }
    }
}
