// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as NativeDialogs
import MeshMC.Theme

/*
 * Answers the questions the core asks while it works -- "overwrite this?",
 * "which of these?", "these mods must be downloaded by hand" -- in the
 * shell's own style instead of widget message boxes. `host` is the
 * shell's QmlUiHost; whenever it has a pending request, this opens on it,
 * and closing it any way other than a button counts as "no".
 *
 * The root is a plain Item, not the Dialog itself: kind == "filePicker"
 * answers through a native QtQuick.Dialogs file dialog instead of this
 * component's own chrome (there is no sensible way to draw an OS file
 * picker inside a themed Dialog body), so the two live as siblings here,
 * both reacting to the same `host`.
 */
Item {
    id: wrapper

    property var host: null

    Dialog {
    id: root

    readonly property var request: wrapper.host ? wrapper.host.current : null
    readonly property string kind: request ? request.kind : ""

    function severityIcon(severity) {
        switch (severity) {
        case "error": return "alert-triangle"
        case "warning": return "alert-triangle"
        case "question": return "info"
        default: return "info"
        }
    }
    function severityColor(severity) {
        switch (severity) {
        case "error": return Theme.palette.danger
        case "warning": return Theme.palette.warning
        default: return Theme.palette.accent
        }
    }

    readonly property bool allFound: {
        if (!request || kind !== "blockedMods")
            return false
        var mods = request.blockedMods
        for (var i = 0; i < mods.length; ++i)
            if (!mods[i].found)
                return false
        return true
    }

    // Set before a button answers, so onClosed does not answer twice.
    property bool answered: false

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(kind === "blockedMods" || kind === "untrustedMods" || kind === "update" ? 620 : 460,
                    parent ? parent.width - Theme.space.xxl * 2 : 460)
    modal: true
    closePolicy: Popup.CloseOnEscape
    title: request ? request.title : ""

    header: DialogHeader {
        title: root.title
        icon: root.request ? root.severityIcon(root.request.severity) : ""
        iconColor: root.request ? root.severityColor(root.request.severity) : Theme.palette.accent
    }

    onRequestChanged: {
        // filePicker answers through the native FileDialog below instead
        // of this Dialog's own body -- see the file-level comment.
        if (kind === "filePicker") {
            return
        }
        if (request) {
            answered = false
            open()
        } else if (opened) {
            close()
        }
    }
    onClosed: {
        if (request && !answered) {
            answered = true
            request.reject()
        }
    }

    function answer(fn) {
        root.answered = true
        fn()
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        // The severity icon now leads the header badge instead of sitting
        // beside the body text, so this is just the text at full width.
        Text {
            Layout.fillWidth: true
            text: root.request ? root.request.text : ""
            textFormat: Text.AutoText
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            linkColor: Theme.palette.accent
            font.family: Theme.font.family
            font.pixelSize: Theme.type.body.pixelSize
            lineHeight: 1.3
            onLinkActivated: (link) => Qt.openUrlExternally(link)
        }

        // text: one line to type, prefilled with the suggested answer.
        TextField {
            id: textAnswer
            Layout.fillWidth: true
            visible: root.kind === "text"
            selectByMouse: true
            onAccepted: if (text.trim().length > 0) root.answer(() => root.request.accept(text.trim()))
            Connections {
                target: root
                function onRequestChanged() {
                    if (root.kind === "text") {
                        textAnswer.text = root.request.value || ""
                        textAnswer.forceActiveFocus()
                        textAnswer.selectAll()
                    }
                }
            }
        }

        // Blocked mods: each file, whether it is already downloaded, and
        // a way to its download page. The folder is watched in C++.
        Column {
            Layout.fillWidth: true
            visible: root.kind === "blockedMods"
            spacing: Theme.space.xs
            Repeater {
                model: root.kind === "blockedMods" && root.request ? root.request.blockedMods : []
                delegate: Rectangle {
                    required property var modelData
                    required property int index
                    width: parent.width
                    height: Theme.control.heightLg
                    radius: Theme.radius.md
                    color: Theme.palette.surfaceRaised
                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.space.md
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.space.sm
                        MeshIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            iconName: modelData.found ? "check" : "download"
                            size: Theme.icon.sm
                            color: modelData.found ? Theme.palette.success : Theme.palette.textTertiary
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.fileName
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.label.pixelSize
                        }
                    }
                    Button {
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.space.xs
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !modelData.found
                        flat: true
                        text: qsTr("Download")
                        icon.source: Icons.url("external-link")
                        onClicked: root.request.openDownload(index)
                    }
                }
            }
            Button {
                flat: true
                text: qsTr("Check again")
                icon.source: Icons.url("refresh")
                onClicked: root.request.rescanDownloads()
            }
        }

        // Untrusted files: what would be written, and a deliberate pause
        // before it can be accepted.
        Column {
            Layout.fillWidth: true
            visible: root.kind === "untrustedMods"
            spacing: Theme.space.sm
            Rectangle {
                width: parent.width
                height: Math.min(180, fileList.contentHeight + Theme.space.md)
                radius: Theme.radius.md
                color: Theme.palette.surfaceSunken
                ListView {
                    id: fileList
                    anchors.fill: parent
                    anchors.margins: Theme.space.sm
                    clip: true
                    model: root.kind === "untrustedMods" && root.request ? root.request.untrustedModsFiles : []
                    delegate: Text {
                        required property string modelData
                        width: fileList.width
                        text: modelData
                        elide: Text.ElideMiddle
                        color: Theme.palette.textSecondary
                        font.family: Theme.font.mono
                        font.pixelSize: Theme.type.caption.pixelSize
                    }
                }
            }
            CheckBox {
                id: trustBox
                text: qsTr("I trust these files")
                enabled: !trustDelay.running
                Timer {
                    id: trustDelay
                    interval: root.request ? root.request.confirmDelayMs : 0
                }
                Connections {
                    target: root
                    function onRequestChanged() {
                        trustBox.checked = false
                        if (root.kind === "untrustedMods")
                            trustDelay.restart()
                    }
                }
            }
        }

        // Profile setup: a Microsoft account that owns Minecraft but has
        // never picked a username -- checked live against Mojang's API,
        // the way the widget ProfileSetupDialog does.
        Column {
            Layout.fillWidth: true
            visible: root.kind === "profileSetup"
            spacing: Theme.space.sm

            TextField {
                id: profileNameField
                width: parent.width
                placeholderText: qsTr("Username")
                selectByMouse: true
                enabled: !(root.request && root.request.profileSubmitting)
                onTextChanged: profileCheckDelay.restart()
                Timer {
                    id: profileCheckDelay
                    interval: 500
                    onTriggered: if (root.kind === "profileSetup" && root.request)
                                     root.request.checkProfileName(profileNameField.text.trim())
                }
                Connections {
                    target: root
                    function onRequestChanged() {
                        if (root.kind === "profileSetup") {
                            profileNameField.text = ""
                            profileNameField.forceActiveFocus()
                        }
                    }
                }
            }

            Row {
                spacing: Theme.space.xs
                visible: root.request && (root.request.profileNameStatus === "available"
                                          || root.request.profileNameError.length > 0)
                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: root.request && root.request.profileNameStatus === "available" ? "check" : "alert-triangle"
                    size: Theme.icon.sm
                    color: root.request && root.request.profileNameStatus === "available" ? Theme.palette.success : Theme.palette.danger
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.request ? (root.request.profileNameStatus === "available"
                                          ? qsTr("Available") : root.request.profileNameError) : ""
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
                }
            }
        }

        // Update: what changes, in the author's words.
        Column {
            Layout.fillWidth: true
            visible: root.kind === "update"
            spacing: Theme.space.sm
            Row {
                spacing: Theme.space.sm
                Tag {
                    text: root.request && root.request.updateInfo ? root.request.updateInfo.currentVersion || "" : ""
                }
                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "chevron-right"
                    size: Theme.icon.sm
                }
                Tag {
                    text: root.request && root.request.updateInfo ? root.request.updateInfo.availableVersion || "" : ""
                    iconName: "download"
                }
            }
            ScrollView {
                width: parent.width
                height: Math.min(260, notes.implicitHeight + Theme.space.md)
                visible: notes.text.length > 0
                Text {
                    id: notes
                    width: parent.width
                    text: root.request && root.request.updateInfo ? root.request.updateInfo.releaseNotes || "" : ""
                    textFormat: Text.MarkdownText
                    wrapMode: Text.Wrap
                    color: Theme.palette.textSecondary
                    linkColor: Theme.palette.accent
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                    onLinkActivated: (link) => Qt.openUrlExternally(link)
                }
            }
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.lg
        topPadding: 0

        // choose: one button per action, the first as the primary.
        Repeater {
            model: root.kind === "choose" && root.request ? root.request.actions : []
            delegate: Button {
                required property string modelData
                required property int index
                text: modelData
                highlighted: index === 0
                onClicked: root.answer(() => root.request.choose(index))
            }
        }

        // update: install / later / skip this version.
        Button {
            visible: root.kind === "update"
            highlighted: true
            text: qsTr("Install update")
            onClicked: root.answer(() => root.request.answerUpdate("install"))
        }
        Button {
            visible: root.kind === "update"
            flat: true
            text: qsTr("Skip this version")
            onClicked: root.answer(() => root.request.answerUpdate("skip"))
        }
        Button {
            visible: root.kind === "update"
            flat: true
            text: qsTr("Later")
            onClicked: root.answer(() => root.request.answerUpdate("later"))
        }

        // profileSetup: Create submits and, unlike every other kind, may
        // fail without answering the request at all (a taken name, a
        // server error) -- so this does not go through root.answer(), which
        // would mark it answered before the request is actually done; only
        // Cancel (below) ends the request from this footer for this kind.
        Button {
            visible: root.kind === "profileSetup"
            highlighted: true
            enabled: root.request && root.request.profileNameStatus === "available" && !root.request.profileSubmitting
            text: root.request && root.request.profileSubmitting ? qsTr("Creating…") : qsTr("Create")
            onClicked: root.request.submitProfileName(profileNameField.text.trim())
        }
        Button {
            visible: root.kind === "profileSetup"
            flat: true
            enabled: !(root.request && root.request.profileSubmitting)
            text: qsTr("Cancel")
            onClicked: root.answer(() => root.request.reject())
        }

        Button {
            visible: root.kind !== "choose" && root.kind !== "update" && root.kind !== "profileSetup" && root.kind !== "filePicker"
            enabled: root.kind === "blockedMods" ? root.allFound
                   : root.kind === "untrustedMods" ? trustBox.checked
                   : root.kind === "text" ? textAnswer.text.trim().length > 0 : true
            highlighted: true
            text: root.request && root.request.acceptLabel.length > 0 ? root.request.acceptLabel
                : root.kind === "message" ? qsTr("OK") : qsTr("Continue")
            onClicked: root.answer(() => root.kind === "text" ? root.request.accept(textAnswer.text.trim())
                                                              : root.request.accept())
        }
        Button {
            visible: root.kind !== "message" && root.kind !== "update" && root.kind !== "profileSetup" && root.kind !== "filePicker"
            flat: true
            text: root.request && root.request.rejectLabel.length > 0 ? root.request.rejectLabel : qsTr("Cancel")
            onClicked: root.answer(() => root.request.reject())
        }
    }
    }

    // filePicker: a plugin's open/save dialog, shown natively rather than
    // inside root's own themed body (see the file-level comment). Plain
    // accept()/reject() -- the same generic answer mechanism "text" uses --
    // so QmlUiHost needs no filePicker-specific invokables of its own.
    NativeDialogs.FileDialog {
        id: nativeFileDialog
        readonly property var request: wrapper.host ? wrapper.host.current : null

        fileMode: request && request.filePickerMode === "save" ? NativeDialogs.FileDialog.SaveFile
                                                                : NativeDialogs.FileDialog.OpenFile
        nameFilters: request && request.filePickerFilter.length > 0
                     ? request.filePickerFilter.split(";;") : [qsTr("All files (*)")]

        onAccepted: if (request) request.accept(selectedFile.toString())
        onRejected: if (request) request.reject()

        Connections {
            target: wrapper.host
            function onCurrentChanged() {
                var req = wrapper.host ? wrapper.host.current : null
                if (!req || req.kind !== "filePicker")
                    return
                // Set fresh before every open(), imperatively rather than as
                // a binding: FileDialog owns selectedFile once shown (the
                // user's pick lives there too), so a binding here would only
                // ever apply once.
                nativeFileDialog.selectedFile =
                    req.filePickerMode === "save"
                        ? Format.fileUrl(req.filePickerDefaultPath) : ""
                nativeFileDialog.open()
            }
        }
    }
}
