// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/* A square, quiet icon-only button; its label is the tooltip and the accessible name. */
Button {
    id: control

    property string iconName
    property string tip
    property int size: Theme.control.height

    flat: true
    display: AbstractButton.IconOnly
    icon.source: iconName.length > 0 ? Icons.url(iconName) : ""
    icon.width: Theme.icon.mid
    icon.height: Theme.icon.mid
    implicitWidth: size
    implicitHeight: size
    padding: 0

    Accessible.name: tip

    ToolTip.visible: tip.length > 0 && hovered
    ToolTip.delay: 500
    ToolTip.text: tip
}
