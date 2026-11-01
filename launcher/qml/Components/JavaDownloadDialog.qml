// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * Download a Java runtime: pick a vendor, a Java version and a build for this
 * computer, then watch it download. The work is JavaDownloadController's. The
 * runtime lands in Kite's own Java folder.
 */
AppDialog {
    id: root

    readonly property var download: JavaStore.download
    readonly property bool busy: !!download && download.downloading
    readonly property bool done: !!download && !download.downloading && download.installedPath.length > 0

    signal useRequested(string path)

    title: qsTr("Download Java")
    primaryText: root.done ? qsTr("Use this Java")
                           : !!root.download && root.download.alreadyInstalled ? qsTr("Download again") : qsTr("Download")
    primaryEnabled: root.done || (!!root.download && root.download.build >= 0 && !root.busy && !root.download.loading)
    cancelText: root.done ? qsTr("Close") : qsTr("Cancel")
    // The download goes on after the click; the dialog closes when it is done or cancelled.
    closeOnPrimary: false
    closePolicy: root.busy ? Popup.NoAutoClose : Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onPrimaryClicked: {
        if (!root.done) {
            root.download.download()
            return
        }
        root.useRequested(root.download.installedPath)
        root.close()
    }
    onOpened: if (root.download) root.download.reset()
    onClosed: if (root.download) root.download.cancel()

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Copy {
            Layout.fillWidth: true
            text: qsTr("Kite keeps downloaded Java in its own folder, apart from anything you installed yourself.")
            wrapMode: Text.Wrap
            tone: "tertiary"
            role: "label"
            lineHeight: spec.lineHeight
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Theme.space.lg
            rowSpacing: Theme.space.md
            enabled: !root.busy

            FieldLabel { Layout.alignment: Qt.AlignVCenter; text: qsTr("Vendor") }
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

            FieldLabel { Layout.alignment: Qt.AlignVCenter; text: qsTr("Java version") }
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

            FieldLabel { Layout.alignment: Qt.AlignVCenter; text: qsTr("Build") }
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

        Copy {
            Layout.fillWidth: true
            visible: !!root.download && root.download.alreadyInstalled && !root.busy && !root.done
            text: qsTr("This build is already downloaded. Downloading again replaces it.")
            wrapMode: Text.Wrap
            tone: "secondary"
            role: "label"
        }

        Copy {
            Layout.fillWidth: true
            visible: !!root.download && root.download.error.length > 0
            text: root.download ? root.download.error : ""
            wrapMode: Text.Wrap
            tone: "danger"
            role: "label"
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
            Copy {
                Layout.fillWidth: true
                text: root.download ? root.download.status : ""
                elide: Text.ElideRight
                tone: "tertiary"
                role: "caption"
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
            Copy {
                Layout.fillWidth: true
                text: qsTr("Java is downloaded and ready.")
            }
        }
    }
}
