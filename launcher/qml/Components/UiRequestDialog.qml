// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs as NativeDialogs
import Kite.Theme

/*
 * Answers the core's questions ("overwrite this?", "which account?", ...) in
 * the shell's style. The root is a plain Item: kind == "filePicker" answers
 * through a native file dialog, a sibling of the themed Dialog. Closing any
 * way other than a button counts as "no".
 */
Item {
    id: wrapper

    property var host: null

    AppDialog {
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

    // kind == "update": four buttons, and room for the release notes.
    size: kind === "update" ? "lg"
          : kind === "blockedMods" || kind === "untrustedMods" || kind === "modal" || kind === "optionalMods" ? "md" : "sm"

    // kind == "update" (QmlUiRequest::setUpdateInfo): managed means a package manager installs it and Kite only
    // tells; installNow is off while a game started from Kite runs.
    readonly property var updateInfo: kind === "update" && request && request.updateInfo ? request.updateInfo : ({})
    readonly property bool updateManaged: (updateInfo.managedHint || "").length > 0
    readonly property bool updateInstallNow: !updateManaged && updateInfo.canInstallNow !== false
    title: request ? request.title : ""
    icon: root.request ? root.severityIcon(root.request.severity) : ""
    iconColor: root.request ? root.severityColor(root.request.severity) : Theme.palette.accent

    // Why the accepting button is off, for the kinds where that is not obvious.
    readonly property string primaryReason: {
        if (!request)
            return ""
        if (kind === "blockedMods")
            return allFound ? "" : qsTr("Continue turns on once every file has been downloaded.")
        if (kind === "untrustedMods")
            return trustBox.checked ? "" : trustDelay.running ? qsTr("Take a moment to read the list.")
                                                              : qsTr("Tick “I trust these files” to continue.")
        if (kind === "profileSetup") {
            if (request.profileSubmitting || request.profileNameStatus === "available")
                return ""
            if (profileNameField.text.trim().length === 0)
                return qsTr("Type the username you want to use.")
            return request.profileNameError.length === 0 ? qsTr("Checking that the username is free…") : ""
        }
        return ""
    }

    onRequestChanged: {
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

        // Plain, with no links: the core's text often names an instance, a pack or a version from someone else.
        // Line breaks are newlines.
        Copy {
            objectName: "requestText"
            Layout.fillWidth: true
            // A plugin modal brings its own text in its document.
            visible: text.length > 0
            text: root.request ? root.request.text : ""
            tone: "secondary"
            wrapMode: Text.Wrap
            lineHeight: 1.3
        }

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

        // Blocked mods: each file, whether downloaded, and a way to its download page; the folder
        // is watched in C++. Rows are hairline-separated on the dialog's own surface.
        Column {
            Layout.fillWidth: true
            visible: root.kind === "blockedMods"
            spacing: Theme.space.xs
            Column {
                width: parent.width
                Repeater {
                    model: root.kind === "blockedMods" && root.request ? root.request.blockedMods : []
                    delegate: Item {
                        id: blockedRow
                        required property var modelData
                        required property int index
                        width: parent.width
                        height: Theme.control.heightLg
                        Rectangle {
                            visible: blockedRow.index > 0
                            anchors.top: parent.top
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 1
                            color: Theme.palette.divider
                        }
                        Row {
                            anchors.left: parent.left
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: Theme.space.sm
                            MeshIcon {
                                anchors.verticalCenter: parent.verticalCenter
                                iconName: blockedRow.modelData.found ? "check" : "download"
                                size: Theme.icon.sm
                                color: blockedRow.modelData.found ? Theme.palette.success : Theme.palette.textTertiary
                            }
                            Copy {
                                anchors.verticalCenter: parent.verticalCenter
                                text: blockedRow.modelData.fileName
                                role: "label"
                            }
                        }
                        Button {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !blockedRow.modelData.found
                            flat: true
                            text: qsTr("Download")
                            icon.source: Icons.url("external-link")
                            onClicked: root.request.openDownload(blockedRow.index)
                        }
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

        Column {
            Layout.fillWidth: true
            visible: root.kind === "untrustedMods"
            spacing: Theme.space.sm
            ListView {
                id: fileList
                width: parent.width
                height: Math.min(180, contentHeight)
                clip: true
                model: root.kind === "untrustedMods" && root.request ? root.request.untrustedModsFiles : []
                ScrollBar.vertical: ScrollBar {}
                delegate: Item {
                    id: fileRow
                    required property int index
                    required property string modelData
                    width: fileList.width
                    height: Math.ceil(Theme.type.caption.lineHeightPx) + Theme.space.sm * 2
                    Rectangle {
                        visible: fileRow.index > 0
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: Theme.palette.divider
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.space.sm
                        anchors.verticalCenter: parent.verticalCenter
                        text: fileRow.modelData
                        textFormat: Text.PlainText
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

        // Profile setup: a Microsoft account that owns Minecraft but has no username yet, checked
        // live against Mojang's API.
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
                Copy {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.request ? (root.request.profileNameStatus === "available"
                                          ? qsTr("Available") : root.request.profileNameError) : ""
                    role: "caption"
                    tone: "secondary"
                }
            }
        }

        // pickAccount: which account to play with and whether it becomes the default;
        // double-clicking a row plays with it.
        Column {
            id: accountPicker
            Layout.fillWidth: true
            visible: root.kind === "pickAccount"
            spacing: Theme.space.xs

            property int selected: 0
            readonly property int faceSize: 32

            function play() {
                root.answer(() => root.request.chooseAccount(accountPicker.selected, makeDefaultBox.checked))
            }

            Connections {
                target: root
                function onRequestChanged() {
                    if (root.kind === "pickAccount") {
                        accountPicker.selected = 0
                        makeDefaultBox.checked = false
                    }
                }
            }

            Repeater {
                model: root.kind === "pickAccount" && root.request ? root.request.accountOptions : []
                delegate: AbstractButton {
                    id: accountRow
                    required property var modelData
                    required property int index
                    readonly property bool selected: accountPicker.selected === index

                    width: parent.width
                    implicitHeight: Theme.control.heightLg + Theme.space.sm
                    hoverEnabled: true
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData.name
                    Accessible.checked: accountRow.selected

                    onClicked: accountPicker.selected = index
                    onDoubleClicked: {
                        accountPicker.selected = index
                        accountPicker.play()
                    }

                    // The selected row is the tinted surface and a bare check, so it is never read as hover.
                    background: StateFill {
                        radius: Theme.radius.md
                        hovered: accountRow.hovered
                        active: accountRow.selected
                    }

                    contentItem: RowLayout {
                        spacing: Theme.space.sm

                        PlayerHead {
                            Layout.leftMargin: Theme.space.sm
                            size: accountPicker.faceSize
                            source: modelData.faceId ? "image://accountface/" + modelData.faceId : ""
                        }
                        Copy {
                            Layout.fillWidth: true
                            text: modelData.name
                            elide: Text.ElideRight
                            font.weight: Font.Medium
                        }
                        Copy {
                            text: modelData.kind === "Microsoft" ? qsTr("Microsoft") : qsTr("Offline")
                            role: "label"
                            tone: "tertiary"
                        }
                        MeshIcon {
                            Layout.rightMargin: Theme.space.md
                            iconName: "check"
                            size: Theme.icon.sm
                            color: Theme.palette.accentText
                            opacity: accountRow.selected ? 1 : 0
                        }
                    }
                }
            }

            CheckBox {
                id: makeDefaultBox
                leftPadding: 0
                text: qsTr("Make this the default account")
            }
        }

        // modal: a plugin's one-off prompt (UiHost::runPluginModal()); this request is the
        // PluginNode sink. Built afresh per request, since PluginNode inputs read their starting
        // values once.
        Loader {
            Layout.fillWidth: true
            active: root.kind === "modal" && !!root.request
            visible: active
            sourceComponent: PluginNode {
                surfaceId: "modal"
                sink: root.request
                node: root.request.modalDocument.root || root.request.modalDocument
            }
        }

        // optionalMods: an ATLauncher pack's extra mods, ticked on the request's
        // AtlOptionalModsModel, which applies the pack's rules. Hidden rows are libraries.
        Column {
            id: optionalModsBox
            Layout.fillWidth: true
            visible: root.kind === "optionalMods"
            spacing: Theme.space.sm

            readonly property var mods: root.kind === "optionalMods" && root.request ? root.request.optionalMods : null

            ListView {
                id: modsList
                width: parent.width
                height: Math.min(320, contentHeight)
                clip: true
                model: optionalModsBox.mods
                ScrollBar.vertical: ScrollBar {}
                delegate: Item {
                    id: modRow
                    required property int index
                    required property string name
                    required property string description
                    required property bool selected
                    required property bool hidden
                    width: ListView.view ? ListView.view.width : 0
                    visible: !modRow.hidden
                    height: visible ? modColumn.implicitHeight + Theme.space.xs * 2 : 0
                    Column {
                        id: modColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        CheckBox {
                            text: modRow.name
                            checked: modRow.selected
                            onToggled: modsList.model.toggle(modRow.index)
                        }
                        Copy {
                            width: parent.width
                            visible: modRow.description.length > 0
                            leftPadding: Theme.space.xl + Theme.space.xs
                            text: modRow.description
                            role: "caption"
                            tone: "tertiary"
                            wrapMode: Text.Wrap
                            maximumLineCount: 2
                            elide: Text.ElideRight
                        }
                    }
                    Rectangle {
                        visible: modRow.index < modsList.count - 1
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: 1
                        color: Theme.palette.divider
                    }
                }
            }
            Row {
                x: -recommendedButton.leftPadding
                spacing: Theme.space.sm
                Button {
                    id: recommendedButton
                    flat: true
                    text: qsTr("Recommended")
                    onClicked: modsList.model.selectRecommended()
                }
                Button {
                    flat: true
                    text: qsTr("None")
                    onClicked: modsList.model.clearAll()
                }
            }
        }

        Column {
            Layout.fillWidth: true
            visible: root.kind === "update"
            spacing: Theme.space.sm
            Row {
                spacing: Theme.space.sm
                Copy {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.request && root.request.updateInfo ? root.request.updateInfo.currentVersion || "" : ""
                    tone: "secondary"
                }
                MeshIcon {
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "chevron-right"
                    size: Theme.icon.sm
                    color: Theme.palette.textTertiary
                }
                Copy {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.request && root.request.updateInfo ? root.request.updateInfo.availableVersion || "" : ""
                    role: "bodyStrong"
                }
            }
            ScrollView {
                id: notesView
                width: parent.width
                height: Math.min(260, notes.implicitHeight + Theme.space.md)
                contentWidth: availableWidth
                visible: notes.source.length > 0
                // RichBody's article (QmlUiRequest::setUpdateInfo): no raw HTML, links are http(s) or mail only.
                RemoteRichText {
                    id: notes
                    width: notesView.availableWidth
                    format: "html"
                    article: true
                    source: root.updateInfo.releaseNotes || ""
                    pixelSize: Theme.type.label.pixelSize
                    onLinkActivated: (link) => ExternalLinks.open(notes, link)
                }
            }
            // Quiet, under the notes: not one of the answers the footer offers. Its label lines up with the text.
            Button {
                objectName: "updateSkipButton"
                x: -leftPadding
                flat: true
                buttonSize: "sm"
                text: qsTr("Skip this version")
                onClicked: root.answer(() => root.request.answerUpdate("skip"))
            }
        }

        Copy {
            Layout.fillWidth: true
            visible: text.length > 0
            text: root.primaryReason
            role: "caption"
            tone: "tertiary"
            wrapMode: Text.Wrap
        }
    }

    footer: Item {
        implicitWidth: footerRow.implicitWidth
        implicitHeight: footerRow.implicitHeight

    Row {
        id: footerRow
        anchors.right: parent.right
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.xl
        topPadding: 0

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

        PlayButton {
            visible: root.kind === "pickAccount"
            round: false
            size: Theme.control.height
            onClicked: accountPicker.play()
        }

        Button {
            objectName: "updateNowButton"
            visible: root.kind === "update" && root.updateInstallNow
            highlighted: true
            text: qsTr("Restart and update")
            onClicked: root.answer(() => root.request.answerUpdate("install"))
        }
        Button {
            objectName: "updateOnExitButton"
            visible: root.kind === "update" && !root.updateManaged
            highlighted: !root.updateInstallNow
            text: qsTr("Update when I close Kite")
            onClicked: root.answer(() => root.request.answerUpdate("installOnExit"))
        }
        Button {
            objectName: "updateOkButton"
            visible: root.kind === "update" && root.updateManaged
            highlighted: true
            text: qsTr("OK")
            onClicked: root.answer(() => root.request.answerUpdate("later"))
        }

        // profileSetup: Create may fail without answering the request (taken name, server error),
        // so it bypasses root.answer(), which would mark it answered; only Cancel ends the request.
        Button {
            visible: root.kind === "profileSetup"
            highlighted: true
            enabled: root.request && root.request.profileNameStatus === "available" && !root.request.profileSubmitting
            text: root.request && root.request.profileSubmitting ? qsTr("Creating…") : qsTr("Create")
            onClicked: root.request.submitProfileName(profileNameField.text.trim())
        }
        Button {
            visible: root.kind === "profileSetup"
            enabled: !(root.request && root.request.profileSubmitting)
            text: qsTr("Cancel")
            onClicked: root.answer(() => root.request.reject())
        }

        Button {
            visible: root.kind !== "choose" && root.kind !== "update" && root.kind !== "profileSetup"
                     && root.kind !== "filePicker" && root.kind !== "pickAccount" && root.kind !== "modal"
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
            text: root.request && root.request.rejectLabel.length > 0 ? root.request.rejectLabel : qsTr("Cancel")
            onClicked: root.answer(() => root.request.reject())
        }
    }

    // kind == "update": the quiet answer, at the other end from the two that install.
    Button {
        objectName: "updateLaterButton"
        visible: root.kind === "update" && !root.updateManaged
        anchors.left: parent.left
        // Its label on the content's edge, as "Skip this version" above it.
        anchors.leftMargin: Theme.space.xl - leftPadding
        y: footerRow.topPadding
        flat: true
        text: qsTr("Later")
        onClicked: root.answer(() => root.request.answerUpdate("later"))
    }
    }
    }

    // filePicker: a plugin's open/save dialog, shown natively. Plain accept()/reject(), as "text"
    // does, so QmlUiHost needs no filePicker invokables.
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
                // Set before every open(), imperatively: FileDialog owns selectedFile once shown,
                // so a binding would apply only once.
                nativeFileDialog.selectedFile =
                    req.filePickerMode === "save"
                        ? Format.fileUrl(req.filePickerDefaultPath) : ""
                nativeFileDialog.open()
            }
        }
    }
}
