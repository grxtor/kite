// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import MeshMC.Theme

// Frame stays transparent and only outlines its content -- for a filled
// surface, use Pane instead. Keeping the two distinct is what lets "surfaces
// separated by lightness" actually mean something: a Pane inside a Pane
// reads as a step up in lightness, a Frame around either just groups it.
T.Frame {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    padding: Theme.space.lg

    background: Rectangle {
        radius: Theme.radius.lg
        color: "transparent"
        border.width: 1
        border.color: Theme.palette.border
    }
}
