// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * A short note at the bottom of the window that goes away by itself. show()
 * replaces whatever is showing. With an action ("Undo", "Show in folder") it
 * stays twice as long, and never goes while the pointer is on it.
 */
GlassSurface {
    id: root

    property string tone: "neutral" // neutral | success | danger
    property alias text: label.text
    property string actionText: ""
    property var actionCallback: null

    // Drives every animated property below, so the slide and fade run the same way in both directions.
    property bool shown: false

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
    width: Math.min(row.implicitWidth + Theme.space.lg * 2, Theme.layout.dialogMd, parent.width - Theme.space.xxl * 2)
    // A long message wraps and the toast grows upward.
    height: Math.max(Theme.control.heightLg, row.implicitHeight + Theme.space.md * 2)
    radius: Theme.radius.lg
    level: Theme.elevation.md
    opacity: shown ? 1 : 0
    visible: opacity > 0
    z: 1000

    transform: Translate {
        y: root.shown ? 0 : Theme.space.md + root.height * 0.15
        Behavior on y { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
    }
    Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }

    Accessible.role: Accessible.AlertMessage
    Accessible.name: label.text

    readonly property int shortTimeoutMs: 3000
    readonly property int longTimeoutMs: 6000

    Timer {
        id: hideTimer
        interval: root.tone === "danger" || root.actionText.length > 0 ? root.longTimeoutMs : root.shortTimeoutMs
        onTriggered: root.shown = false
    }

    HoverHandler {
        onHoveredChanged: {
            if (hovered)
                hideTimer.stop()
            else if (root.shown)
                hideTimer.restart()
        }
    }

    readonly property color toneColor: Theme.tone(root.tone === "danger" || root.tone === "success" ? root.tone : "accent").ink

    RowLayout {
        id: row
        anchors.centerIn: parent
        width: root.width - Theme.space.lg * 2
        spacing: Theme.space.sm

        MeshIcon {
            Layout.alignment: Qt.AlignVCenter
            iconName: root.tone === "danger" ? "alert-triangle" : root.tone === "success" ? "check" : "info"
            size: Theme.icon.mid
            color: root.toneColor
        }
        Copy {
            id: label
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            font.weight: Font.Medium
            wrapMode: Text.Wrap
            maximumLineCount: 4
            elide: Text.ElideRight
        }

        AbstractButton {
            id: actionButton
            visible: root.actionText.length > 0
            Layout.alignment: Qt.AlignVCenter
            hoverEnabled: true
            leftPadding: Theme.space.sm

            contentItem: Copy {
                text: root.actionText
                color: actionButton.hovered ? Theme.palette.textPrimary : Theme.palette.accentText
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
