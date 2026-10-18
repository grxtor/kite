// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// No `footer:` is set here: Basic's Dialog wires one up as a DialogButtonBox,
// but DialogButtonBox is not in this style's control list, so giving Dialog
// a footer would mean silently reaching for an unstyled control. A dialog
// that uses control.standardButtons will fall back to the platform default
// until DialogButtonBox is styled -- flagged in this style's report.
T.Dialog {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding,
                            implicitHeaderWidth,
                            implicitFooterWidth)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding
                             + (implicitHeaderHeight > 0 ? implicitHeaderHeight + spacing : 0)
                             + (implicitFooterHeight > 0 ? implicitFooterHeight + spacing : 0))

    padding: Theme.space.xl
    spacing: Theme.space.lg

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
        NumberAnimation { property: "scale"; from: 0.96; to: 1.0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
    }

    // The popup glass treatment at the largest corner and the deepest
    // shadow (Theme.elevation.lg).
    background: GlassSurface {
        radius: Theme.radius.xl
        level: Theme.elevation.lg
    }

    header: Label {
        text: control.title
        visible: parent?.parent === T.Overlay.overlay && control.title
        elide: Label.ElideRight
        font.pixelSize: Theme.type.title.pixelSize
        font.family: Theme.font.heading
        font.weight: Theme.type.title.weight
        padding: Theme.space.xl
        bottomPadding: 0
    }

    T.Overlay.modal: Rectangle {
        color: Theme.palette.scrim
    }

    T.Overlay.modeless: Rectangle {
        color: "transparent"
    }
}
