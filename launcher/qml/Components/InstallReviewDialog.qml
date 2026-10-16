// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * What an install from Add content brings along, before anything is
 * downloaded -- the QML replacement for DownloadSummaryDialog. The picked
 * project is always installed; each dependency it needs can be unticked
 * (one already installed at another version starts unticked, as it did
 * there). Dependencies no provider had a compatible version of are named
 * underneath, since the install may not work without them.
 *
 * Shown by ContentBrowserView only when there is something to review: a
 * project with no dependencies installs straight away.
 */
Dialog {
    id: root

    // ContentInstallReview (ContentBrowser.review()), already `ready`.
    property var review: null
    // Started with this review's TaskWatcher once Install is pressed.
    signal installStarted(var watcher)

    property bool started: false

    function show(review) {
        root.review = review
        root.started = false
        root.open()
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent ? parent.width - Theme.space.xxl * 2 : 560)
    height: Math.min(560, parent ? parent.height - Theme.space.xxl * 2 : 560)
    modal: true
    closePolicy: Popup.CloseOnEscape
    title: root.review ? qsTr("Install %1").arg(root.review.title) : ""

    header: DialogHeader {
        title: root.title
        icon: "download"
    }

    // Escape, Cancel or anything else that closes this without installing
    // ends the review, so its resolver does not linger.
    onClosed: {
        if (root.review && !root.started)
            root.review.cancel()
        root.review = null
    }

    contentItem: ColumnLayout {
        spacing: Theme.space.md

        Text {
            Layout.fillWidth: true
            text: root.review && root.review.dependencyCount > 0
                  ? qsTr("It needs these to run. Untick any you do not want.")
                  : qsTr("Nothing else needs to come with it.")
            wrapMode: Text.Wrap
            color: Theme.palette.textSecondary
            font.family: Theme.font.family
            font.pixelSize: Theme.type.label.pixelSize
            lineHeight: Theme.type.label.lineHeight
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: Theme.space.xs
            boundsBehavior: Flickable.StopAtBounds
            model: root.review
            ScrollBar.vertical: ScrollBar {}

            delegate: Rectangle {
                id: row
                required property int index
                required property string name
                required property string fileName
                required property string provider
                required property string requiredBy
                required property bool isDependency
                required property bool checked
                required property bool maybeInstalled
                required property bool browserOnly
                required property string versionType

                width: list.width
                height: Theme.control.heightLg + Theme.space.sm
                radius: Theme.radius.sm
                color: Theme.palette.surfaceSunken
                border.width: 1
                border.color: Theme.palette.border

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space.sm
                    anchors.rightMargin: Theme.space.md
                    spacing: Theme.space.sm

                    // The picked project cannot be unticked: it is what
                    // this install is for.
                    CheckBox {
                        enabled: row.isDependency
                        checked: row.checked
                        Accessible.name: qsTr("Install %1").arg(row.name)
                        onToggled: {
                            root.review.setChecked(row.index, checked)
                            checked = Qt.binding(() => row.checked)
                        }
                    }
                    Column {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 1
                        Text {
                            width: parent.width
                            text: row.name
                            elide: Text.ElideRight
                            color: Theme.palette.textPrimary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.body.pixelSize
                            font.weight: Font.Medium
                        }
                        Text {
                            width: parent.width
                            text: row.maybeInstalled ? qsTr("Another version is already installed")
                                : row.browserOnly ? qsTr("The author only allows downloads from their own page")
                                : row.isDependency && row.requiredBy.length > 0 ? qsTr("Needed by %1").arg(row.requiredBy)
                                : row.fileName
                            elide: Text.ElideRight
                            color: row.maybeInstalled || row.browserOnly ? Theme.palette.warning : Theme.palette.textTertiary
                            font.family: Theme.font.family
                            font.pixelSize: Theme.type.caption.pixelSize
                        }
                    }
                    Tag {
                        visible: row.versionType.length > 0 && row.versionType !== "release"
                        text: row.versionType === "beta" ? qsTr("Beta")
                            : row.versionType === "alpha" ? qsTr("Alpha") : row.versionType
                    }
                    Tag {
                        visible: row.provider.length > 0
                        text: row.provider
                    }
                }
            }
        }

        NoticeBar {
            Layout.fillWidth: true
            visible: !!root.review && root.review.unresolved.length > 0
            text: root.review
                  ? qsTr("No compatible version was found for %1. It may not work without it.")
                        .arg(root.review.unresolved.join(", "))
                  : ""
        }
    }

    footer: Item {
        implicitHeight: footerRow.implicitHeight + Theme.space.lg

        RowLayout {
            id: footerRow
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: Theme.space.lg
            anchors.rightMargin: Theme.space.lg
            spacing: Theme.space.sm

            CheckBox {
                visible: !!root.review && root.review.dependencyCount > 1
                text: qsTr("All dependencies")
                checkState: root.allState()
                onClicked: {
                    root.review.setAllDependenciesChecked(root.review.checkedCount !== root.review.count)
                    checkState = Qt.binding(() => root.allState())
                }
            }
            Item { Layout.fillWidth: true }
            Button {
                flat: true
                text: qsTr("Cancel")
                onClicked: root.close()
            }
            Button {
                highlighted: true
                enabled: !!root.review && root.review.ready
                text: root.review && root.review.checkedCount > 1
                      ? qsTr("Install %1 files").arg(root.review.checkedCount) : qsTr("Install")
                icon.source: Icons.url("download")
                onClicked: {
                    const watcher = root.review.install()
                    root.started = !!watcher
                    root.close()
                    if (watcher)
                        root.installStarted(watcher)
                }
            }
        }
    }

    // The "All dependencies" box: ticked, unticked, or some of them.
    function allState() {
        if (!root.review || root.review.checkedCount === 1)
            return Qt.Unchecked
        return root.review.checkedCount === root.review.count ? Qt.Checked : Qt.PartiallyChecked
    }
}
