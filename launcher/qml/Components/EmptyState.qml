// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * Centered "nothing here" placeholder. `iconName` draws the usual bare glyph; the illustration is a default-property
 * slot for a caller with a fuller drawing.
 */
Item {
    id: root

    property alias title: titleLabel.text
    property alias body: bodyLabel.text
    // A bare icon above the title; leave empty and drop a drawing in the slot for anything else.
    property string iconName: ""
    property color iconColor: Theme.palette.textTertiary
    property string actionText: ""
    property string actionIcon: ""
    property bool actionEnabled: true
    // Opt-in: fills the parent and settles the content near the top third instead of dead centre, for an empty
    // state in an otherwise empty, very tall scrolling column.
    property bool upperThird: false

    signal actionTriggered()

    default property alias illustration: illustrationSlot.data
    // Something under the text and over the action. Nothing there takes no room; `extraShown` hides it while it has
    // nothing to say (a child's own `visible` follows its parent's).
    property alias extra: extraSlot.data
    property bool extraShown: extraSlot.children.length > 0

    // A fixed reading width: the column is as wide as this item, so taking its width from the column made the body
    // wrap a word per line.
    readonly property int readingWidth: 320
    implicitWidth: root.upperThird && root.parent ? root.parent.width : root.readingWidth
    implicitHeight: root.upperThird && root.parent ? root.parent.height : column.implicitHeight

    Column {
        id: column
        anchors.horizontalCenter: parent.horizontalCenter
        // In the default mode the root is exactly the column's size, so y is 0 however a caller anchors the root.
        y: root.upperThird ? Math.max(0, Math.round(parent.height * 0.30 - height / 2))
                            : Math.round((parent.height - height) / 2)
        spacing: Theme.space.md
        width: Math.min(root.readingWidth, root.width)

        Item {
            id: illustrationSlot
            anchors.horizontalCenter: parent.horizontalCenter
            width: childrenRect.width
            height: childrenRect.height

            Loader {
                active: root.iconName.length > 0
                sourceComponent: MeshIcon {
                    iconName: root.iconName
                    size: Theme.icon.xl
                    color: root.iconColor
                }
            }
        }

        Copy {
            id: titleLabel
            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            role: "title"
        }

        Copy {
            id: bodyLabel
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            tone: "secondary"
            visible: text.length > 0
        }

        Item {
            id: extraSlot
            anchors.horizontalCenter: parent.horizontalCenter
            width: childrenRect.width
            height: childrenRect.height
            visible: root.extraShown
        }

        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: root.actionText
            highlighted: true
            icon.source: root.actionIcon.length > 0 ? Icons.url(root.actionIcon) : ""
            visible: root.actionText.length > 0
            enabled: root.actionEnabled
            onClicked: root.actionTriggered()
        }
    }
}
