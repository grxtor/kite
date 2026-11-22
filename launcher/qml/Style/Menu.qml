// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

T.Menu {
    id: control

    // As wide as its widest shown entry, but not so narrow that a short menu
    // shrinks to a tooltip, nor wider than the ceiling, past which an entry
    // elides. A `width` set on a menu still wins.
    readonly property real minimumWidth: Theme.control.heightLg * 4
    readonly property real maximumWidth: Theme.control.heightLg * 8
    // Hidden entries (an item zeroed out with visible: false) do not count.
    readonly property real widestEntry: {
        var widest = 0
        for (var i = 0; i < control.count; ++i) {
            var item = control.itemAt(i)
            if (item && item.visible)
                widest = Math.max(widest, item.implicitWidth)
        }
        return widest
    }

    // A whole number of pixels: entries' text widths are fractional, and a menu hung by its right edge
    // (x = anchor - width) then sat on a half pixel and visibly moved over by one when it settled.
    implicitWidth: Math.ceil(Math.min(control.maximumWidth,
                                      Math.max(control.minimumWidth,
                                               implicitBackgroundWidth + leftInset + rightInset,
                                               control.widestEntry + leftPadding + rightPadding)))
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    // A menu opened near the window's edge is pushed in by this much.
    margins: Theme.space.sm
    overlap: 1
    padding: Theme.space.xs

    delegate: MenuItem { }

    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        interactive: Window.window
                     ? contentHeight + control.topPadding + control.bottomPadding > control.height
                     : false
        clip: true
        currentIndex: control.currentIndex

        T.ScrollIndicator.vertical: ScrollIndicator { }
    }

    // The popup surface (Theme.glass): surfaceOverlay, the hairline border and the md shadow. Flat: no sheen
    // and no light top edge.
    background: GlassSurface {
        radius: Theme.radius.sm
        level: Theme.elevation.md
    }

    // A menu is under the pointer the moment it is asked for: a quick fade and nothing else. Growing and
    // overshooting from its top centre made a menu hung from its right corner seem to arrive from the side.
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: Theme.motion.press; easing.type: Theme.motion.easing }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
    }

    T.Overlay.modal: Rectangle {
        color: Theme.palette.scrim
        Behavior on opacity { NumberAnimation { duration: Theme.motion.normal; easing.type: Theme.motion.easing } }
    }

    T.Overlay.modeless: Rectangle {
        color: "transparent"
    }
}
