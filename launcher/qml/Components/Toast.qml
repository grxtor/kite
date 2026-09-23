// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
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
    property string actionText: ""
    property var actionCallback: null

    // Drives every animated property below; show()/hide() only ever flip
    // this, so the slide+fade always run the same way in both directions.
    property bool shown: false

    // @p actionLabel/@p callback are optional -- most calls just say
    // something happened and are left with no button at all.
    function show(message, messageTone, actionLabel, callback) {
        label.text = message
        root.tone = messageTone || "neutral"
        root.actionText = actionLabel || ""
        root.actionCallback = callback || null
        root.shown = true
        hideTimer.restart()
    }

    function hide() {
        hideTimer.stop()
        root.shown = false
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
    // Without this the accent bar's square corners would poke out past
    // root's own rounded ones.
    clip: true
    opacity: shown ? 1 : 0
    visible: opacity > 0
    z: 1000

    // Slides up out of the bottom margin as it fades in, and back down as
    // it fades out.
    transform: Translate {
        y: root.shown ? 0 : Theme.space.md + root.height * 0.15
        Behavior on y { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
    }
    Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: label.text

    Timer {
        id: hideTimer
        interval: root.tone === "danger" ? 6000 : 3000
        onTriggered: root.shown = false
    }

    readonly property color toneColor: tone === "danger" ? Theme.palette.danger
                                       : tone === "success" ? Theme.palette.success : Theme.palette.accent

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 4
        color: root.toneColor
    }

    Row {
        id: row
        anchors.centerIn: parent
        anchors.horizontalCenterOffset: Theme.space.xs
        spacing: Theme.space.sm

        MeshIcon {
            anchors.verticalCenter: parent.verticalCenter
            iconName: root.tone === "danger" ? "alert-triangle" : root.tone === "success" ? "check" : "info"
            size: Theme.icon.sm + 2
            color: root.toneColor
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

        AbstractButton {
            id: actionButton
            visible: root.actionText.length > 0
            anchors.verticalCenter: parent.verticalCenter
            hoverEnabled: true
            leftPadding: Theme.space.sm
            implicitHeight: label.implicitHeight

            contentItem: Text {
                text: root.actionText
                color: actionButton.hovered ? Theme.palette.accentHover : Theme.palette.accent
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
                font.weight: Font.DemiBold
            }
            background: Item {}

            onClicked: {
                var callback = root.actionCallback
                root.hide()
                if (callback)
                    callback()
            }
        }
    }
}
