// SPDX-FileCopyrightText: 2026 grxtor
// SPDX-FileContributor: grxtor
// SPDX-License-Identifier: Apache-2.0

import QtQuick
import QtQuick.Layouts
import Kite.Theme

/*
 * The top of a modpack's page (ModpackDetail): logo, name, summary, facts and categories. With a gallery picture
 * (`cover`) it sits on that picture as a banner, exactly like an instance's header (InstanceBanner, its scrim and the
 * light-over-photo text); without one it is the plain row.
 */
Item {
    id: root

    property var pack: ({})
    // The gallery's featured picture, or its first; "" for none.
    property string cover: ""

    readonly property int logoSize: 96
    readonly property int bannerHeight: 208
    readonly property bool overPhoto: banner.shown
    readonly property color ink: overPhoto ? Theme.media.text : Theme.palette.textPrimary
    readonly property color inkSecondary: overPhoto ? Theme.media.textSecondary : Theme.palette.textSecondary
    readonly property color inkFaint: overPhoto ? Theme.media.textFaint : Theme.palette.textTertiary
    readonly property string updatedText: Format.ageText(root.pack.updated || "")

    implicitHeight: root.overPhoto ? Math.max(root.bannerHeight, row.implicitHeight + Theme.space.xl + banner.fadeHeight)
                                 : row.implicitHeight

    InstanceBanner {
        id: banner
        anchors.fill: parent
        source: root.cover
        kind: "custom"
    }

    RowLayout {
        id: row
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.overPhoto ? Theme.space.xl : 0
        anchors.rightMargin: root.overPhoto ? Theme.space.xl : 0
        // Over a cover the row sits on the banner's lower edge, clear of its fade into the page.
        y: root.overPhoto ? root.height - banner.fadeHeight - height : 0
        spacing: Theme.space.xl

        Item {
            Layout.preferredWidth: root.logoSize
            Layout.preferredHeight: root.logoSize
            Layout.alignment: Qt.AlignBottom

            Rectangle {
                anchors.fill: parent
                radius: Theme.radius.xl
                color: Theme.palette.surface
            }
            Image {
                id: logoImg
                anchors.fill: parent
                source: root.pack.logoUrl || ""
                sourceSize: Qt.size(width * 2, height * 2)
                fillMode: Image.PreserveAspectCrop
                smooth: true
                mipmap: true
                asynchronous: true
                visible: status === Image.Ready
            }
            MeshIcon {
                anchors.centerIn: parent
                visible: logoImg.status !== Image.Ready
                iconName: "package"
                size: Theme.icon.lg
                color: Theme.palette.textTertiary
            }
            // On a photo there is no one colour behind the corners, so the logo stays square-cornered under a
            // hairline; on the page it is rounded like every other logo.
            CornerMatte {
                visible: !root.overPhoto
                cornerRadius: Theme.radius.xl
                matte: Theme.palette.canvas
                outline: Theme.palette.border
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.alignment: Qt.AlignBottom
            spacing: Theme.space.sm

            Copy {
                Layout.fillWidth: true
                text: root.pack.title || ""
                elide: Text.ElideRight
                role: "display"
                color: root.ink
            }
            Copy {
                Layout.fillWidth: true
                visible: text.length > 0
                text: root.pack.description || ""
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
                color: root.inkSecondary
                lineHeight: Theme.type.body.lineHeight
            }

            Flow {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space.xxs
                spacing: Theme.space.lg

                Repeater {
                    model: [
                        { icon: "user", text: root.pack.author ? qsTr("by %1").arg(root.pack.author) : "" },
                        { icon: "download", text: (root.pack.downloads || 0) > 0
                              ? qsTr("%1 downloads").arg(Format.compactNumber(root.pack.downloads)) : "" },
                        { icon: "clock", text: root.updatedText.length > 0 ? qsTr("Updated %1").arg(root.updatedText) : "" }
                    ].filter((fact) => fact.text.length > 0)
                    delegate: Row {
                        required property var modelData
                        spacing: Theme.space.xs
                        MeshIcon {
                            anchors.verticalCenter: parent.verticalCenter
                            iconName: modelData.icon
                            size: Theme.icon.sm
                            color: root.inkFaint
                        }
                        Copy {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.text
                            role: "label"
                            color: root.inkSecondary
                            font.weight: Font.Medium
                        }
                    }
                }
            }

            Flow {
                Layout.fillWidth: true
                spacing: Theme.space.xs
                visible: (root.pack.categories || []).length > 0
                Repeater {
                    model: root.pack.categories || []
                    delegate: Tag {
                        required property var modelData
                        onMedia: root.overPhoto
                        text: Format.titleCase(String(modelData))
                    }
                }
            }
        }
    }
}
