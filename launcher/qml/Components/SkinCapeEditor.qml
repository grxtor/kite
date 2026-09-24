// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MeshMC.Theme

/*
 * Skin and cape editor for one Microsoft account, opened from AccountsPage.
 * Everything here calls back into AccountsController -- changeSkin()/
 * resetSkin()/changeCape()/accountSkinInfo()/validateSkinFile() -- which
 * drives the same three services (SkinUpload/SkinDelete/CapeChange) the
 * classic SkinManageDialog used, minus its local skin library: this picks a
 * file and uploads it directly (with whichever arm width is currently
 * selected) rather than keeping a gallery of skins to switch between.
 *
 * One instance is shared by the whole page (see AccountsPage.qml); `row` is
 * set right before open() for whichever account was clicked.
 */
Dialog {
    id: root
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 520
    modal: true
    title: root.accountName.length > 0 ? qsTr("Skin & cape — %1").arg(root.accountName)
                                        : qsTr("Skin & cape")

    // AccountsController: accountSkinInfo, validateSkinFile, changeSkin,
    // resetSkin, changeCape.
    property var controller: null
    property int row: -1
    property string accountId: ""
    property string accountName: ""
    // Bumped by AccountsPage whenever an account's data may have changed,
    // so the body image's url changes and QML actually refetches it -- see
    // AccountsPage.qml's imageRevision.
    property int rev: 0

    // Snapshot from controller.accountSkinInfo(row) -- re-read on open and
    // whenever the account list reports a change (a refresh landing).
    property var info: emptyInfo()
    function emptyInfo() {
        return { valid: false, slim: false, currentCapeId: "", capes: [] }
    }
    function reload() {
        // Not gated on row >= 0: row -1 is the qml-preview-tools demo
        // sentinel (see AccountsController::skinDemoRequested()), which
        // accountSkinInfo() already reports as "invalid" the rest of the
        // time, so nothing else here needs to know the sentinel exists.
        root.info = root.controller
                    ? root.controller.accountSkinInfo(root.row) : root.emptyInfo()
    }

    // The arm width a picked file uploads with. A plain property, not a
    // binding to info.slim: it has to survive info being re-read (a
    // response to something unrelated changing elsewhere) without
    // clobbering a choice the user just made but has not uploaded yet. Set
    // from info.slim once, on open, same as any other field here.
    property bool slim: false

    // Whichever change is running, if any -- shared by upload/reset/cape so
    // only one can run at a time and its status/error show the same way.
    property var watcher: null
    readonly property bool busy: !!root.watcher && root.watcher.running
    property string pickError: ""
    // Same per-account fallback tint as AccountsPage's own hero stage
    // (design-plan.md §5/§9) -- this stage duplicates that idiom
    // independently, so it needs the same fix, not just the hero card.
    readonly property color stageTint: Format.hashTint(root.accountId)

    onOpened: {
        reload()
        root.slim = root.info.slim
        pickError = ""
        watcher = null
    }

    Connections {
        target: root.controller ? root.controller.accounts : null
        function onDataChanged() { root.reload() }
    }

    function runWatcher(w) {
        if (!w)
            return
        root.watcher = w
        w.finished.connect(function() { root.reload() })
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.lg

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.space.lg

            // The stage: a floor shadow and either the account's real
            // skin body or a neutral, per-account-tinted silhouette
            // standing on it -- the same idiom AccountsPage's own hero card
            // uses, scaled down to fit here (design-plan.md §5/§9).
            Item {
                id: stage
                Layout.preferredWidth: 132
                Layout.preferredHeight: 208
                Layout.alignment: Qt.AlignTop

                // Soft floor shadow the figure appears to stand on, matching
                // AccountsPage's hero exactly -- a flattened pill, not a
                // glow behind the figure (design-plan.md "no glows").
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    width: 96; height: 16
                    radius: height / 2
                    color: Format.shade(root.stageTint, Theme.dark ? 0.45 : 0.55, 0.6)
                    opacity: 0.30
                }

                SkinSilhouette {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 10
                    unit: 0.87
                    opacity: 0.65
                    color: Format.shade(root.stageTint, Theme.dark ? 0.62 : 0.42, 0.5)
                }

                Image {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    width: 76
                    height: 152
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                    source: root.accountId.length > 0 ? "image://accountface/body/" + root.accountId + "?rev=" + root.rev : ""
                    sourceSize: Qt.size(152, 304)
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Theme.space.md

                Text {
                    text: qsTr("Arm width")
                    color: Theme.palette.textSecondary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.label.pixelSize
                }
                SegmentedControl {
                    options: [
                        { value: "classic", label: qsTr("Classic") },
                        { value: "slim", label: qsTr("Slim") }
                    ]
                    current: root.slim ? "slim" : "classic"
                    onActivated: (value) => root.slim = value === "slim"
                }

                RowLayout {
                    Layout.topMargin: Theme.space.xs
                    spacing: Theme.space.sm
                    Button {
                        highlighted: true
                        text: qsTr("Change skin…")
                        enabled: !root.busy
                        onClicked: skinFileDialog.open()
                    }
                    Button {
                        flat: true
                        text: qsTr("Reset skin")
                        enabled: !root.busy
                        onClicked: root.runWatcher(root.controller.resetSkin(root.row))
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.busy || root.pickError.length > 0 || (!!root.watcher && root.watcher.failed)
                    spacing: Theme.space.sm

                    BusyIndicator {
                        visible: root.busy
                        running: visible
                        width: 20; height: 20
                    }
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        color: (root.pickError.length > 0 || (!!root.watcher && root.watcher.failed))
                               ? Theme.palette.danger : Theme.palette.textTertiary
                        text: root.pickError.length > 0 ? root.pickError
                            : root.busy ? (root.watcher.status.length > 0 ? root.watcher.status : qsTr("Working…"))
                            : (root.watcher ? root.watcher.error : "")
                        font.family: Theme.font.family
                        font.pixelSize: Theme.type.label.pixelSize
                    }
                }

                // Fills the space below a short status line so the actions
                // above stay top-aligned with the stage instead of centring
                // in whatever height the capes row below ends up needing.
                Item { Layout.fillHeight: true }
            }
        }

        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.palette.divider }

        Text {
            text: qsTr("Capes")
            color: Theme.palette.textPrimary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.title.pixelSize
            font.weight: Font.Bold
        }

        Flow {
            Layout.fillWidth: true
            spacing: Theme.space.sm

            CapeTile {
                label: qsTr("No cape")
                isNoCapeOption: true
                selected: root.info.currentCapeId.length === 0
                enabled: !root.busy
                onClicked: root.runWatcher(root.controller.changeCape(root.row, ""))
            }
            Repeater {
                model: root.info.capes
                delegate: CapeTile {
                    required property var modelData
                    label: modelData.alias
                    capeUrl: modelData.url
                    selected: modelData.id === root.info.currentCapeId
                    enabled: !root.busy
                    onClicked: root.runWatcher(root.controller.changeCape(root.row, modelData.id))
                }
            }
        }
    }

    footer: Row {
        layoutDirection: Qt.RightToLeft
        spacing: Theme.space.sm
        padding: Theme.space.lg
        topPadding: 0
        Button {
            text: qsTr("Close")
            onClicked: root.close()
        }
    }

    FileDialog {
        id: skinFileDialog
        title: qsTr("Select skin texture")
        nameFilters: [qsTr("PNG images (*.png)")]
        onAccepted: {
            const path = selectedFile.toString()
            const error = root.controller ? root.controller.validateSkinFile(path) : ""
            if (error.length > 0) {
                root.pickError = error
            } else {
                root.pickError = ""
                root.runWatcher(root.controller.changeSkin(root.row, path, root.slim))
            }
        }
    }
}
