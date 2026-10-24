// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * The mods a Kite Performance instance gets, as a read-only row of their
 * icons with the name as a tooltip: the ones the setup installs by default
 * (BundleCatalog::mods() rows with isDefault), so the user sees what is
 * coming before pressing Create. Wraps onto more lines in a narrow dialog.
 */
Flow {
    id: root

    // BundleCatalog::mods() rows; only the default ones are shown.
    property var mods: []

    readonly property int iconSize: Theme.control.height
    readonly property var shown: root.mods.filter((mod) => mod.isDefault === true)

    spacing: Theme.space.sm

    Repeater {
        model: root.shown

        RowThumb {
            id: thumb
            required property var modelData

            width: root.iconSize
            height: root.iconSize
            source: thumb.modelData.icon || ""

            Accessible.role: Accessible.StaticText
            Accessible.name: thumb.modelData.name

            HoverHandler { id: hover }
            ToolTip.visible: hover.hovered
            ToolTip.delay: 400
            ToolTip.text: thumb.modelData.name
        }
    }
}
