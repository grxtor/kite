// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Kite.Theme

/*
 * "Send this crash to Kite so we can find the fix": under a crash Kite could not explain, or one its
 * fix did not help, in the crash window and the Log tab's details. Nothing goes unless Send is
 * pressed, or the player allowed sending without asking (then it says it was sent). "What is sent"
 * folds open the exact report (CrashPayloadView). The box under the button turns on sending without
 * asking, only once Send is pressed; Settings > Log upload turns it off. No surface of its own.
 */
ColumnLayout {
    id: root

    // CrashDoctorModel, or null.
    property var doctor: null
    property bool payloadShown: false
    // Sent or on its way: the block says so instead of asking.
    readonly property string sendState: root.doctor ? root.doctor.sendState : ""
    readonly property bool asking: !!root.doctor && root.doctor.canSend
    readonly property bool shown: root.asking || root.sendState.length > 0

    visible: root.shown
    spacing: Theme.space.sm

    RowLayout {
        Layout.fillWidth: true
        spacing: Theme.space.md

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: Theme.space.xs

            Copy {
                Layout.fillWidth: true
                text: qsTr("Send this crash to Kite so we can find the fix")
                role: "bodyStrong"
                wrapMode: Text.Wrap
            }
            Copy {
                Layout.fillWidth: true
                text: root.statusText()
                tone: root.sendState === "failed" ? "danger" : "secondary"
                wrapMode: Text.Wrap
            }
        }

        Button {
            Layout.alignment: Qt.AlignTop
            visible: root.asking || root.sendState === "sending"
            enabled: root.asking
            highlighted: true
            text: root.sendState === "sending" ? qsTr("Sending…") : qsTr("Send")
            onClicked: root.doctor.send(alwaysBox.checked)
        }
    }

    CheckBox {
        id: alwaysBox
        visible: root.asking
        text: qsTr("From now on, send without asking")
    }

    // The same words open or closed; the chevron says which.
    AbstractButton {
        id: toggle
        hoverEnabled: true
        implicitHeight: Theme.control.heightSm
        implicitWidth: toggleRow.implicitWidth
        onClicked: root.payloadShown = !root.payloadShown
        contentItem: Item {}
        Accessible.role: Accessible.Button
        Accessible.name: toggleText.text

        Row {
            id: toggleRow
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space.xs
            MeshIcon {
                anchors.verticalCenter: parent.verticalCenter
                iconName: root.payloadShown ? "chevron-down" : "chevron-right"
                size: Theme.icon.xs
                color: toggle.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
            }
            Copy {
                id: toggleText
                anchors.verticalCenter: parent.verticalCenter
                text: root.asking ? qsTr("What is sent") : qsTr("What was sent")
                role: "label"
                color: toggle.hovered ? Theme.palette.textPrimary : Theme.palette.textSecondary
            }
        }
    }

    CrashPayloadView {
        Layout.fillWidth: true
        Layout.leftMargin: Theme.icon.xs + Theme.space.xs
        visible: root.payloadShown
        payload: root.doctor ? root.doctor.payload : ({})
    }

    LinkButton {
        visible: !!root.doctor && root.doctor.autoSent
        text: qsTr("Ask me before sending")
        onClicked: SettingsStore.setValue("CrashReportsAutoSend", false)
    }

    function statusText() {
        if (!root.doctor)
            return ""
        switch (root.sendState) {
        case "sending":
            return qsTr("Sending…")
        case "queued":
            return qsTr("There seems to be no connection right now. Kite sends it once there is.")
        case "sent":
            return root.doctor.autoSent
                   ? qsTr("Kite sent it by itself, as you allowed. When a fix is found, Kite tells you.")
                   : qsTr("Sent, thank you. When a fix is found, Kite tells you.")
        case "failed":
            return qsTr("Kite's server did not take it.")
        }
        if (root.doctor.fixDidNotHelp)
            return qsTr("The fix Kite offered doesn't seem to have helped. Sending the crash shows us where the rule is wrong; your account, your name and your IP address stay here.")
        // Vanilla sends no mod list, so it does not say it does.
        return (root.doctor.payload.modCount || 0) > 0
               ? qsTr("Kite doesn't know this crash yet. Sending it shows us the error lines, the versions and the mod list, so the fix can come for everyone; your account, your name and your IP address stay here.")
               : qsTr("Kite doesn't know this crash yet. Sending it shows us the error lines and the versions, so the fix can come for everyone; your account, your name and your IP address stay here.")
    }
}
