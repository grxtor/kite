// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

// No `footer:` is set: Basic wires one up as a DialogButtonBox, which this
// style does not style. A dialog that uses control.standardButtons falls back
// to the platform default.
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

    // Fades in, grows from popScale and rises `dialogRise` px into place,
    // settling with the pop easing; the fade never overshoots. The rise
    // is a Translate on the popup's own item (what contentItem, header and
    // footer sit in), not `y`, which the centring owns.
    Translate { id: rise }
    onAboutToShow: {
        const frame = control.contentItem ? control.contentItem.parent : null
        if (frame && frame.transform.length === 0)
            frame.transform = [rise]
    }
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: Theme.motion.slow; easing.type: Theme.motion.easing }
        NumberAnimation { property: "scale"; from: Theme.motion.popScale; to: 1.0; duration: Theme.motion.slow; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot }
        NumberAnimation { target: rise; property: "y"; from: Theme.motion.dialogRise; to: 0; duration: Theme.motion.slow; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
    }

    // The popup glass treatment at the largest corner and the deepest shadow.
    background: GlassSurface {
        radius: Theme.radius.xl
        level: Theme.elevation.lg
    }

    header: Label {
        text: control.title
        visible: parent?.parent === T.Overlay.overlay && control.title
        elide: Label.ElideRight
        font.pixelSize: Theme.type.subheading.pixelSize
        font.family: Theme.font.heading
        font.weight: Theme.type.subheading.weight
        padding: Theme.space.xl
        bottomPadding: 0
    }

    T.Overlay.modal: ModalScrim {}

    T.Overlay.modeless: Rectangle {
        color: "transparent"
    }
}
