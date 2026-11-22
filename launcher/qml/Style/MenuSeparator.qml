// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Templates as T
import Kite.Theme

T.MenuSeparator {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    // Inset like the rows' own text, so the line doesn't run wall to wall.
    padding: Theme.space.md - Theme.space.xxs
    verticalPadding: Theme.space.xs

    contentItem: Rectangle {
        implicitWidth: Theme.control.heightLg * 3
        implicitHeight: 1
        color: Theme.palette.divider
    }
}
