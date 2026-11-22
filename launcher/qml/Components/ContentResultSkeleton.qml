// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import Kite.Theme

/*
 * Placeholder rows for ContentBrowserView while the first page of a search loads: `rows` of them, each as tall as a
 * result (`rowHeight`, ContentResultRow's) with its logo square and its lines.
 */
Column {
    id: root

    property int rows: 5
    property int rowHeight: 92
    readonly property int logoSize: 40

    Repeater {
        model: root.rows
        delegate: Item {
            width: root.width
            height: root.rowHeight
            Skeleton {
                id: logoBlock
                x: Theme.space.lg
                anchors.verticalCenter: parent.verticalCenter
                width: root.logoSize
                height: root.logoSize
                radius: Theme.radius.sm
            }
            Column {
                anchors.left: logoBlock.right
                anchors.leftMargin: Theme.space.md
                anchors.right: parent.right
                anchors.rightMargin: Theme.space.lg
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space.sm
                Skeleton { width: parent.width * 0.3; height: Theme.type.body.pixelSize; radius: Theme.radius.xs }
                Skeleton { width: parent.width * 0.7; height: Theme.type.label.pixelSize; radius: Theme.radius.xs }
                Skeleton { width: parent.width * 0.4; height: Theme.type.caption.pixelSize; radius: Theme.radius.xs }
            }
        }
    }
}
