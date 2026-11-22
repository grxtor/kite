// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * A project's gallery as one row of pictures that scrolls sideways and settles on a picture's edge. When there is
 * more than fits, arrow buttons stand on the side that has more and a count in the corner says how far along the
 * row is ("4 / 12"). A click asks for that picture large (opened(index)); the host shows it in a GalleryViewer.
 */
Item {
    id: root

    // [{ url, title? }]
    property var images: []
    property int thumbWidth: 220
    // The colour behind the strip, for the pictures' rounded corners.
    property color matte: Theme.palette.canvas
    signal opened(int index)

    readonly property int thumbHeight: Math.round(root.thumbWidth * 9 / 16)
    readonly property bool overflows: list.contentWidth > list.width + 1
    // The last picture fully in view, counted from one.
    readonly property int reached: {
        const step = root.thumbWidth + list.spacing
        return Math.max(1, Math.min(list.count, Math.floor((list.contentX + list.width + list.spacing + 1) / step)))
    }

    implicitHeight: root.thumbHeight

    function page(direction) {
        const step = root.thumbWidth + list.spacing
        const perPage = Math.max(1, Math.floor((list.width + list.spacing) / step))
        const first = Math.round(list.contentX / step)
        const target = Math.max(0, Math.min(list.count - 1, first + direction * perPage))
        slide.to = Math.max(0, Math.min(list.contentWidth - list.width, target * step))
        slide.restart()
    }
    NumberAnimation {
        id: slide
        target: list
        property: "contentX"
        duration: Theme.motion.normal
        easing.type: Theme.motion.easing
    }

    ListView {
        id: list
        anchors.fill: parent
        orientation: ListView.Horizontal
        spacing: Theme.space.sm
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        snapMode: ListView.SnapToItem
        model: root.images

        delegate: AbstractButton {
            id: shot
            required property var modelData
            required property int index
            width: root.thumbWidth
            height: root.thumbHeight
            hoverEnabled: true
            Accessible.name: shot.modelData.title && shot.modelData.title.length > 0
                             ? shot.modelData.title : qsTr("Picture %1").arg(shot.index + 1)
            onClicked: root.opened(shot.index)

            background: Rectangle {
                radius: Theme.radius.md
                color: Theme.palette.surfaceSunken
            }
            contentItem: Item {
                Image {
                    anchors.fill: parent
                    source: shot.modelData.url || ""
                    sourceSize: Qt.size(width * 2, height * 2)
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    mipmap: true
                    asynchronous: true
                }
                Rectangle {
                    anchors.fill: parent
                    visible: shot.hovered || shot.visualFocus
                    color: Theme.palette.hoverOverlay
                }
                CornerMatte {
                    cornerRadius: Theme.radius.md
                    matte: root.matte
                    outline: shot.visualFocus ? Theme.palette.textPrimary : Theme.palette.border
                }
            }
        }
    }

    IconButton {
        anchors.left: parent.left
        anchors.leftMargin: Theme.space.sm
        anchors.verticalCenter: parent.verticalCenter
        visible: root.overflows && !list.atXBeginning
        flat: false
        iconName: "chevron-left"
        tip: qsTr("Previous")
        onClicked: root.page(-1)
        // Over a picture: its own opaque ground instead of an outline on the photo.
        background: Rectangle {
            radius: Theme.radius.md
            color: parent.hovered ? Theme.palette.surfaceHover : Theme.palette.surface
            border.width: 1
            border.color: Theme.palette.border
        }
    }
    IconButton {
        anchors.right: parent.right
        anchors.rightMargin: Theme.space.sm
        anchors.verticalCenter: parent.verticalCenter
        visible: root.overflows && !list.atXEnd
        flat: false
        iconName: "chevron-right"
        tip: qsTr("Next")
        onClicked: root.page(1)
        background: Rectangle {
            radius: Theme.radius.md
            color: parent.hovered ? Theme.palette.surfaceHover : Theme.palette.surface
            border.width: 1
            border.color: Theme.palette.border
        }
    }

    Tag {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.space.sm
        visible: root.overflows
        onMedia: true
        text: qsTr("%1 / %2").arg(root.reached).arg(list.count)
    }
}
