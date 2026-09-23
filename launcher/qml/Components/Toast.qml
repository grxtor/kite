// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * A short note at the bottom of the window that goes away by itself --
 * "Instance deleted", "Copy failed: ..." -- for outcomes worth saying but
 * not worth a dialog. show() replaces whatever is showing.
 */
Rectangle {
    id: root

    property string tone: "neutral" // neutral | success | danger
    property alias text: label.text

    function show(message, messageTone) {
        label.text = message
        root.tone = messageTone || "neutral"
        root.opacity = 1
        hideTimer.restart()
    }

    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: Theme.space.xl
    width: Math.min(row.implicitWidth + Theme.space.lg * 2, parent.width - Theme.space.xxl * 2)
    height: Theme.control.heightLg
    radius: Theme.radius.lg
    color: Theme.palette.surfaceOverlay
    border.width: 1
    border.color: Theme.palette.borderStrong
    opacity: 0
    visible: opacity > 0
    z: 1000

    Behavior on opacity { NumberAnimation { duration: Theme.motion.normal } }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: label.text

    Timer {
        id: hideTimer
        interval: root.tone === "danger" ? 6000 : 3000
        onTriggered: root.opacity = 0
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.space.sm

        MeshIcon {
            anchors.verticalCenter: parent.verticalCenter
            iconName: root.tone === "danger" ? "alert-triangle" : root.tone === "success" ? "check" : "info"
            size: Theme.icon.sm + 2
            color: root.tone === "danger" ? Theme.palette.danger
                 : root.tone === "success" ? Theme.palette.success : Theme.palette.accent
        }
        Text {
            id: label
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
            font.weight: Font.Medium
            elide: Text.ElideRight
        }
    }
}
