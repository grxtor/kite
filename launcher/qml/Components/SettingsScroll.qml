// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

// One settings section: a scrolling column of groups under a heading,
// kept to a readable width on wide windows.
Flickable {
    id: root

    property string title
    default property alias groups: column.data

    contentWidth: width
    contentHeight: column.height + Theme.space.xxl
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    Accessible.name: title

    ScrollBar.vertical: ScrollBar {}

    Column {
        id: column
        width: Math.min(root.width - Theme.space.md, 760)
        y: Theme.space.xs
        spacing: Theme.space.xl
    }
}
