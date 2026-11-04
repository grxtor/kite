// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import Kite.Theme

/*
 * "Set up Kite" on Home's side panel: a checklist of what a new player still has to do. A done step is struck
 * through and quiet; a step still to do is a button. The page hides the card once every step is done.
 *
 * "Bring over instances from other launchers" is there only while the other launchers have instances to bring over
 * (the importer's scan, asked once per launch the first time the card is due) and the player has neither imported
 * nor hidden it; it leaves the list without a trace, as it was never a duty.
 */
Rectangle {
    id: root

    property bool hasInstances: false
    property bool signedIn: false
    // QmlShell.launcherImport, or null.
    property var importer: null
    // No fill or hairline, for a host that is already a panel.
    property bool flat: false

    signal signInRequested()
    signal importRequested()

    readonly property bool importHidden: SettingsStore.bool("SetupImportDone")
    readonly property bool importPending: !!root.importer && !root.importHidden && root.importer.availableCount > 0
    // The importer looks once per launch, and only for a card that can show the step.
    readonly property bool wantsScan: root.hasInstances && !!root.importer && !root.importHidden
                                      && !root.importer.scanned && !root.importer.scanning
    readonly property bool allDone: root.signedIn && !root.importPending
    // An empty library has the page's own "Create instance"; the checklist waits for a first instance.
    readonly property bool shown: root.hasInstances && !root.allDone

    // The step's ⋯ and an import both settle it for good (SetupImportDone).
    function settleImport() {
        SettingsStore.setValue("SetupImportDone", true)
    }

    // Later, not inside the change that made it due: the scan changes what wantsScan reads.
    function lookAtOtherLaunchers() {
        if (root.wantsScan)
            root.importer.scan()
    }

    onWantsScanChanged: Qt.callLater(root.lookAtOtherLaunchers)
    Component.onCompleted: Qt.callLater(root.lookAtOtherLaunchers)

    Connections {
        target: root.importer
        ignoreUnknownSignals: true
        function onFinished(imported, failed, cancelled) {
            if (imported > 0)
                root.settleImport()
        }
    }

    implicitHeight: column.implicitHeight + Theme.space.md * 2
    radius: Theme.radius.lg
    color: root.flat ? "transparent" : Theme.palette.surface
    border.width: root.flat ? 0 : 1
    border.color: Theme.palette.border

    component Step: AbstractButton {
        id: step
        property bool done: false
        // Shows a ⋯ with "Hide" at the end of the step.
        property bool hideable: false
        readonly property int indicatorSize: Theme.icon.md

        enabled: !step.done
        hoverEnabled: true
        implicitHeight: Theme.control.height
        signal hideRequested()

        leftPadding: Theme.space.md
        rightPadding: step.hideable ? Theme.space.xs : Theme.space.md
        Accessible.name: step.text
        Accessible.description: step.done ? qsTr("Done") : ""

        background: StateFill {
            hovered: step.hovered
            down: step.down
            radius: Theme.radius.md
            border.width: step.visualFocus ? Theme.focus.width : 0
            border.color: Theme.palette.focusRing
        }

        contentItem: Item {
            Rectangle {
                id: ring
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: step.indicatorSize
                height: width
                radius: width / 2
                color: step.done ? Theme.palette.textTertiary : "transparent"
                border.width: step.done ? 0 : 2
                border.color: Theme.palette.textSecondary

                MeshIcon {
                    anchors.centerIn: parent
                    visible: step.done
                    iconName: "check"
                    size: Theme.icon.xxs
                    color: Theme.palette.surface
                }
            }

            Copy {
                id: label
                anchors.left: ring.right
                anchors.leftMargin: Theme.space.sm
                anchors.right: moreButton.visible ? moreButton.left : parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: step.text
                tone: step.done ? "tertiary" : "primary"
                wrapMode: Text.WordWrap
                maximumLineCount: 3
                elide: Text.ElideRight
                font.strikeout: step.done
                font.weight: Font.Medium
            }

            IconButton {
                id: moreButton
                visible: step.hideable
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                size: Theme.control.heightSm
                iconName: "more"
                tip: qsTr("More")
                onClicked: hideMenu.popup(moreButton, moreButton.width - hideMenu.width, moreButton.height)

                Menu {
                    id: hideMenu
                    MenuItem {
                        text: qsTr("Hide")
                        icon.source: Icons.url("x")
                        onTriggered: step.hideRequested()
                    }
                }
            }
        }
    }

    Column {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Theme.space.md
        spacing: Theme.space.sm

        Copy {
            width: parent.width
            height: Theme.control.heightSm
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Set up Kite")
            role: "title"
            elide: Text.ElideRight
        }

        Step {
            width: parent.width
            text: qsTr("Sign in with Microsoft")
            done: root.signedIn
            onClicked: root.signInRequested()
        }

        Step {
            objectName: "importStep"
            width: parent.width
            visible: root.importPending
            hideable: true
            text: qsTr("Bring over instances from other launchers")
            onClicked: root.importRequested()
            onHideRequested: root.settleImport()
        }
    }
}
