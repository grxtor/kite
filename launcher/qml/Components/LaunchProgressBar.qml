// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import MeshMC.Theme

/*
 * A thin bar for a launch in progress. `progress` 0..1 fills it; a negative
 * value means "busy, no idea how far" and runs a sliding segment instead.
 */
Rectangle {
    id: root

    property real progress: -1
    property bool onMedia: false
    readonly property bool indeterminate: progress < 0

    implicitHeight: 4
    radius: height / 2
    clip: true
    color: onMedia ? Qt.rgba(1, 1, 1, 0.18) : Theme.palette.surfaceOverlay

    Rectangle {
        id: fill
        height: parent.height
        radius: parent.radius
        color: Theme.palette.accent
        width: root.indeterminate ? parent.width * 0.3 : parent.width * Math.max(0, Math.min(1, root.progress))
        x: root.indeterminate ? slide.position : 0
        Behavior on width {
            enabled: !root.indeterminate
            NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing }
        }
    }

    QtObject {
        id: slide
        property real position: -fill.width
    }

    NumberAnimation {
        target: slide
        property: "position"
        from: -fill.width
        to: root.width
        duration: 1100
        easing.type: Easing.InOutQuad
        loops: Animation.Infinite
        running: root.indeterminate && root.visible
    }
}
