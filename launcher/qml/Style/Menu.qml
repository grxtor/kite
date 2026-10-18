// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

T.Menu {
    id: control

    // As wide as its widest shown entry: never narrower than a menu always
    // was, so a short one does not shrink to a tooltip, and never wider than
    // the ceiling, past which an entry elides rather than the menu running
    // across the window. A `width` set on a menu still wins.
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

    implicitWidth: Math.min(control.maximumWidth,
                            Math.max(control.minimumWidth,
                                     implicitBackgroundWidth + leftInset + rightInset,
                                     control.widestEntry + leftPadding + rightPadding))
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    // Never flush against the window's edge: a menu opened near it is
    // pushed in by this much.
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

    // The popup glass treatment (Theme.glass): a barely see-through
    // surfaceOverlay, the hairline border, a light top edge and the md
    // shadow.
    background: GlassSurface {
        radius: Theme.radius.lg
        level: Theme.elevation.md
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
    }

    T.Overlay.modal: Rectangle {
        color: Theme.palette.scrim
    }

    T.Overlay.modeless: Rectangle {
        color: "transparent"
    }
}
