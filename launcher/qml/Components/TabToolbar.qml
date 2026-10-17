// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * The one toolbar every instance tab shares (Worlds, Data packs, Servers,
 * Backups, Version, Screenshots), so the tabs read as one set:
 *
 *     [ leading ........................ ] [⟳] [Open folder] [More ▾] [Add ...]
 *
 * Left: whatever narrows or picks what the tab shows (a world picker, a
 * search box) -- or nothing. Right: the plain actions, then the "More"
 * menu if the tab has rarer ones, and the tab's own "add" action last, so
 * its edge is the content's right edge on every tab. The play bar's Play
 * is the page's one primary button, so none of these is highlighted.
 *
 * Items declared inside go to the right-hand group; items for the left
 * go in `leading`. Both stay one control tall.
 *
 *     TabToolbar {
 *         Layout.fillWidth: true
 *         leading: ComboBox { ... }
 *         IconButton { ... }
 *         Button { text: qsTr("Open folder") ... }
 *         Button { text: qsTr("Add world") ... }
 *     }
 */
RowLayout {
    id: root

    default property alias actions: actionRow.data
    property alias leading: leadingRow.data

    spacing: Theme.space.sm

    RowLayout {
        id: leadingRow
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        // A nested layout's maximum is the sum of what it holds, so without
        // this its fixed-width children would stop it taking the free space
        // and the actions would crowd up against them.
        Layout.maximumWidth: Number.POSITIVE_INFINITY
        spacing: root.spacing
    }

    RowLayout {
        id: actionRow
        spacing: root.spacing
    }
}
