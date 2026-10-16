// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import MeshMC.Theme

/*
 * Says why a control is unavailable. A disabled control gets no hover of
 * its own, so its tooltip can never show; wrapped in this, hovering it
 * shows `reason` while that is non-empty. The wrapped control keeps its
 * own size, and this takes the same.
 */
Item {
    id: root

    property string reason: ""
    default property alias content: holder.data
    readonly property Item control: holder.children.length > 0 ? holder.children[0] : null

    implicitWidth: control ? control.implicitWidth : 0
    implicitHeight: control ? control.implicitHeight : 0

    Item {
        id: holder
        anchors.fill: parent
    }

    HoverHandler { id: hover }

    ToolTip.visible: hover.hovered && root.reason.length > 0
    ToolTip.delay: 400
    ToolTip.text: root.reason
}
