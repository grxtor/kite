// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "Share as code" for one instance: what the code would carry (version, loader, how many mods and packs) and the
 * files it leaves out, then "Make code", then the code with Copy. InstanceExportFlows opens it with
 * openFor(id, name); ShareCodesController (`controller`) does the work.
 */
AppDialog {
    id: root

    property var controller: null
    property string instanceId
    property string instanceName
    // ShareManifest::toMap() once read, null before; `error` when the instance can't be shared.
    property var preview: null
    property string code: ""
    property bool making: false
    property string failure: ""

    readonly property bool ready: !!root.preview && !root.preview.error
    readonly property var skipped: root.ready ? root.preview.skipped : []
    readonly property int skippedRows: 5

    function openFor(id, name) {
        root.instanceId = id
        root.instanceName = name || ""
        root.preview = null
        root.code = ""
        root.making = false
        root.failure = ""
        root.open()
        if (root.controller && root.controller.available)
            root.controller.prepare(id)
    }
    function reasonText(reason) {
        return reason === "turnedOff" ? qsTr("Turned off") : qsTr("Not from Modrinth or CurseForge")
    }

    size: "md"
    title: qsTr("Share “%1” as a code").arg(root.ready && root.preview.name ? root.preview.name : root.instanceName)
    showCancel: root.code.length === 0 && root.ready
    primaryText: root.code.length > 0 || !root.ready ? qsTr("Done") : root.making ? qsTr("Making…") : qsTr("Make code")
    primaryEnabled: !root.making && (root.code.length > 0 || !!root.preview || !root.controller || !root.controller.available)
    closeOnPrimary: root.code.length > 0 || !root.ready
    onPrimaryClicked: {
        if (root.code.length > 0 || !root.ready)
            return
        root.making = true
        root.failure = ""
        root.controller.share(root.instanceId)
    }

    Connections {
        target: root.controller
        function onPrepared(id, preview) {
            if (id === root.instanceId)
                root.preview = preview
        }
        function onShared(id, code) {
            if (id === root.instanceId)
                root.code = code
        }
        function onActionFinished(action, ok, message) {
            if (action !== "share")
                return
            root.making = false
            if (!ok)
                root.failure = message
        }
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.lg

        Copy {
            Layout.fillWidth: true
            visible: !root.controller || !root.controller.available
            text: qsTr("Share codes go through Kite's friends server. Turn on Friends in Settings to make one.")
            tone: "secondary"
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            visible: !!root.controller && root.controller.available && !root.preview
            spacing: Theme.space.sm
            BusyIndicator { Layout.preferredWidth: Theme.icon.md; Layout.preferredHeight: Theme.icon.md; running: visible }
            Copy { text: qsTr("Reading the instance…"); tone: "secondary" }
        }

        Copy {
            Layout.fillWidth: true
            visible: !!root.preview && !!root.preview.error
            text: root.preview && root.preview.error ? root.preview.error : ""
            tone: "danger"
            wrapMode: Text.Wrap
        }

        ShareCodeSummary {
            Layout.fillWidth: true
            visible: root.ready
            showName: false
            info: root.ready ? root.preview : ({})
        }

        ShareCodeBox {
            Layout.fillWidth: true
            visible: root.code.length > 0
            code: root.code
        }

        Copy {
            Layout.fillWidth: true
            visible: root.ready
            text: root.code.length > 0
                  ? qsTr("Anyone with Kite can make this instance from the code for 90 days after it was last used. Your codes are in Settings › Friends.")
                  : qsTr("A friend who enters the code gets an instance with the same Minecraft version, loader and content, downloaded from Modrinth and CurseForge.")
            role: "label"
            tone: "tertiary"
            wrapMode: Text.Wrap
        }

        Copy {
            Layout.fillWidth: true
            visible: root.failure.length > 0
            text: root.failure
            role: "label"
            tone: "danger"
            wrapMode: Text.Wrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.skipped.length > 0 && root.code.length === 0
            spacing: Theme.space.xs

            Copy {
                text: root.skipped.length === 1 ? qsTr("1 file won't be included")
                                                : qsTr("%1 files won't be included").arg(root.skipped.length)
                role: "label"
                font.weight: Font.DemiBold
            }
            ListView {
                id: skippedList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(skippedList.contentHeight,
                                                 root.skippedRows * (Theme.control.heightSm + Theme.space.xxs))
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: root.skipped
                ScrollBar.vertical: ScrollBar {}
                delegate: RowLayout {
                    required property var modelData
                    width: skippedList.width
                    height: Theme.control.heightSm
                    spacing: Theme.space.md
                    Copy {
                        Layout.fillWidth: true
                        text: modelData.name
                        textFormat: Text.PlainText
                        role: "label"
                        elide: Text.ElideRight
                    }
                    Copy {
                        text: root.reasonText(modelData.reason)
                        role: "caption"
                        tone: "tertiary"
                    }
                }
            }
        }
    }
}
