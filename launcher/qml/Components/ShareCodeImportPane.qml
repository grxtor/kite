// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * The New instance dialog's "Share code" tab: a field for the code a friend shared (case, spaces and dashes do not
 * matter), looked up as soon as it can be one, and what it holds: the name, Minecraft version, loader and how many
 * mods and packs. NewInstanceDialog's Create makes the instance (ShareCodesController.createInstance()).
 */
ColumnLayout {
    id: root

    // QmlShell.shareCodes (ShareCodesController), or null.
    property var controller: null
    property bool locked: false
    // ShareCodes::toMap() of the code found, or null.
    property var preview: null
    property bool lookingUp: false
    // ShareContent::contentNames() for the code shown, [] until the platforms answer.
    property var contents: []
    property bool contentsShown: false
    readonly property var kindNames: ({ mod: qsTr("Mod"), resourcepack: qsTr("Resource pack"),
                                        shaderpack: qsTr("Shader pack"), datapack: qsTr("Data pack") })
    readonly property int contentRows: 8
    property string error: ""
    readonly property bool available: !!root.controller && root.controller.available
    readonly property string typedCode: root.controller ? root.controller.display(field.text) : ""
    readonly property int lookUpDelayMs: 400

    function reset() {
        field.text = ""
        root.preview = null
        root.contents = []
        root.contentsShown = false
        root.error = ""
        root.lookingUp = false
    }
    function enter(code) {
        field.text = code
        root.lookUp()
    }
    function lookUp() {
        if (root.typedCode.length === 0 || root.lookingUp)
            return
        root.lookingUp = true
        root.error = ""
        root.controller.lookUp(root.typedCode)
    }

    spacing: Theme.space.lg

    Connections {
        target: root.controller
        function onLookedUp(preview) {
            root.preview = preview
            root.contents = []
        }
        function onContentNamed(code, items) {
            if (root.preview && root.preview.code === code)
                root.contents = items
        }
        function onActionFinished(action, ok, message) {
            if (action !== "lookUp")
                return
            root.lookingUp = false
            if (!ok) {
                root.preview = null
                root.error = message
            }
        }
    }
    Timer {
        id: lookUpTimer
        interval: root.lookUpDelayMs
        onTriggered: root.lookUp()
    }

    Copy {
        Layout.fillWidth: true
        text: root.available ? qsTr("Enter the code a friend shared with you, like KITE-7F3K-9QXM. Kite makes the same instance and downloads its mods and packs from Modrinth and CurseForge.")
                             : qsTr("Share codes go through Kite's friends server. Turn on Friends in Settings to use one.")
        tone: "secondary"
        wrapMode: Text.Wrap
    }

    RowLayout {
        Layout.fillWidth: true
        visible: root.available
        spacing: Theme.space.sm

        TextField {
            id: field
            Layout.fillWidth: true
            enabled: !root.locked
            selectByMouse: true
            placeholderText: "KITE-XXXX-XXXX"
            font.family: Theme.font.mono
            inputMethodHints: Qt.ImhUppercaseOnly | Qt.ImhNoPredictiveText
            Accessible.name: qsTr("Share code")
            onTextEdited: {
                root.error = ""
                if (root.preview && root.preview.code !== root.typedCode)
                    root.preview = null
                if (root.typedCode.length > 0 && !root.preview)
                    lookUpTimer.restart()
            }
            onAccepted: root.lookUp()
        }
        Button {
            enabled: !root.locked && !root.lookingUp && root.typedCode.length > 0
            text: root.lookingUp ? qsTr("Looking up…") : qsTr("Look up")
            onClicked: root.lookUp()
        }
    }

    Copy {
        Layout.fillWidth: true
        visible: root.error.length > 0
        text: root.error
        role: "label"
        tone: "danger"
        wrapMode: Text.Wrap
    }

    BoxSurface {
        Layout.fillWidth: true
        visible: !!root.preview
        implicitHeight: found.implicitHeight + Theme.space.lg * 2
        radius: Theme.radius.md

        RowLayout {
            id: found
            anchors.fill: parent
            anchors.margins: Theme.space.lg
            spacing: Theme.space.md

            MeshIcon {
                Layout.alignment: Qt.AlignTop
                iconName: "package"
                size: Theme.icon.lg
                color: Theme.palette.textSecondary
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.space.xs

                ShareCodeSummary {
                    Layout.fillWidth: true
                    info: root.preview || ({})
                }
                // Its text, not the box it pulls out for the chevron, lines up with the lines above.
                LinkButton {
                    id: listLink
                    Layout.leftMargin: -listLink.chevronBearing
                    visible: root.contents.length > 0
                    text: root.contentsShown ? qsTr("Hide the list") : qsTr("See what's in it")
                    onClicked: root.contentsShown = !root.contentsShown
                }
                ListView {
                    id: contentList
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentList.contentHeight,
                                                     root.contentRows * Theme.control.heightSm)
                    visible: root.contentsShown
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: root.contentsShown ? root.contents : []
                    ScrollBar.vertical: ScrollBar {}
                    delegate: RowLayout {
                        required property var modelData
                        width: contentList.width
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
                            text: root.kindNames[modelData.kind] || ""
                            role: "caption"
                            tone: "tertiary"
                        }
                    }
                }
            }
            Copy {
                Layout.alignment: Qt.AlignTop
                text: root.preview ? root.preview.code : ""
                font.family: Theme.font.mono
                role: "label"
                tone: "tertiary"
            }
        }
    }

    Item { Layout.fillHeight: true }
}
