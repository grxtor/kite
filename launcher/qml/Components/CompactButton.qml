// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls

/*
 * A toolbar button that keeps only its icon in a narrow column, with the text as its tooltip.
 */
Button {
    property bool compact: false

    display: compact ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon
    ToolTip.visible: compact && hovered
    ToolTip.text: text
}
