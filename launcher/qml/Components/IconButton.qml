// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * A square, quiet button that is only an icon, with its label as a tooltip
 * and as the accessible name -- an icon alone says nothing to a screen
 * reader.
 */
Button {
    id: control

    property string iconName
    property string tip
    property int size: Theme.control.height

    flat: true
    display: AbstractButton.IconOnly
    icon.source: iconName.length > 0 ? Icons.url(iconName) : ""
    icon.width: Theme.icon.sm + 2
    icon.height: Theme.icon.sm + 2
    implicitWidth: size
    implicitHeight: size
    padding: 0

    Accessible.name: tip

    ToolTip.visible: tip.length > 0 && hovered
    ToolTip.delay: 500
    ToolTip.text: tip
}
