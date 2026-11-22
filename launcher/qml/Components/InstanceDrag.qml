// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Makes an instance's card or row draggable to another group of the library (InstanceSection): a chip with its icon
 * and name follows the pointer and carries the instance and its group to the section it is dropped on.
 */
Item {
    id: dragSource
    required property Item section
    property string instanceId
    property string name
    property string iconKey
    anchors.fill: parent
    // Under the card's content: on top, it took the ⋯ and Play clicks from the card's selection
    // tap. Its handler still sees presses that start on them.
    z: -1
    enabled: dragSource.section.dragEnabled

    DragHandler {
        id: dragger
        target: null
        dragThreshold: dragSource.section.dragThreshold
        // Takes the pointer from the card's taps and keeps it from the page's scrolling.
        grabPermissions: PointerHandler.CanTakeOverFromItems | PointerHandler.CanTakeOverFromHandlersOfDifferentType
        onActiveChanged: {
            if (active) {
                chipLoader.active = true
                chipLoader.item.Drag.start()
            } else if (chipLoader.item) {
                chipLoader.item.Drag.drop()
                chipLoader.active = false
            }
            dragSource.section.dragActiveChanged(active)
        }
        onCentroidChanged: if (active) dragSource.section.dragMoved(centroid.scenePosition.y)
    }

    // Built only while a drag is on; the chip lives in the window overlay.
    Loader {
        id: chipLoader
        active: false
        sourceComponent: Component {
            Rectangle {
                id: chip
                parent: Overlay.overlay
                readonly property string instanceId: dragSource.instanceId
                readonly property string group: dragSource.section.group
                x: dragger.centroid.scenePosition.x - width / 2
                y: dragger.centroid.scenePosition.y - height / 2
                width: chipRow.implicitWidth + Theme.space.md * 2
                height: Theme.control.height
                radius: Theme.radius.sm
                color: Theme.palette.surfaceOverlay
                border.width: 1
                border.color: Theme.palette.borderStrong
                Drag.keys: [dragSource.section.dragKey]
                Drag.hotSpot.x: width / 2
                Drag.hotSpot.y: height / 2

                Row {
                    id: chipRow
                    anchors.centerIn: parent
                    spacing: Theme.space.sm
                    Image {
                        anchors.verticalCenter: parent.verticalCenter
                        width: Theme.icon.md
                        height: Theme.icon.md
                        sourceSize.width: width * 2
                        sourceSize.height: height * 2
                        source: Icons.instanceIcon(dragSource.iconKey)
                        smooth: true
                        mipmap: true
                    }
                    Copy {
                        anchors.verticalCenter: parent.verticalCenter
                        text: dragSource.name
                        font.weight: Font.Medium
                    }
                }
            }
        }
    }
}
