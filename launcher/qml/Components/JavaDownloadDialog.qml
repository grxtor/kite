// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * Download a Java runtime: pick a vendor, a Java version and a build for
 * this computer, then watch it download. The QML replacement for the
 * widget JavaDownloadDialog; the work is JavaDownloadController's. The
 * runtime lands in MeshMC's own Java folder, so it shows up under
 * "Downloaded Java" and in the Java picker afterwards.
 */
Dialog {
    id: root

    readonly property var download: JavaStore.download
    readonly property bool busy: !!download && download.downloading
    readonly property bool done: !!download && !download.downloading && download.installedPath.length > 0

    // Use the freshly installed Java; the caller writes the setting.
    signal useRequested(string path)

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(520, parent ? parent.width - Theme.space.xxl * 2 : 520)
    modal: true
    closePolicy: root.busy ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside
    title: qsTr("Download Java")

    header: DialogHeader {
        title: root.title
        icon: "download"
    }

    onOpened: if (root.download) root.download.reset()
    onClosed: if (root.download) root.download.cancel()

    component FieldLabel: Text {
        Layout.alignment: Qt.AlignVCenter
        color: Theme.palette.textSecondary
        font.family: Theme.font.family
        font.pixelSize: Theme.type.body.pixelSize
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Text {
            Layout.fillWidth: true
            text: qsTr("MeshMC keeps downloaded Java in its own folder, apart from anything you installed yourself.")
            wrapMode: Text.Wrap
            color: Theme.palette.textTertiary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            lineHeight: Theme.type.label.lineHeight
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Theme.space.lg
            rowSpacing: Theme.space.md
            enabled: !root.busy

            FieldLabel { text: qsTr("Vendor") }
            ComboBox {
                Layout.fillWidth: true
                model: root.download ? root.download.vendors : []
                textRole: "label"
                valueRole: "value"
                Accessible.name: qsTr("Vendor")
                // Selected by value, and again whenever the list or the
                // choice changes (see JavaVendorRow.qml for why not a binding).
                readonly property string chosen: root.download ? root.download.vendor : ""
                function sync() { currentIndex = count > 0 ? indexOfValue(chosen) : -1 }
                onChosenChanged: sync()
                onCountChanged: sync()
                Component.onCompleted: sync()
                onActivated: {
                    root.download.selectVendor(currentValue)
                    sync()
                }
            }

            FieldLabel { text: qsTr("Java version") }
            ComboBox {
                Layout.fillWidth: true
                model: root.download ? root.download.versions : []
                textRole: "label"
                valueRole: "value"
                displayText: currentIndex >= 0 ? currentText : (root.download && root.download.loading ? qsTr("Loading…") : "")
                Accessible.name: qsTr("Java version")
                readonly property string chosen: root.download ? root.download.version : ""
                function sync() { currentIndex = count > 0 ? indexOfValue(chosen) : -1 }
                onChosenChanged: sync()
                onCountChanged: sync()
                Component.onCompleted: sync()
                onActivated: {
                    root.download.selectVersion(currentValue)
                    sync()
                }
            }

            FieldLabel { text: qsTr("Build") }
            ComboBox {
                Layout.fillWidth: true
                model: root.download ? root.download.builds : []
                textRole: "label"
                displayText: currentIndex >= 0 ? currentText : ""
                Accessible.name: qsTr("Build")
                readonly property int chosen: root.download ? root.download.build : -1
                function sync() { currentIndex = count > 0 ? chosen : -1 }
                onChosenChanged: sync()
                onCountChanged: sync()
                Component.onCompleted: sync()
                onActivated: {
                    root.download.selectBuild(currentIndex)
                    sync()
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: !!root.download && root.download.alreadyInstalled && !root.busy && !root.done
            text: qsTr("This build is already downloaded. Downloading again replaces it.")
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
        }

        Text {
            Layout.fillWidth: true
            visible: !!root.download && root.download.error.length > 0
            text: root.download ? root.download.error : ""
            wrapMode: Text.Wrap
            color: Theme.palette.danger
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
        }

        ColumnLayout {
            Layout.fillWidth: true
            visible: root.busy
            spacing: Theme.space.xs
            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: 1
                value: root.download ? root.download.progress : 0
                indeterminate: !!root.download && root.download.progress <= 0
            }
            Text {
                Layout.fillWidth: true
                text: root.download ? root.download.status : ""
                elide: Text.ElideRight
                color: Theme.palette.textTertiary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.caption.pixelSize
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: root.done
            spacing: Theme.space.sm
            MeshIcon {
                iconName: "check"
                size: Theme.icon.md
                color: Theme.palette.success
            }
            Text {
                Layout.fillWidth: true
                text: qsTr("Java is downloaded and ready.")
                color: Theme.palette.textPrimary
                font.family: Theme.font.family
                font.pixelSize: Theme.type.body.pixelSize
            }
        }
    }

    footer: RowLayout {
        spacing: Theme.space.sm
        Item { Layout.fillWidth: true }
        Button {
            Layout.bottomMargin: Theme.space.lg
            visible: !root.done
            text: qsTr("Cancel")
            flat: true
            onClicked: root.close()
        }
        Button {
            Layout.bottomMargin: Theme.space.lg
            Layout.rightMargin: Theme.space.lg
            visible: !root.done
            highlighted: true
            text: !!root.download && root.download.alreadyInstalled ? qsTr("Download again") : qsTr("Download")
            enabled: !!root.download && root.download.build >= 0 && !root.busy && !root.download.loading
            onClicked: root.download.download()
        }
        Button {
            Layout.bottomMargin: Theme.space.lg
            visible: root.done
            text: qsTr("Close")
            flat: true
            onClicked: root.close()
        }
        Button {
            Layout.bottomMargin: Theme.space.lg
            Layout.rightMargin: Theme.space.lg
            visible: root.done
            highlighted: true
            text: qsTr("Use this Java")
            onClicked: {
                root.useRequested(root.download.installedPath)
                root.close()
            }
        }
    }
}
