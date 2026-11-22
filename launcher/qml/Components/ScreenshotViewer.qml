// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * One screenshot large, over the whole window: Left/Right (or the arrows beside the picture, or a swipe) go through
 * the folder, Esc closes. Under the picture are its actions; they call back into the ScreenshotsTab (`owner`), so a
 * delete goes to the trash with Undo exactly as from the grid. Modal, so the window's shortcuts wait while it is open.
 */
Popup {
    id: root

    // The ScreenshotsTab: model, captionOf(), isCover(), copyImage(), askRemove(), upload() and the cover signals.
    required property var owner
    readonly property var model: owner.model
    readonly property int count: list.count
    readonly property int index: list.currentIndex
    readonly property var current: list.currentItem
    readonly property string currentPath: current ? current.path : ""
    readonly property bool currentIsCover: {
        owner.coverSource
        return root.currentPath.length > 0 && owner.isCover(root.currentPath)
    }
    readonly property alias toast: viewerToast
    readonly property int barHeight: Theme.control.heightLg + Theme.space.lg * 2

    function show(row) {
        root.open()
        list.jumpTo(row)
    }
    function step(by) {
        const next = list.currentIndex + by
        if (next >= 0 && next < list.count)
            list.currentIndex = next
    }

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.CloseOnEscape
    // The last picture was deleted: nothing left to look at.
    onCountChanged: if (root.count === 0 && root.opened) root.close()

    // The viewer fades in while the picture grows into place with the pop easing; out is the quick fade.
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motion.normal; easing.type: Theme.motion.easing }
        NumberAnimation { target: list; property: "scale"; from: Theme.motion.popScale; to: 1; duration: Theme.motion.slow; easing.type: Theme.motion.popEasing; easing.overshoot: Theme.motion.popOvershoot }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; to: 0; duration: Theme.motion.fast; easing.type: Theme.motion.easing }
    }
    Overlay.modal: Item {}

    background: Rectangle { color: Theme.palette.canvas }

    contentItem: FocusScope {
        focus: true
        Keys.onLeftPressed: root.step(-1)
        Keys.onRightPressed: root.step(1)

        RowLayout {
            id: topBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: Theme.space.xl
            anchors.rightMargin: Theme.space.lg
            height: root.barHeight
            spacing: Theme.space.md

            Column {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: Theme.space.xxs
                Copy {
                    width: parent.width
                    text: root.current ? owner.captionOf(root.current.name) : ""
                    elide: Text.ElideRight
                    role: "title"
                }
                Copy {
                    width: parent.width
                    text: root.current ? root.current.name : ""
                    elide: Text.ElideMiddle
                    role: "label"
                    tone: "secondary"
                }
            }
            Copy {
                Layout.alignment: Qt.AlignVCenter
                visible: root.count > 1
                text: qsTr("%1 of %2").arg(root.index + 1).arg(root.count)
                role: "label"
                tone: "tertiary"
            }
            IconButton {
                Layout.alignment: Qt.AlignVCenter
                iconName: "x"
                tip: qsTr("Close")
                onClicked: root.close()
            }
        }

        ListView {
            id: list
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: topBar.bottom
            anchors.bottom: actions.top
            clip: true
            orientation: ListView.Horizontal
            snapMode: ListView.SnapOneItem
            highlightRangeMode: ListView.StrictlyEnforceRange
            highlightMoveDuration: Theme.motion.normal
            boundsBehavior: Flickable.StopAtBounds
            // Only while open: a closed viewer decodes nothing.
            model: root.visible ? root.model : null

            // Without the slide: the viewer opens on the picture that was clicked.
            function jumpTo(row) {
                const slide = highlightMoveDuration
                highlightMoveDuration = 0
                currentIndex = Math.max(0, Math.min(row, count - 1))
                positionViewAtIndex(currentIndex, ListView.Beginning)
                highlightMoveDuration = slide
            }

            delegate: Item {
                id: page
                required property int index
                required property string name
                required property string path
                required property string url

                width: list.width
                height: list.height

                // The grid's thumbnail until the file itself is decoded.
                Image {
                    anchors.fill: full
                    visible: full.status !== Image.Ready
                    source: "image://screenshot/" + encodeURIComponent(page.path)
                    sourceSize: Qt.size(512, 512)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                }
                Image {
                    id: full
                    anchors.fill: parent
                    anchors.leftMargin: Theme.control.heightLg + Theme.space.xl * 2
                    anchors.rightMargin: Theme.control.heightLg + Theme.space.xl * 2
                    source: page.url
                    sourceSize: Qt.size(width * 2, height * 2)
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    mipmap: true
                }
            }
        }

        IconButton {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space.xl
            anchors.verticalCenter: list.verticalCenter
            size: Theme.control.heightLg
            flat: false
            visible: root.count > 1
            enabled: root.index > 0
            iconName: "chevron-left"
            tip: qsTr("Previous")
            onClicked: root.step(-1)
        }
        IconButton {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space.xl
            anchors.verticalCenter: list.verticalCenter
            size: Theme.control.heightLg
            flat: false
            visible: root.count > 1
            enabled: root.index < root.count - 1
            iconName: "chevron-right"
            tip: qsTr("Next")
            onClicked: root.step(1)
        }

        Row {
            id: actions
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            height: root.barHeight
            spacing: Theme.space.sm

            Button {
                anchors.verticalCenter: parent.verticalCenter
                // The Gece look shows no covers.
                visible: Theme.look !== "night"
                text: root.currentIsCover ? qsTr("Remove cover") : qsTr("Set as cover")
                icon.source: Icons.url("image")
                onClicked: {
                    if (root.currentIsCover) {
                        owner.coverCleared()
                    } else {
                        owner.coverRequested(root.currentPath)
                        viewerToast.show(qsTr("Cover set."), "success")
                    }
                }
            }
            Button {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Copy")
                icon.source: Icons.url("copy")
                onClicked: owner.copyImage(root.index)
            }
            Button {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Show in folder")
                icon.source: Icons.url("folder")
                onClicked: owner.openFolderRequested(owner.directory)
            }
            Button {
                anchors.verticalCenter: parent.verticalCenter
                visible: owner.canUpload
                enabled: !!owner.uploader && !owner.uploader.busy
                text: owner.uploading ? qsTr("Uploading…") : qsTr("Upload to Imgur")
                icon.source: Icons.url("arrow-up")
                onClicked: owner.upload([root.currentPath])
            }
            Button {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Delete")
                icon.source: Icons.url("trash")
                onClicked: owner.askRemove([root.currentPath])
            }
        }

        Toast { id: viewerToast }
    }
}
