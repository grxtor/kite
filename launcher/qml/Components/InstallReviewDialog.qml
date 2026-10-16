// SPDX-FileCopyrightText: 2026 Project Tick
// SPDX-FileContributor: Project Tick
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MeshMC.Theme

/*
 * What an install brings along, before anything is downloaded -- the QML
 * replacement for DownloadSummaryDialog. Two shapes, from the review's
 * own primaryCount:
 *   - A picked project (Add content, "Change version…"): the project is
 *     always installed, so its row has no checkbox; what it needs is
 *     listed under it, each one optional (one already installed at
 *     another version starts unticked, as it did there).
 *   - Missing dependencies ("Verify dependencies"): only what the
 *     installed mods need and do not have, all optional.
 * Dependencies no provider had a compatible version of are named
 * underneath, since things may not work without them.
 *
 * As tall as its rows, up to a limit; past that the list scrolls.
 */
Dialog {
    id: root

    // ContentInstallReview, already `ready`.
    property var review: null
    // Started with this review's TaskWatcher once Install is pressed.
    signal installStarted(var watcher)

    property bool started: false
    readonly property bool picked: !!root.review && root.review.primaryCount > 0
    readonly property int preferredWidth: 560
    readonly property int maxHeight: 560

    function show(review) {
        root.review = review
        root.started = false
        root.open()
    }

    // The "All dependencies" box: ticked, unticked, or some of them.
    function allState() {
        if (!root.review || root.review.checkedDependencyCount === 0)
            return Qt.Unchecked
        return root.review.checkedDependencyCount === root.review.dependencyCount ? Qt.Checked : Qt.PartiallyChecked
    }

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(root.preferredWidth, parent ? parent.width - Theme.space.xxl * 2 : root.preferredWidth)
    height: Math.min(root.maxHeight, implicitHeight, parent ? parent.height - Theme.space.xxl * 2 : root.maxHeight)
    modal: true
    closePolicy: Popup.CloseOnEscape
    title: !root.review ? "" : root.picked ? qsTr("Install %1").arg(root.review.title) : root.review.title

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
            text: !root.review ? ""
                  : !root.picked ? qsTr("Mods in this instance need these, and they are not installed.")
                  : root.review.dependencyCount > 0 ? qsTr("It needs these to run. Untick any you do not want.")
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
            Layout.preferredHeight: contentHeight
            clip: true
            spacing: Theme.space.xs
            boundsBehavior: Flickable.StopAtBounds
            model: root.review
            ScrollBar.vertical: ScrollBar {}

            // "Needed to run it" over the dependencies, under the project.
            section.property: "isDependency"
            // ListView keeps section items visible itself, so the one over
            // the picked project collapses to nothing instead.
            section.delegate: Item {
                required property string section
                readonly property bool shown: root.picked && section === "true"
                width: list.width
                height: shown ? sectionLabel.implicitHeight + Theme.space.sm : 0
                Text {
                    id: sectionLabel
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: Theme.space.xxs
                    text: parent.shown ? qsTr("Needed to run it") : ""
                    color: Theme.palette.textTertiary
                    font.family: Theme.font.family
                    font.pixelSize: Theme.type.caption.pixelSize
                    font.weight: Font.DemiBold
                }
            }

            delegate: ReviewRow {
                required property int index
                required property string name
                required property string fileName
                required property string provider
                required property string requiredBy
                required property bool isDependency
                required property bool maybeInstalled
                required property bool browserOnly
                required property string versionType
                required property var model

                width: list.width
                checkable: isDependency
                checked: model.checked
                title: name
                subtitle: maybeInstalled ? qsTr("Another version is already installed")
                        : browserOnly ? qsTr("The author only allows downloads from their own page")
                        : isDependency && requiredBy.length > 0 ? qsTr("Needed by %1").arg(requiredBy)
                        : fileName
                subtitleWarning: maybeInstalled || browserOnly
                tags: (versionType === "beta" ? [qsTr("Beta")] : versionType === "alpha" ? [qsTr("Alpha")] : [])
                      .concat(provider.length > 0 ? [provider] : [])
                onToggled: (on) => root.review.setChecked(index, on)
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
                // Box edge on the content edge.
                leftPadding: 0
                visible: !!root.review && root.review.dependencyCount > 1
                text: qsTr("All dependencies")
                checkState: root.allState()
                onClicked: {
                    root.review.setAllDependenciesChecked(root.review.checkedDependencyCount !== root.review.dependencyCount)
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
                enabled: !!root.review && root.review.ready && root.review.checkedCount > 0
                text: !root.review ? qsTr("Install")
                      : root.review.checkedCount > 1 ? qsTr("Install %1 files").arg(root.review.checkedCount)
                      : qsTr("Install")
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
}
